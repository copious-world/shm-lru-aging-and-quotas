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
#include <tuple>


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

// ---- ---- ---- ---- ---- ---- ---- ---- ----
//
#define NUM_ATOMIC_FLAG_OPS_PER_TIER		(4)
#define LRU_ATOMIC_HEADER_WORDS				(8)


/**
 * The 64 bit key stores a 32bit hash (xxhash or other) in the lower word.
 * The top 32 bits stores a structured bit array. The top 1 bit will be
 * the selector of the hash region where the key match will be found. The
 * bottom 20 bits will store the element offset (an element number) to the position the
 * element data is stored. 
*/


/**
 * The atomic stack is a fixed size memory allocation scheme. 
 * The intention is that elements will be moved out by their age if the stack becomes full.
 * 
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


// uint64_t hash is the augmented hash...

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
	uint32_t	_info;
	uint32_t	_next;
	uint64_t 	_hash;
	time_t		_when;
	uint32_t	_share_key;

	void		init(uint64_t hash_init = UINT64_MAX) {
		this->_info = UINT32_MAX;
		this->_hash = hash_init;
		this->_when = 0;
	}

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

		static const uint8_t	NUM_ATOMIC_OPS{NUM_ATOMIC_FLAG_OPS_PER_TIER};			

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
		LRU_cache(void *region, size_t record_size, size_t seg_sz, size_t els_per_tier, size_t reserve, uint16_t num_procs, bool am_initializer, uint8_t tier) {
			//
			init_diff_timer();
			_all_tiers = nullptr;

			//
			_region = region;
			_endof_region = ((uint8_t *)region + seg_sz);
			//
			_record_size = record_size;
			_max_count = els_per_tier;
			_reserve = reserve*num_procs;  // when the count become this, the reserve is being used and it is time to clean up
			//
			_am_initializer = am_initializer;
			//

			_Procs = num_procs;
			_Tier = tier;
			_step = (sizeof(LRU_element) + _record_size);
			_region_size = _max_count*_step;
			//
			//
			// if the reserve goes down, an attempt to increase _count_free should take place
			// if _count_free becomes zero, all memory is used up
			//
			_lb_time = (atomic<uint32_t> *)(region);   // these are governing time boundaries of the particular tier
			_ub_time = 			(_lb_time + 1);
			_memory_requested =	(_ub_time + 1); // the next pointer in memory
			_count_free = 		(_memory_requested + 1);	
			_reserve_evictor =	(atomic_flag *)(_count_free + 1); // the next pointer in memory
			//
			_cascaded_com_area = (Com_element *)(_lb_time + LRU_ATOMIC_HEADER_WORDS);  // past atomic evictors and reserved ones as well
			_end_cascaded_com_area = _cascaded_com_area + _Procs;
			if ( !check_end((uint8_t *)_cascaded_com_area) || !check_end((uint8_t *)_end_cascaded_com_area) ) {
				throw "lru_cache (1) sizes overrun allocated region determined by region_sz";
			}
			//
			_start = start();
			_end = end();
			if ( !check_end((uint8_t *)_start) ) {
				throw "lru_cache (2) sizes overrun allocated region determined by region_sz";
			}
			if ( !check_end((uint8_t *)_end) ) {
				throw "lru_cache (3) sizes overrun allocated region determined by region_sz";
			}
			//
			pair<uint32_t,uint32_t> *holey_buffer = (pair<uint32_t,uint32_t> *)(_end);
			pair<uint32_t,uint32_t> *shared_queue = (holey_buffer + _max_count*2 + num_procs);  // late arrivals
			if ( !check_end((uint8_t *)shared_queue) ) {
				throw "lru_cache (4) sizes overrun allocated region determined by region_sz";
			}

			// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

			// time lower bound and upper bound for a tier...
			if ( am_initializer ) {
				_count_free->store(_max_count);	
				_lb_time->store(UINT32_MAX);
				_ub_time->store(UINT32_MAX);
				_memory_requested->store(0);
				_reserve_evictor->clear();
				//
				initialize_com_area(num_procs);
				//
				_timeout_table = new Shared_KeyValueManager(holey_buffer, _max_count, shared_queue, num_procs);
				_configured_tier_cooling = ONE_HOUR;
				_configured_shrinkage = 0.3333;

				//
				// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
				auto count_free = setup_region_free_list(_start, _step,(_end - _start));

				if ( count_free != _count_free->load() ) {
					_count_free->store(count_free);
				}
			} else {
				attach_region_free_list(_start,(_end - _start));
			}
			//
		}

		virtual ~LRU_cache() {}

	public:


		/**
		 * check_expected_lru_region_size
		*/

		static uint32_t check_expected_lru_region_size(size_t record_size, size_t els_per_tier, uint32_t num_procs) {
			//
			size_t this_tier_atomics_sz = LRU_ATOMIC_HEADER_WORDS*sizeof(atomic<uint32_t>);  // currently 5 accessed
			size_t com_reader_per_proc_sz = sizeof(Com_element)*num_procs;
			size_t max_count_lru_regions_sz = (sizeof(LRU_element) + record_size)*(els_per_tier + 2);
			// _max_count*2 + num_procs
			size_t holey_buffer_sz = sizeof(pair<uint32_t,uint32_t>)*els_per_tier*2 + sizeof(pair<uint32_t,uint32_t>)*num_procs; // storage for timeout management

			uint32_t predict = (this_tier_atomics_sz + com_reader_per_proc_sz + max_count_lru_regions_sz + holey_buffer_sz);
			//
			return predict;
		}


		/**
		 * check_end
		*/

		bool check_end(uint8_t *ref,bool expect_end = false) {
			if ( ref == _endof_region ) {
				if ( expect_end ) return true;
			}
			if ( (ref < _endof_region)  && (ref > _region) ) return true;
			return false;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * set_tier_table
		 * 
		 * Each LRU works for tier. Sometimes one LRU may exchange entries with another tier.
		 * 
		*/
		void set_tier_table(LRU_cache **siblings_and_self,uint8_t max_tiers) {
			_all_tiers = siblings_and_self;
			_max_tiers = max_tiers;
		}



		/**
		 * start -- start is not just the beginning of the shared memory area (for any reason or other).
		*/

		uint8_t *start(void) {
			uint8_t *rr = (uint8_t *)(_end_cascaded_com_area);
			return rr;
		}


		/**
		 * end - similar to `start`, the end of the region has to be calculated. There may be some controls 
		 * before the actual end.
		*/

		uint8_t *end(void) {
			uint8_t *rr = (uint8_t *)(_end_cascaded_com_area) + _region_size;
			return rr;
		}



		/**
		 * set_hash_impl - allow the user class to determine the implementation of the hash table. 
		 * -- called by initHopScotch -- set two paritions servicing a random selection.
		*/
		void set_hash_impl(void *hh_region,size_t hh_seg_sz,uint32_t els_per_tier) {
			uint8_t *reg1 = (uint8_t *)hh_region;
			_hmap = new HH_map<>(reg1,hh_seg_sz,els_per_tier,_am_initializer);
		}


		void set_random_bits(void *shared_bit_region) {
			if ( _hmap ) {
				_hmap->set_random_bits(shared_bit_region);
			}
		}



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * initialize_com_area
		*/

		void initialize_com_area(uint16_t num_procs) {
			if ( check_end((uint8_t *)(_cascaded_com_area)) ) {
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
					if ( !check_end((uint8_t *)_cascaded_com_area) ) {
						throw "initialize_com_area : beyond end of buffer";
					}
				}
			}
		}




		/**
		 * hash_table_value_restore_thread -- wrap the hash table's value restore thread method
		 * 	This is just so that the calling class doesn't have to refer to the hash table itself.
		 * 	This in spite of the fact that the calling class provides the reference. 
		*/

		void hash_table_value_restore_thread(uint8_t slice_for_thread) {  // a wrapper (parent must call a while loop... )
			_hmap->value_restore_runner(slice_for_thread,this->_thread_id);
		}


		/**
		 * hash_table_random_generator_thread_runner
		*/
		void hash_table_random_generator_thread_runner(void) {
			_hmap->random_generator_thread_runner();
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * Gets the current free count from the `_count_free` atomic.
		*/
		uint32_t free_count(void) {
			auto cnt = _count_free->load();
			return cnt;
		}


		/**
		 * Gets the current count from the part of max count that is not in the `_count_free` atomic.
		*/
		uint32_t current_count(void) {
			auto cnt = _count_free->load();
			return (_max_count - cnt);
		}


		/**
		 * max_count - retuns the value set as a configuration parameter.
		 * Maybe some implementation needs to do some calculation; so, it is in a method.
		*/
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
		 * The `_cel` parameter refers to the process table.
		*/

		uint32_t		filter_existence_check(com_or_offset **messages,com_or_offset **accesses,uint32_t ready_msg_count) {
			uint32_t new_msgs_count = 0;
			while ( --ready_msg_count >= 0 ) {  // walk this list backwards...
				//
				uint64_t hash = (uint64_t)(messages[ready_msg_count]->_cel->_hash);
				uint32_t data_loc = _hmap->get(hash);  // this thread contends with others to get the value
				//
				if ( data_loc != UINT32_MAX ) {    // check if this message is already stored
					messages[ready_msg_count]->_offset = data_loc;  // just putting in an offset... maybe something better
				} else {
					new_msgs_count++;							// the hash has not been found
					accesses[ready_msg_count]->_offset = 0;		// the offset is not yet known (the space will claimed later)
				}
			}
			return new_msgs_count;
		}


		/*
			Free memory is a stack with atomic var protection.
			If a basket of new entries is available, then claim a basket's worth of free nodes and return them as a 
			doubly linked list for later entry. Later, add at most two elements to the STACK of the LRU.

			Take note that the reserve is just free memory that is untouched unless demand spikes.
			When reserve is called on, the process of eviction has to begin...
		*/

		/**
		 * free_mem_requested -- allow for a thread servicing a request to get the amount requesed.
		*/
		uint32_t		free_mem_requested(void) {
			// the requested free memory requested 
			uint32_t total_requested = _memory_requested->load(std::memory_order_relaxed);
			return total_requested;
		}


		/**
		 * free_mem_claim_satisfied -- when the amount of memory (slots) needed to satisfy a request 
		 * has been obtained, set the known requested amount back to zero or substract the number of messages satisfied.
		*/
		void			free_mem_claim_satisfied(uint32_t msg_count) {   // stop requesting the memory... 
			if ( _memory_requested->load() <= msg_count ) {
				_memory_requested->store(0);
			} else {
				_memory_requested->fetch_sub(msg_count);
			}
		}

		/**
		 * check_count_free_against_reserve -- check to see if the number of stored objects is nearing the storage limit,
		 * if it is, then tell the evictor to start evicting what it can. (notify thread)
		*/
		void			check_count_free_against_reserve() {
			auto boundary = _reserve;
			auto current_count = _count_free->load();
			if ( current_count < boundary ) {
				notify_evictor(boundary - current_count);
			}
		}

		/**
		 * notify_evictor -- use atomic notification.
		*/
		void 			notify_evictor([[maybe_unused]] uint32_t reclaim_target) {
			while( !( _reserve_evictor->test_and_set() ) );
#ifndef __APPLE__
			_reserve_evictor->notify_one();					// NOTIFY FOR LINUX  (can ony test on an apple)
#else
			_evictor_spinner.signal();
#endif
		}


		/**
		 *	claim_free_mem
		 * 
		 * 		The free memory is a stack that is shared.
		 * 		A number of items are requested. This method takes as many requested
		 *  	items off the stack as it can and puts each offset into reserved_offsets.
		 * 		The number of items to be taken should have already been settled prior to a call
		 * 		to this method. If the reserve is being used, the items will be pulled out of the stack
		 * 		just the same. The calling method will know about the reserve.
		*/

		uint32_t		claim_free_mem(uint32_t ready_msg_count,uint32_t *reserved_offsets) {
			//
			uint8_t *start = this->start();
			//
			uint32_t status = pop_number(start,ready_msg_count,reserved_offsets);
			if ( status == UINT32_MAX ) {
				_status = false;
				_reason = "out of free memory: free count == 0";
				return(UINT32_MAX);
			}

			// If the request cuts into reserves, then the handling thread will 
			// be flagged with a state indicating that some offline eviction should be starting 
			// or should have already been started.

			check_count_free_against_reserve();      // check_count_free_against_reserve
			//
			free_mem_claim_satisfied(ready_msg_count);
			//
			return 0;
		}


		/**
		 *	return_to_free_mem - returns a single storage object back to the free stack.
		 *  	calls on the atomic stack methods to release the object. Then, updates the 
		 *  	value of the free count.
		*/
		void			return_to_free_mem(LRU_element *el) {			// a versions of push
			uint8_t *start = this->start();
			_atomic_stack_push(start,el);
		}



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 *	attach_to_lru_list
		 * 
		 * 		Those elements being added are stored in data structure that manages the presence of the data 
		 * 		relative to preconfigured periods of time for them to remain locally. 
		 * 
		 * 		This method takes of list of newly minted offsets (`ready_msg_count` long), marks them with the current
		 * 		system time and then puts the offset,time pair into the data structure, `_timeout_table`.
		 * 
		*/
		void 			attach_to_lru_list(uint32_t *lru_element_offsets, uint32_t ready_msg_count) {
			//
			uint32_t entry_times[ready_msg_count];
			for ( uint32_t i = 0; i < ready_msg_count; i++ ) {
				entry_times[i] = current_time_next();
			}
			_timeout_table->add_entries(lru_element_offsets,entry_times,ready_msg_count);
			//
		}


		void			timestamp_update(uint32_t *updating_offsets,uint8_t count_updates) {
			//
			uint32_t entry_times[count_updates];
			uint32_t old_times[count_updates];
			//
			for ( uint32_t i = 0; i < count_updates; i++ ) {
				entry_times[i] = current_time_next();
				auto offset = *updating_offsets++;
				LRU_element *lrue = (LRU_element *)data_info_location(offset);
				old_times[i++] = lrue->_when;
			}
			//
			_timeout_table->update_entries(old_times,entry_times,count_updates);
		}

		/**
		 * check_and_maybe_request_free_mem -- checks to see if there is enough memory for the request.
		 * 		Threads learn of the requested amount by examining the `_memory_requested` atomic variable.
		 * 		Serves to increment free memory the *add* flag is true. 
		*/
		bool 			check_and_maybe_request_free_mem(uint32_t msg_count,bool add) {
			//
			// using _prev to count the free elements... it is not updated in this check (just inspected)
			auto count_free = _count_free->load(std::memory_order_acquire);
			//
			auto total_requested = _memory_requested->load(std::memory_order_relaxed);
			if ( add ) {
				total_requested += msg_count;			// update public info about the amount requested 
				_memory_requested->fetch_add(msg_count);		// should be the amount of all curren requests
			}
			if ( count_free < total_requested ) return false;
			return true;
		}


		/**
		 * timeout_table_evictions
		*/
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


		/**
		 * transfer_hashes - finds a number of entries in the hash table and memory to age out to another tier.
		 * 		This method takes another tier as a parameter. It works to move the hashes it wants to age out
		 * 		to the tier that has been passed.
		*/
		void 			transfer_hashes(LRU_cache *next_tier,uint32_t req_count,uint8_t thread_id)  {
			//
			list<uint32_t> offsets_moving;  // the offsets that will be moved
			//
			this->timeout_table_evictions(offsets_moving,req_count);  // offsets_moving will have the hashes to move
			if ( offsets_moving.size() ) {
				next_tier->claim_hashes(offsets_moving,this->start());		// the next tier insersts its hashes.
				this->relinquish_hashes(offsets_moving,thread_id);			// this tier gives up the hashes moved.
				//
				uint32_t lb_timestamp = _timeout_table->least_time_key();	// reset the time boundaries (least time is now greater)
				this->raise_lru_lb_time_bounds(lb_timestamp);
			}
		}

		// have to wakeup a secondary process that will move data from reserve to primary
		// and move relinquished data to the secondary... (free up reserve again... need it later)

		/**
		 * claim_hashes -- only used during a transfer of hashes from one tier to another.
		*/
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
				for ( auto offset : moving ) {   // offset of where it is going
					LRU_element *lel = (LRU_element *)(evicting_tier_region + offset);
					uint64_t hash64 = lel->_hash;
					//
					//
					uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
					uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
					auto target_offset = *current++;

					uint8_t which_slice;
					atomic<uint32_t> *control_bits;
					uint32_t cbits = 0;
					uint32_t cbits_op = 0;
					uint32_t cbits_base_op = 0;
					hh_element *buffer = nullptr;
					hh_element *end_buffer = nullptr;
					hh_element *el = nullptr;
					CBIT_stash_holder *cbit_stashes[4];

					// hash_bucket goes in by ref and will be stamped
					uint64_t augmented_hash = this->get_augmented_hash_locking(full_hash,&control_bits,&hash_bucket,&which_slice,&cbits,&cbits_op,&cbits_base_op,&el,&buffer,&end_buffer,cbit_stashes);
					//
					if ( augmented_hash != UINT64_MAX ) { // add to the hash table...
						void *src = (void *)(lel + 1);
						void *target = (void *)(start + (target_offset + sizeof(LRU_element)));
						memcpy(target,src,_record_size);
						//
						// -- if there is a problem, it will affect older entries
						this->store_in_hash_unlocking(control_bits,full_hash,hash_bucket,offset,which_slice,cbits,cbits_op,cbits_base_op,el,buffer,end_buffer,cbit_stashes);
						//
					}
					//
				}
				//
				this->attach_to_lru_list(lru_element_offsets,additional_locations);  // attach to an LRU as a whole bucket...
			} else {
				this->transfer_out_of_tier_to_remote(moving);
			}
		}


		/**
		 * relinquish_hashes -- clear the tables that know of the moving hashes of the moving hashes. i.e. discard the
		 * 		hashes mentioned in `moving`.
		*/
		void			relinquish_hashes(list<uint32_t> &moving,uint8_t thread_id = 1) {
			//
			uint8_t *start = this->start();
			for ( auto offset : moving ) {			// offset of where it is going
				LRU_element *el = (LRU_element *)(start + offset);
				uint64_t hash64  = el->_hash;
				this->return_to_free_mem(el);
				this->_hmap->del(hash64,thread_id);
			}
			//
		}


	private:

		void (* app_transfer_method)(list<tuple<uint64_t,time_t,uint8_t *>> &) = nullptr;

	public:

		/**
		 * transfer_out_of_tier_to_remote - this method is called when an element has run out of tiers.
		 * 
		 * 	Depending on the application implementation of the `app_transfer_method`, elements moving past the 
		 * 	number of tiers will either come to the end of their life, go into a file or other secondary storage,
		 * 	or be moved across the network, maybe to a processor that acts like a clearing house. 
		 * 	
		 * 	In some scenarios, the object with a particular hash may be still be accessed after the operation, albeit slowly.
		 * 
		 * 	The interface to entire module separates the `get` operation from `set` or `put`. So, if a `get` fails on the 
		 * 	current machine, it might succeed in the cluster. Outside the cluster, it is expected that that hash will be 
		 * 	much more universal, i.e. the hashes within a cluster will have a much smaller range of values relative to 
		 * 	hashes used in the wide area.
		 * 
		 * The `LRU_cache` type of object does not implemen a method 
		*/
		bool 			transfer_out_of_tier_to_remote(list<uint32_t> &moving) {
			if ( app_transfer_method != nullptr ) {

				list<tuple<uint64_t,time_t,uint8_t *>> move_data;

				uint8_t *start = this->start();
				for ( auto offset : moving ) {			// offset of where it is going
					LRU_element *el = (LRU_element *)(start + offset);
					uint64_t hash64  = el->_hash;
					time_t update_time = el->_when;
					uint8_t *data = this->data_info_location(offset);  /// pointer to the header info before data block
					//
					tuple<uint64_t,time_t,uint8_t *> dat{hash64,update_time,data};
					move_data.push_back(dat);
				}

				(* app_transfer_method)(move_data);
				relinquish_hashes(moving,251);  // some suprious thread id
			}
			return false; 
		}


		/**
		 * set_app_transfer_method -- the application calls this method to set the 
		 * references to the application transfer method.
		*/
		void set_app_transfer_method(void (* atm)(list<tuple<uint64_t,time_t,uint8_t *>> &)) {
			app_transfer_method = atm;
		}


	public:

		// thread methods....

		/**
		 * local_evictor
		 * 
		 * local_evictor is notified to run as soon as the reserve line is passed or from time to time.
		 * There is not a huge requirement that it operates at super speed. But, it should start cleaning up space
		 * for small numbers of entries accessing and sorting small tails of the time buffer. 
		 * It may shift a portion of the time buffer by running compression. 
		 * All compressors must make sure to update the upper and lower time limits of a tier.
		*/

		void			local_evictor(void) {   // parent caller executes a while loop and determins if it is still running
			//
#ifndef __APPLE__
			_reserve_evictor->wait(true,std::memory_order_acquire);
#else
			_evictor_spinner.wait();
#endif
			uint8_t thread_id = this->_thread_id;
			if ( (_Tier+1) < _max_tiers ) {
				uint32_t req_count = _Procs;
				LRU_cache *next_tier = this->_all_tiers[_Tier+1];
				this->transfer_hashes(next_tier,req_count,thread_id);
			} else {
				// crisis mode...				elements will have to be discarded or sent to another machine
				list<uint32_t> offsets_moving;
				uint32_t count_reclaimed_stamps = this->timeout_table_evictions(offsets_moving,_Procs*3);
				if ( count_reclaimed_stamps > 0 ) {
					this->transfer_out_of_tier_to_remote(offsets_moving);   // also copy data...
				}
			}
			_reserve_evictor->clear();
			//
		}


		/**
		 * raise_lru_lb_time_bounds - given an update to the lowest time bound of (this) lru cache instance
		 * resets the lower bounds using atomic updates. If the tier is the zero tier, the upper bound is not
		 * set (presumed inifinte). Tiers other than the zero tier update their upper time by figuring the delta 
		 * between their old lower bound and the new and then adding it to the upper. As such, the upper time bound
		 * does not necessarily express the time of the tier's newest data, but should include it.
		 * 
		*/

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
					ubt += delta;
					this->_ub_time->store(lb_timestamp);					
					return lbt;
				}
				return 0;
			}
			//
			return 0;
		}


	public:

		/**
		 * store_in_hash  -- full_hash is 32 bit hash of data... hash_bucket is a modulus relative to the element count.
		 * 		hash_bucket can have some control bits at the top of it indicating if the element is live yet and if 
		 * 		so, which segment it lies in.
		 * 
		 * 		The full hash is stored in the top part of a 64 bit word. While the bucket information
		 * 		is stored in the lower half.
		 * 
		 * 		| full 32 bit hash || control bits (2 to 4) | bucket number |
		 * 		|   32 bits			| 32 bits ---->					        |
		 * 							| 4 bits				| 28 bits		|  // 28 bits for 500 millon entries
		*/

