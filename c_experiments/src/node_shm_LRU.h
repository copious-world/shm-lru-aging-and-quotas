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
#include "holey_buffer.h"
#include "atomic_proc_rw_state.h"


#include "time_bucket.h"


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


template<typename T>
inline string joiner(list<T> &jlist) {
	if ( jlist.size() == 0 ) {
		return("");
	}
	stringstream ss;
	for ( auto v : jlist ) {
		ss << v;
		ss << ',';
	}
	string out = ss.str();
	return(out.substr(0,out.size()-1));
}

template<typename K,typename V>
inline string map_maker_destruct(map<K,V> &jmap) {
	if ( jmap.size() == 0 ) {
		return "{}";
	}
	stringstream ss;
	char del = 0;
	ss << "{";
	for ( auto p : jmap ) {
		if ( del ) { ss << del; }
		del = ',';
		K h = p.first;
		V v = p.second;
		ss << "\""  << h << "\" : \""  << v << "\"";
		delete p.second;
	}
	ss << "}";
	string out = ss.str();
	return(out.substr(0,out.size()));
}


typedef struct LRU_ELEMENT_HDR {
	uint32_t	_prev;
	uint32_t	_next;
	uint64_t 	_hash;
	time_t		_when;
	uint32_t	_share_key;
} LRU_element;



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


//
//	LRU_cache --
//
//	Interleaved free memory is a stack -- fixed sized elements
//


class LRU_Consts {

	public: 

		LRU_Consts() {
			_status = true;
			_NTiers = 0;
		}

		virtual ~LRU_Consts() {}

	public:

		uint32_t			_NTiers;
		uint32_t			_Procs;
		bool				_status;
		bool				_am_initializer;

		uint32_t 			_beyond_entries_for_tiers_and_mutex;

};



// class LRU_cache : public LRU_Consts {
// 	//
// 	public:
// 		// LRU_cache -- constructor

class LRU_cache : public LRU_Consts {

	public:

		// LRU_cache -- constructor
		LRU_cache(void *region,size_t record_size, size_t seg_sz, size_t els_per_tier,size_t reserve_percent,uint16_t num_procs, bool am_initializer, uint8_t tier) {
			//
			_region = region;
			_record_size = record_size;
			_region_size = seg_sz;
			_max_count = els_per_tier;
			_reserve_percent = reserve_size;
			//
			_am_initializer = am_initializer;
			//
			_count_free = _max_count;
			_count = 0;

			_Procs = num_procs;
			_Tier = tier;

			_step = (sizeof(LRU_element) + _record_size);
			//
			_lb_time = (atomic<uint32_t> *)(region);   // these are governing time boundaries of the particular tier
			_ub_time = _lb_time + 1;

			//
			_cascaded_com_area = (Com_element *)(_ub_time + 1);
			initialize_com_area(num_procs);
			_end_cascaded_com_area = _cascaded_com_area + _Procs;


			_start = start();
			_end = end();
			//
			_reserve_end = _region + _region_size;
			_reserve_start = end;
			_reserve_count_free = (_max_count/100)*_reserve_percent;

			// time lower bound and upper bound for a tier...
			_lb_time->store(UINT32_MAX);
			_ub_time->store(UINT32_MAX);

		}

