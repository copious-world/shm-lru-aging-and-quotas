#pragma once

#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>

#include <map>
#include <unordered_map>
#include <queue>
#include <list>
#include <chrono>
#include <atomic>
#include <tuple>


using namespace std;

#include "random_selector.h"
#include "atomic_proc_rw_state.h"
#include "holey_buffer.h"

#include "worker_waiters.h"

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


inline uint32_t now(void) {
	uint32_t ms;
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



// ----

typedef enum STP_table_choice {
	STP_TABLE_HH,
	STP_TABLE_SLABS,
	STP_TABLE_QUEUED,
	STP_TABLE_INTERNAL_ONLY,
	STP_TABLE_EMPTY_SHELL_TEST
} stp_table_choice;

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



typedef struct _LRU_Alloc_Sections_and_Threads_ {
	
	bool			_run_random_upates_threads{false};
	bool			_run_restore_threads{false};
	bool			_run_cropper_threads{false};
	bool			_test_no_run_evictor_threads{false};
	//
	bool			_alloc_randoms;		// do allocation and launch threads
	bool			_alloc_hash_tables;	// do allocation and launch threads
	uint8_t			_num_hash_tables;	// usually one or two (sparse may be more)
	uint8_t			_num_initial_typed_slab{0};	// usually one or two (sparse may be more)
	//
	uint8_t			_num_tiers;
	//
	bool			_alloc_secondary_com_buffer{false}; // for the queued case

} LRU_Alloc_Sections_and_Threads;


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



		/**
		 * check_expected_lru_region_size
		*/

		static uint32_t check_expected_lru_region_size(size_t record_size, size_t els_per_tier, uint32_t num_procs) {
			//
			size_t this_tier_atomics_sz = LRU_ATOMIC_HEADER_WORDS*sizeof(atomic<uint32_t>);  // currently 5 accessed
			size_t com_reader_per_proc_sz = sizeof(Com_element)*num_procs;
			size_t max_count_lru_regions_sz = (sizeof(LRU_element) + record_size)*(els_per_tier + 2);
			// _max_count*2 + num_procs
			size_t holey_buffer_sz = Shared_KeyValueManager::check_expected_holey_buffer_size(els_per_tier,num_procs); // storage for timeout management

			uint32_t predict = (this_tier_atomics_sz + com_reader_per_proc_sz + max_count_lru_regions_sz + holey_buffer_sz);
			//
			return predict;
		}


		static uint32_t check_expected_com_size(uint32_t num_procs, uint32_t num_tiers) {
			size_t tier_atomics_sz = NUM_ATOMIC_OPS*num_tiers*sizeof(atomic_flag *);  // ref to the atomic flag
			size_t proc_tier_com_sz = sizeof(Com_element)*num_procs*num_tiers;
			uint32_t seg_size = tier_atomics_sz + proc_tier_com_sz;
			return seg_size;
		}

};




class LocalTimeManager {
	public:

		LocalTimeManager() {}
		virtual ~LocalTimeManager(void) {}


		static const uint8_t atomic_region_size = NUM_H_BUFFER_ATOMICS*sizeof(atomic<uint32_t>);


	public:

		void init([[maybe_unused]] atomic<uint32_t> *start_atoms, [[maybe_unused]] pair<uint32_t,uint32_t> *primary_storage,
									[[maybe_unused]] uint32_t count_size, [[maybe_unused]] pair<uint32_t,uint32_t> *shared_queue, [[maybe_unused]] uint16_t expected_proc_max) {
		}

		inline bool add_entries([[maybe_unused]] uint32_t *lru_element_offsets,[[maybe_unused]] uint32_t *entry_times,[[maybe_unused]] uint32_t ready_msg_count) {

			for ( uint32_t i = 0; i < ready_msg_count; i++ ) {
				uint32_t value = lru_element_offsets[i];
				uint32_t key = entry_times[i];
				pair<uint32_t,uint32_t> p(key,value);
				_timer_queue.insert(p);  //insert(key,value);
			}

			return true;
		}

