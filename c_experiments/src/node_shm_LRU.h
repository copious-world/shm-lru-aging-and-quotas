#ifndef _H_HOPSCOTCH_HASH_LRU_
#define _H_HOPSCOTCH_HASH_LRU_
#pragma once

// node_shm_LRU.h

#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>

#include <map>
#include <unordered_map>
#include <list>
#include <chrono>
#include <atomic>


using namespace std;


#include "hmap_interface.h"
#include "node_shm_HH.h"

#include "holey_buffer.h"
#include "atomic_proc_rw_state.h"
#include "time_bucket.h"
#include "atomic_stack.h"



using namespace std::chrono;


#define MAX_BUCKET_FLUSH 12

#define ONE_HOUR 	(60*60*1000)


/**
 * The 64 bit key stores a 32bit hash (xxhash or other) in the lower word.
 * The top 32 bits stores a structured bit array. The top 1 bit will be
 * the selector of the hash region where the key match will be found. The
 * bottom 20 bits will store the element offset (an element number) to the position the
 * element data is stored. 
*/



inline uint64_t epoch_ms(void) {
	uint64_t ms;
	ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	return ms;
}



uint32_t g_prev_time = 0;
uint32_t g_interval_counter = 0;
//
inline uint32_t current_time_next() {
	//
	uint32_t probe_time = epoch_ms();
	if ( g_prev_time == probe_time ) {
		g_interval_counter++;
	} else {
		g_interval_counter = 0;
		g_prev_time = probe_time;
	}
	//
	uint32_t timemarker = (g_interval_counter & (~0x1111)) | (uint32_t)(g_prev_time << 4);
	return timemarker;
}

void init_diff_timer() {
	g_prev_time = (uint32_t)epoch_ms();
	g_interval_counter = 0;
}

const uint32_t MAX_MESSAGE_SIZE = 128;
//
const uint32_t OFFSET_TO_MARKER = 0;					// in bytes
const uint32_t OFFSET_TO_OFFSET = sizeof(uint32_t);		// 
const uint32_t OFFSET_TO_HASH = (OFFSET_TO_OFFSET + sizeof(uint32_t));   // start of 64bits

const uint32_t TOTAL_ATOMIC_OFFSET = (OFFSET_TO_HASH + sizeof(uint64_t));	
//
const uint32_t DEFAULT_MICRO_TIMEOUT = 2; // 2 seconds


// five fences (??)
typedef struct COM_ELEMENT {
	atomic<COM_BUFFER_STATE>	_marker;	// control over accessing the message buffer
	uint8_t						_proc;		// the index of the owning proc
	uint8_t						_tier;		// the index of the managed tier (this com element)
	uint8_t						_ops;		// read/write/delete and other flags. 
	//
	uint64_t					_hash;		// the hash of the message
	uint32_t					_offset;	// offset to the LRU element... where data will be stored
	uint32_t					_timestamp;	// control over accessing the message buffer
	char						_message[MAX_MESSAGE_SIZE];   // 64*2
} Com_element;


typedef union {
	uint32_t			_offset;
	Com_element			*_cel;
} com_or_offset;


typedef struct LRU_ELEMENT_HDR {
	uint32_t	_prev;
	uint32_t	_next;
	uint64_t 	_hash;
	time_t		_when;
	uint32_t	_share_key;
} LRU_element;



//
//	LRU_cache --
//
//	Interleaved free memory is a stack -- fixed sized elements
//


class LRU_Consts {

	public: 

		LRU_Consts() {
			_NTiers = 0;
		}

		virtual ~LRU_Consts() {}

	public:

		uint32_t			_NTiers;
		uint32_t			_Procs;
		bool				_am_initializer;

};



// class LRU_cache : public LRU_Consts {
// 	//
// 	public:
// 		// LRU_cache -- constructor


/**
 * LRU_cache
 * 		The LRU cache manages a reference to a hopscotch hash table and to a storage area.
 * 		The storage area is managed as a free mem stack. 
 * 		Contention for the stack is managed by atomic access.  
*/