/*
		// no longer called
		uint64_t		store_in_hash(uint32_t full_hash,uint32_t hash_bucket,uint32_t new_el_offset,uint8_t thread_id = 1) {
			HMap_interface *T = this->_hmap;
			uint64_t result = UINT64_MAX;

			uint8_t selector_bit = 0;
			// a bit for being entered and one or more for which slab...
			if ( !(selector_bit_is_set(hash_bucket,selector_bit)) ) {
				// result = T->add_key_value(full_hash,hash_bucket,new_el_offset,thread_id); //UINT64_MAX
			} else {
				result = T->update(hash_bucket,full_hash,new_el_offset,thread_id);
			}
			return result;
		}

		/ * *
		 * store_in_hash
		*       /

		// no longer called
		uint64_t		store_in_hash(uint64_t hash64, uint32_t new_el_offset,uint8_t thread_id = 1) {
			uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
			uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
			return store_in_hash(full_hash,hash_bucket,new_el_offset,thread_id);
		}
*/


		/**
		 * data_location
		*/

		uint8_t 		*data_location(uint32_t write_offset) {
			uint8_t *strt = this->start();
			if ( strt != nullptr ) {
				return (this->start() + write_offset + sizeof(LRU_element));
			}
			return nullptr;
		}

		uint8_t 		*data_info_location(uint32_t write_offset) {
			uint8_t *strt = this->start();
			if ( strt != nullptr ) {
				return (this->start() + write_offset);
			}
			return nullptr;
		}


		/**
		 * getter
		*/

		uint32_t		getter(uint32_t full_hash,uint32_t h_bucket,[[maybe_unused]] uint32_t timestamp = 0) {
			// can check on the limits of the timestamp if it is not zero
			return _hmap->get(full_hash,h_bucket);
		}


		/**
		 * update_in_hash
		*/

		uint64_t		update_in_hash(uint32_t full_hash,uint32_t hash_bucket,uint32_t new_el_offset) {
			HMap_interface *T = this->_hmap;
			uint64_t result = T->update(full_hash,hash_bucket,new_el_offset);
			return result;
		}


		/**
		 * remove_key
		*/

		void 			remove_key(uint32_t full_hash, uint32_t h_bucket, uint32_t timestamp) {
			_timeout_table->remove_entry(timestamp);
			_hmap->del(full_hash,h_bucket);
		}


		/**
		 * free_memory_and_key -- calls two methods: one, to reclaim the memory that the eleent used; 
		 *	two, a second method to remove the object hash from the hash table... 
		*/

		void			free_memory_and_key(LRU_element *le,uint32_t hash_bucket, uint32_t full_hash, uint32_t timestamp) {
			this->return_to_free_mem(le);
			this->remove_key(hash_bucket,full_hash,timestamp);
		}

		uint64_t		get_augmented_hash_locking(uint32_t full_hash, atomic<uint32_t> **control_bits_ref, uint32_t *h_bucket_ref,uint8_t *which_table_ref,uint32_t *cbits_ref,uint32_t *cbits_op_ref,uint32_t *cbits_base_op_ref,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,CBIT_stash_holder *cbit_stashes[4]) {
			HMap_interface *T = this->_hmap;
			uint64_t result = UINT64_MAX;
			//
			uint8_t selector_bit = 0;
			auto h_bucket = *h_bucket_ref;
			// a bit for being entered and one or more for which slab...
			if ( !(selector_bit_is_set(h_bucket,selector_bit)) ) {
				//
				uint8_t which_table = 0;
				uint32_t cbits = 0;
				uint32_t cbits_op = 0;
				uint32_t cbits_base_ops = 0;
				//
				if ( T->prepare_for_add_key_value_known_refs(control_bits_ref,h_bucket,which_table,cbits,cbits_op,cbits_base_ops,bucket_ref,buffer_ref,end_buffer_ref,cbit_stashes) ) {
					*which_table_ref = which_table;
					h_bucket = stamp_key(h_bucket,which_table);   // but the control bits into the hash
					*h_bucket_ref = h_bucket;
					*cbits_ref = cbits;
					*cbits_op_ref = cbits_op;
					*cbits_base_op_ref = cbits_base_ops;
					result = h_bucket | ((uint64_t)full_hash << HALF);
				}
			}
			return result;
		}

		//  ----
		void			store_in_hash_unlocking(atomic<uint32_t> *control_bits,uint32_t full_hash, uint32_t h_bucket,uint32_t offset,uint8_t which_table,uint32_t cbits,uint32_t cbits_op,uint32_t cbits_base_ops,hh_element *bucket,hh_element *buffer,hh_element *end_buffer,CBIT_stash_holder *cbit_stashes[4]) {
			HMap_interface *T = this->_hmap;
			T->add_key_value_known_refs(control_bits,full_hash,h_bucket,offset,which_table,cbits,cbits_op,cbits_base_ops,bucket,buffer,end_buffer,cbit_stashes);
		}
		

	public:

		void							*_region;
		void 							*_endof_region;
		//
		size_t		 					_record_size;
		size_t							_region_size;
		//
		uint32_t						_step;
		uint8_t							*_start;
		uint8_t							*_end;

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		uint16_t						_max_count;  // max possible number of records
		//
		uint8_t							_Tier;
		uint8_t							_max_tiers;
		uint8_t							_thread_id;
		//
		uint16_t						_reserve;
		//
		uint32_t						_configured_tier_cooling;
		double							_configured_shrinkage;

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

 		HMap_interface 					*_hmap;
 		Shared_KeyValueManager			*_timeout_table;
		//
		LRU_cache 						**_all_tiers;		// set by caller
		//
		Com_element						*_cascaded_com_area;   // if outsourcing tiers ....
		Com_element						*_end_cascaded_com_area;

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		//
		atomic<uint32_t>				*_lb_time;
		atomic<uint32_t>				*_ub_time;
		atomic<uint32_t>				*_memory_requested;
		atomic_flag						*_reserve_evictor;
		//

#ifdef __APPLE__
		Spinners						_evictor_spinner;
#endif

};


#endif  // _H_HOPSCOTCH_HASH_LRU_