		inline bool update_entries([[maybe_unused]] uint32_t *old_times,[[maybe_unused]] uint32_t *new_times,[[maybe_unused]] uint8_t count_updates) {
			for ( uint32_t i = 0; i < count_updates; i++ ) {
				uint32_t old_ky = old_times[i];
				uint32_t new_ky = new_times[i];
				auto bgnd = _timer_queue.equal_range(old_ky);
				for ( auto kv = bgnd.first; kv != bgnd.second; kv++ ) {
					auto value = kv->second;
					pair<uint32_t,uint32_t> p(new_ky,value);
					_timer_queue.insert(p);  //insert(new_ky,value);
					uint32_t old_ky_chk = old_times[i+1];
					uint32_t new_ky_chk = new_times[i+1];
					if ( old_ky_chk != old_ky ) break;
					i++;
					new_ky = new_ky_chk;
				}
			}
			return true;
		}

		void displace_lowest_value_threshold([[maybe_unused]] list<uint32_t> &deposit, [[maybe_unused]] uint32_t min_max, [[maybe_unused]] uint32_t max_count) {
			auto itr = _timer_queue.begin();
			while ( itr != _timer_queue.end() && (max_count != 0 )) {
				auto tm = itr->first;
				if ( tm < min_max ) {
					deposit.push_back(itr->second);
					itr++;
					max_count--;
				} else break;
			}
			_timer_queue.erase(_timer_queue.begin(),--itr);
		}

		inline bool remove_entry([[maybe_unused]] uint32_t key) {
			_timer_queue.erase(key);
			return true;
		}

		uint32_t least_time_key(void) {
			auto least = _timer_queue.begin();
			return least->first;
		}

	public:

		multimap<uint32_t,uint32_t>			_timer_queue;

};



/**
 * LRU_time_bounds
 */

template<class TimeManager = Shared_KeyValueManager>
class LRU_time_bounds {

	public:

		LRU_time_bounds(void) {}
		virtual ~LRU_time_bounds(void) {}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void init(atomic<uint32_t> *atoms,atomic<uint32_t> *start_atoms, pair<uint32_t,uint32_t> *primary_storage,
							uint32_t count_size, pair<uint32_t,uint32_t> *shared_queue, uint16_t expected_proc_max, uint8_t tier) {
			_timeout_table = new TimeManager();
			_lb_time = atoms;
			_ub_time = atoms + 1;
			if ( tier == 0 ) {
				_lb_time->store(now()-1);
			} else {
				_lb_time->store(UINT32_MAX);
			}
			_ub_time->store(UINT32_MAX);
			//
			_timeout_table->init(start_atoms, primary_storage, count_size, shared_queue, expected_proc_max);
		}

	public:

		inline bool add_entries(uint32_t *lru_element_offsets,uint32_t *entry_times,uint32_t ready_msg_count) {
			return _timeout_table->add_entries(lru_element_offsets,entry_times,ready_msg_count);
		}

		inline bool update_entries(uint32_t *old_times,uint32_t *new_times,uint8_t count_updates) {
			return _timeout_table->update_entries(old_times,new_times,count_updates);
		}

		inline void displace_lowest_value_threshold(list<uint32_t> &deposit, uint32_t min_max, uint32_t max_count) {
			_timeout_table->displace_lowest_value_threshold(deposit,min_max,max_count);
		}

		inline bool remove_entry(uint32_t key){
			return _timeout_table->remove_entry(key);
		}

		inline uint32_t least_time_key(void) {
			return  _timeout_table->least_time_key();
		}

	public:

 		TimeManager				*_timeout_table;
		atomic<uint32_t>		*_lb_time;
		atomic<uint32_t>		*_ub_time;

};



#define TESTING_SKIP_TABLE 1