class LRU_cache : public LRU_Consts, public AtomicStack<LRU_element> {

	public:

		// LRU_cache -- constructor
		LRU_cache(void *region,size_t record_size, size_t seg_sz, size_t els_per_tier, size_t reserve, uint16_t num_procs, bool am_initializer, uint8_t tier) {
			//
			init_diff_timer();
			//
			_region = region;
			_record_size = record_size;
			_region_size = seg_sz;
			_max_count = els_per_tier;
			_reserve = reserve*num_procs;  // when the count become this, the reserve is being used and it is time to clean up
			//
			_am_initializer = am_initializer;
			//
			// if the reserve goes down, an attempt to increase _count_free should take place
			// if _count_free becomes zero, all memory is used up
			//
			_count_free->store(_max_count);				
			_count = 0;

			_Procs = num_procs;
			_Tier = tier;

			_step = (sizeof(LRU_element) + _record_size);
			//
			_lb_time = (atomic<uint32_t> *)(region);   // these are governing time boundaries of the particular tier
			_ub_time = _lb_time + 1;
			_memory_requested = (_ub_time + 1); // the next pointer in memory
			_reserve_evictor =  (atomic_flag *)(_memory_requested + 1); // the next pointer in memory
			//
			_cascaded_com_area = (Com_element *)(_reserve_evictor + 1);
			//
			initialize_com_area(num_procs);
			_end_cascaded_com_area = _cascaded_com_area + _Procs;

			_all_tiers = nullptr;

			// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

			_start = start();
			_end = end();
			//

			// time lower bound and upper bound for a tier...
			_lb_time->store(UINT32_MAX);
			_ub_time->store(UINT32_MAX);
			_memory_requested->store(0);
			_reserve_evictor->clear();

			pair<uint32_t,uint32_t> *holey_buffer = (pair<uint32_t,uint32_t> *)(_start + _region_size);
			pair<uint32_t,uint32_t> *shared_queue = holey_buffer + _max_count;  // late arrivals
			//
			_timeout_table = new KeyValueManager(holey_buffer, _max_count, shared_queue, num_procs);
			_configured_tier_cooling = ONE_HOUR;
			_configured_shrinkage = 0.3333;

			//
			// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
			auto count_free = setup_region_free_list(_start, _step,(_end - _start));

			if ( count_free != _count_free->load() ) {
				_count_free->store(count_free);
			}

			//
		}

		virtual ~LRU_cache() {}

	public:

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void set_tier_table(LRU_cache **siblings_and_self,uint8_t max_tiers) {
			_all_tiers = siblings_and_self;
			_max_tiers = max_tiers;
		}


		uint8_t *start(void) {
			uint8_t *rr = (uint8_t *)(_end_cascaded_com_area);
			return rr;
		}


		uint8_t *end(void) {
			uint8_t *rr = (uint8_t *)(_end_cascaded_com_area);
			rr += _max_count*_step;
			return rr;
		}