		virtual ~LRU_cache() {}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

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
		void set_hash_impl(void *hh_region,size_t seg_size) {
			//
			uint8_t *reg1 = hh_region;
			uint8_t *reg2 = reg1 + (seg_size/2);
			HH_map *hmap1 = new HH_map(reg1,els_per_tier/2,_am_initializer);
			HH_map *hmap2 = new HH_map(reg2,els_per_tier/2,_am_initializer);
			//
			_hmap_i[0] = hmap1;
			_hmap_i[1] = hmap2;
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


		// HH_map method - calls get -- 
		//
		uint32_t 		partition_get(uint64_t key) {
			uint8_t selector = ((key & HH_SELECT_BIT) == 0) ? 0 : 1;
			HMap_interface *T = _hmap_i[selector];
			return get_hh_map(T, key);
		}

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
				uint64_t hash = (uint64_t *)(messages[ready_msg_count]->_cel->_hash);
				uint32_t data_loc = this->partition_get(hash);
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

		uint32_t		free_mem_requested(void) {
			return 0;
		}

		uint32_t claim_free_mem([[maybe_unused]] uint32_t ready_msg_count,[[maybe_unused]] uint32_t *reserved_offsets) {
			return 0;
		}




		// LRU_cache method
		atomic<uint32_t> *wait_on_tail(LRU_element *ctrl_tail,bool set_high = false,uint32_t delay = 4) {
			//
			auto flag_pos = static_cast<atomic<uint32_t>*>(&(ctrl_tail->_share_key));
			if ( set_high ) {
				uint32_t check = UINT32_MAX;
				while ( check !== 0 ) {
					check = flag_pos->load(std::memory_order_relaxed);
					if ( check != 0 )  {			// waiting until everyone is done with it
						usleep(delay);
					}
				}
				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,UINT32_MAX)
							&& (ctrl_tail->_share_key) !== UINT32_MAX));
			} else {
				uint32_t check = UINT32_MAX;
				while ( check == UINT32_MAX ) {
					check = flag_pos->load(std::memory_order_relaxed);
					if ( check == UINT32_MAX )  {
						usleep(delay);
					}
				}
				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,(check+1))
							&& (ctrl_tail->_share_key < UINT32_MAX) );
			}
			return flag_pos;
		}


		// LRU_cache method
		void done_with_tail(LRU_element *ctrl_tail,atomic<uint32_t> *flag_pos,bool set_high = false) {
			if ( set_high ) {
				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,0)
							&& (ctrl_tail->_share_key == UINT32_MAX));   // if some others have gone through OK
			} else {
				auto prev_val = flag_pos->load();
				if ( prev_val == 0 ) return;  // don't go below zero
				flag_pos->fetch_sub(ctrl_tail->_share_key,1);
			}
		}

		/**
		 * Prior to attachment, the required space availability must be checked.
		*/
		void attach_to_lru_list(uint32_t *lru_element_offsets,uint32_t ready_msg_count) {
			//
			uint32_t last = lru_element_offsets[(ready_msg_count - 1)];  // freed and assigned to hashes...
			uint32_t first = lru_element_offsets[0];  // freed and assigned to hashes...
			//
			uint8_t *start = this->start();
			size_t step = _step;
			//
			LRU_element *ctrl_hdr = (LRU_element *)start;
			//
			// wait
			LRU_element *ctrl_tail = (LRU_element *)(start + step);
			auto tail_block = wait_on_tail(ctrl_tail,false); // WAIT :: stops if the tail hash is set high ... this call does not set it

			// new head
			auto head = static_cast<atomic<uint32_t>*>(&(ctrl_hdr->_next));
			//
			uint32_t next = head->exchange(first);  // ensure that some next (another basket first perhaps) is available for buidling the LRU
			//
			LRU_element *old_first = (LRU_element *)(start + next);  // this has been settled
			//
			old_first->_prev = last;			// ---- ---- ---- ---- ---- ---- ---- ----
			//
			// thread the list
			for ( uint32_t i = 0; i < ready_msg_count; i++ ) {
				LRU_element *current_el = (LRU_element *)(start + lru_element_offsets[i]);
				current_el->_prev = ( i > 0 ) ? lru_element_offsets[i-1] : 0;
				current_el->_next = ((i+1) < ready_msg_count) ? lru_element_offsets[i+1] : next;
			}
			//

			done_with_tail(ctrl_tail,tail_block,false);
		}


	

		void			wait_for_reserves([[maybe_unused]] uint32_t req_count) {}

		bool			has_reserve(void) { return true; }

		bool			check_free_mem([[maybe_unused]] uint32_t msg_count,[[maybe_unused]] bool add) {
			return true;
		}

		bool 			transfer_out_of_tier(void) { return false; }

		void 			move_from_reserve_to_primary(void) { }

		void 			from_reserve([[maybe_unused]] list<LRU_element *> &free_reserve,[[maybe_unused]] uint32_t req_count) {}

		void			return_to_free_mem([[maybe_unused]] LRU_element *el) {}

		uint32_t		timeout_table_evictions([[maybe_unused]] list<uint32_t> &moving,[[maybe_unused]] uint32_t req_count) { return 0; }

		void 			claim_hashes([[maybe_unused]] list<uint32_t> &moving) {}

		void			relinquish_hashes([[maybe_unused]] list<uint32_t> &moving) {}

		bool			add_key_value([[maybe_unused]] uint64_t hash,[[maybe_unused]] uint32_t offset) {
			return true; // faux success
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
		uint8_t							*_reserve_start;
		uint8_t							*_reserve_end;
		uint16_t						_reserve_count_free;
		

		uint16_t						_count_free;
		uint16_t						_count;
		uint16_t						_max_count;  // max possible number of records

		//
		uint8_t							_Tier;
		uint8_t							_reserve_percent;

 		HMap_interface 					*_hmap_i[2];

		//
		atomic<uint32_t>				*_lb_time;
		atomic<uint32_t>				*_ub_time;
		Com_element						*_cascaded_com_area;   // if outsourcing tiers ....
		Com_element						*_end_cascaded_com_area;

};


#endif  // _H_HOPSCOTCH_HASH_LRU_