		// set_hash_impl - called by initHopScotch -- set two paritions servicing a random selection.
		//
		void set_hash_impl(void *hh_region,uint32_t els_per_tier) {
			uint8_t *reg1 = (uint8_t *)hh_region;
			_hmap = new HH_map(reg1,els_per_tier,_am_initializer);
		}



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void initialize_com_area(uint16_t num_procs) {
			Com_element *proc_entry = _cascaded_com_area;
			uint32_t P = num_procs;
			for ( uint32_t p = 0; p < P; p++ ) {
				proc_entry->_marker.store(CLEAR_FOR_WRITE);
				proc_entry->_hash = 0L;
				proc_entry->_offset = UINT32_MAX;
				proc_entry->_timestamp = 0;
				proc_entry->_tier = _Tier;
				proc_entry->_proc = p;
				proc_entry->_ops = 0;
				memset(proc_entry->_message,0,MAX_MESSAGE_SIZE);
				proc_entry++;
			}
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		//

		uint32_t free_count(void) {
			auto cnt = _count_free->load();
			return cnt;
		}


		uint32_t current_count(void) {
			auto cnt = _count_free->load();
			return (_max_count - cnt);
		}


		uint32_t max_count(void) {
			return _max_count;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		//

		// LRU_cache method - calls get -- 
		/**
		 * filter_existence_check
		 * 
		 * Both arrays, messages and accesses, contain references to hash words.. 
		 * These are the hash parameters left by the process requesting storage.
		 * 
		*/

		uint32_t		filter_existence_check(com_or_offset **messages,com_or_offset **accesses,uint32_t ready_msg_count) {
			uint32_t new_msgs_count = 0;
			while ( --ready_msg_count >= 0 ) {
				//
				uint64_t hash = (uint64_t)(messages[ready_msg_count]->_cel->_hash);
				uint32_t data_loc = _hmap->get(hash);
				//
				if ( data_loc != 0 ) {    // check if this message is already stored
					messages[ready_msg_count]->_offset = data_loc;  // just putting in an offset... maybe something better
				} else {
					new_msgs_count++;
					accesses[ready_msg_count]->_offset = 0;
				}
			}
			return new_msgs_count;
		}


		/*
			Free memory is a stack with atomic var protection.
			If a basket of new entries is available, then claim a basket's worth of free nodes and return them as a 
			doubly linked list for later entry. Later, add at most two elements to the LIFO queue the LRU.

			Take note that the reserve is just free memory that is untouched unless demand spikes.
			When reserve is called on, the process of eviction has to begin...
		*/

		//
		uint32_t		free_mem_requested(void) {
			// the requested free memory requested 
			uint32_t total_requested = _memory_requested->load(std::memory_order_relaxed);
			return total_requested;
		}

		// LRU_cache method
		void			free_mem_claim_satisfied(uint32_t msg_count) {   // stop requesting the memory... 
			if ( _memory_requested->load() <= msg_count ) {
				_memory_requested->store(0);
			} else {
				_memory_requested->fetch_sub(msg_count);
			}
		}

		// check_count_free_against_reserve
		void			check_count_free_against_reserve() {
			auto boundary = _reserve;
			auto current_count = _count_free->load();
			if ( current_count < boundary ) {
				notify_evictor(boundary - current_count);
			}
		}


		void 			notify_evictor([[maybe_unused]] uint32_t reclaim_target) {
			while( !( _reserve_evictor->test_and_set() ) );
			#ifndef __APPLE__
			_reserve_evictor->notify_one();					// NOTIFY FOR LINUX  (can ony test on an apple)
			#endif
		}


		// claim_free_mem
		// 
		//		The free memory is a stack that is shared. 
		//		A number of items are requested. This method takes as many requested 
		// 		items off the stack as it can and puts the offset into reserved_offsets.
		//		The number of items to be taken should have already been settled prior to a call
		//		to this method. If the reserve is being use, it will be pulled out of the stack 
		//		just the same. Calling method will know about the reserve.
		//

		uint32_t		claim_free_mem(uint32_t ready_msg_count,uint32_t *reserved_offsets) {
			//
			uint8_t *start = this->start();
			size_t step = _step;
			//

			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
			//
			uint32_t status = pop_number(start,ctrl_free,ready_msg_count,reserved_offsets);
			if ( status == UINT32_MAX ) {
				_status = false;
				_reason = "out of free memory: free count == 0";
				return(UINT32_MAX);
			}
			//
			_count_free->fetch_sub(ready_msg_count, std::memory_order_relaxed);

			// If the request cuts into reserves, then the handling thread will 
			// be flagged with a state indicating that some offline eviction should be starting 
			// or should have already been started.

			check_count_free_against_reserve();      // check_count_free_against_reserve

			//
			free_mem_claim_satisfied(ready_msg_count);
			//
			return 0;
		}


		void			return_to_free_mem(LRU_element *el) {			// a versions of push
			uint8_t *start = this->start();
			_atomic_stack_push(start,(LRU_element *)(start + 2*_step),el);
			_count_free->fetch_add(1, std::memory_order_relaxed);
		}





		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * Prior to attachment, the required space availability must be checked.
		 * This is going away... we are using the sorted time stamp buffer...
		*/
		void 			attach_to_lru_list(uint32_t *lru_element_offsets, uint32_t ready_msg_count) {
			// 
			for ( uint32_t i = 0; i < ready_msg_count; i++ ) {
				uint32_t value = lru_element_offsets[i];
				uint32_t key = current_time_next();
				_timeout_table->add_entry(key, value);
			}
			//
		}


		// checks to see if there is enough memory for the request.
		
		bool 			check_free_mem(uint32_t msg_count,bool add) {
			//
			// using _prev to count the free elements... it is not updated in this check (just inspected)
			auto count_free = _count_free->load();
			//
			auto total_requested = _memory_requested->load(std::memory_order_relaxed);
			if ( add ) {
				total_requested += msg_count;			// update public info about the amount requested 
				_memory_requested->fetch_add(msg_count);		// should be the amount of all curren requests
			}
			if ( count_free < total_requested ) return false;
			return true;
		}



		uint32_t 		timeout_table_evictions(list<uint32_t> &offsets_moving,uint32_t req_count) {
			//
			const auto now = system_clock::now();
    		const time_t t_c = system_clock::to_time_t(now);
			uint32_t min_max_time = (t_c - _configured_tier_cooling);
			uint32_t as_many_as = min((uint32_t)(_max_count*_configured_shrinkage),(req_count*3));
			//
			_timeout_table->displace_lowest_value_threshold(offsets_moving,min_max_time,as_many_as);
			return offsets_moving.size();
		}


		void 			transfer_hashes(LRU_cache *next_tier,uint32_t req_count) {
			list<uint32_t> offsets_moving;
			//uint32_t count_reclaimed_stamps = 
			this->timeout_table_evictions(offsets_moving,req_count);
			//
			next_tier->claim_hashes(offsets_moving,this->start());
			this->relinquish_hashes(offsets_moving);
			//
			uint32_t lb_timestamp = _timeout_table->least_time_key();
			this->raise_lru_lb_time_bounds(lb_timestamp);
		}

		// have to wakeup a secondary process that will move data from reserve to primary
		// and move relinquished data to the secondary... (free up reserve again... need it later)

		void 			claim_hashes(list<uint32_t> &moving,uint8_t *evicting_tier_region) {
			// launch this proc...
			//
			auto additional_locations = moving.size();
			uint32_t lru_element_offsets[additional_locations+1];
			// clear the buffer
			memset((void *)lru_element_offsets,0,sizeof(uint32_t)*(additional_locations+1)); 

			// the next thing off the free stack.
			//
			bool mem_claimed = (UINT32_MAX != this->claim_free_mem(additional_locations,lru_element_offsets)); // negotiate getting a list from free memory
			//
			// if there are elements, they are already removed from free memory and this basket belongs to this process..
			if ( mem_claimed ) {
				//
				uint32_t *current = lru_element_offsets;   // offset to new elemnents in the regions
				uint8_t *start = this->start();
				//
				// map hashes to the offsets
				//
				for ( auto offset : moving ) {   // offset of where it is goin
					LRU_element *lel = (LRU_element *)(evicting_tier_region + offset);
					uint64_t hash64 = lel->_hash;
					//
					//
					auto target_offset = *current++;

					if ( this->store_in_hash(hash64,target_offset) != UINT64_MAX ) { // add to the hash table...
						void *src = (void *)(lel + 1);
						void *target = (void *)(start + (target_offset + sizeof(LRU_element)));
						memcpy(target,src,_record_size);
					}
				}
			} else {
				this->transfer_out_of_tier_to_remote(moving);
			}
			//
			this->attach_to_lru_list(lru_element_offsets,additional_locations);  // attach to an LRU as a whole bucket...
		}


		void			relinquish_hashes(list<uint32_t> &moving) {
			//
			uint8_t *start = this->start();
			for ( auto offset : moving ) {   // offset of where it is going
				LRU_element *el = (LRU_element *)(start + offset);
				uint64_t hash64  = el->_hash;
				this->return_to_free_mem(el);
				this->_hmap->del(hash64);
			}
			//
		}


	public:

		bool 			transfer_out_of_tier_to_remote([[maybe_unused]]list<uint32_t> &moving) { return false; }


	public:

		// thread methods....

		// local_evictor is notified to run as soon as the reserve line is passed or from time to time.
		// There is not a huge requirement that it operates at super speed. But, it should start cleaning up space
		// for small numbers of entries accessing and sorting small tails of the time buffer. 
		// It may shift a portion of the time buffer by running compression. 
		// All compressors must make sure to update the upper and lower time limits of a tier.
		//
		void			local_evictor(void) {
			//
			do {
#ifndef __APPLE__
				_reserve_evictor->wait(std::memory_order_acquire);
#endif
				if ( _Tier+1 < _max_tiers ) {
					uint32_t req_count = _Procs;
					LRU_cache *next_tier = this->_all_tiers[_Tier+1];
					this->transfer_hashes(next_tier,req_count);
				} else {
					// crisis mode...				elements will have to be discarded or sent to another machine
					list<uint32_t> offsets_moving;
					uint32_t count_reclaimed_stamps = this->timeout_table_evictions(offsets_moving,_Procs*3);
					if ( count_reclaimed_stamps > 0 ) {
						this->transfer_out_of_tier_to_remote(offsets_moving);   // also copy data...
					}
				}
				_reserve_evictor->clear();
			} while (true);
			//
		}


		uint32_t		raise_lru_lb_time_bounds(uint32_t lb_timestamp) {
			//
			if ( this->_Tier == 0 ) {
				this->_ub_time->store(UINT32_MAX);
				uint32_t lbt = this->_lb_time->load();
				if ( lbt < lb_timestamp ) {
					this->_lb_time->store(lb_timestamp);
					return lbt;
				}
				return 0;
			}
			if ( this->_Tier < _NTiers ) {
				uint32_t lbt = this->_lb_time->load();
				if ( lbt < lb_timestamp ) {
					uint32_t ubt = this->_ub_time->load();
					uint32_t delta = (lb_timestamp - lbt);
					this->_lb_time->store(lb_timestamp);
					ubt -= delta;
					this->_ub_time->store(lb_timestamp);					
					return lbt;
				}
				return 0;
			}
			//
			return 0;
		}


	public:


		uint64_t		store_in_hash(uint32_t full_hash,uint32_t hash_bucket,uint32_t new_el_offset) {
			HMap_interface *T = this->_hmap;
			uint64_t result = T->add_key_value(full_hash,hash_bucket,new_el_offset); //UINT64_MAX
			return result;
		}

		uint64_t		store_in_hash(uint64_t hash64, uint32_t new_el_offset) {
			uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
			uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
			return store_in_hash(full_hash,hash_bucket,new_el_offset);
		}

		uint8_t 		*data_location(uint32_t write_offset) {
			uint8_t *strt = this->start();
			if ( strt != nullptr ) {
				return (this->start() + write_offset);
			}
			return nullptr;
		}


	public:

		void							*_region;
		size_t		 					_record_size;
		size_t							_region_size;
		//
		uint32_t						_step;
		uint8_t							*_start;
		uint8_t							*_end;		

		uint16_t						_count;
		uint16_t						_max_count;  // max possible number of records

		//
		uint8_t							_Tier;
		uint8_t							_reserve;
		uint8_t							_max_tiers;

		uint32_t						_configured_tier_cooling;
		double							_configured_shrinkage;

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

 		HMap_interface 					*_hmap;
 		KeyValueManager					*_timeout_table;
		//
		LRU_cache 						**_all_tiers;		// set by caller

		Com_element						*_cascaded_com_area;   // if outsourcing tiers ....
		Com_element						*_end_cascaded_com_area;

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		//
		atomic<uint32_t>				*_lb_time;
		atomic<uint32_t>				*_ub_time;
		atomic<uint32_t>				*_memory_requested;
		atomic<uint32_t>				*_count_free;
		atomic_flag						*_reserve_evictor;
		//
};


#endif  // _H_HOPSCOTCH_HASH_LRU_