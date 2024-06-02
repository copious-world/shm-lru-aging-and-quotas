#ifndef _H_HOPSCOTCH_HASH_SHM_
#define _H_HOPSCOTCH_HASH_SHM_

#pragma once

#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>
#include <thread>
#include <ctime>
#include <atomic>
#include <thread>
#include <mutex>

#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <deque>
#include <chrono>


#include "hmap_interface.h"
#include "random_selector.h"
#include "entry_holder.h"
#include "thread_activity.h"



#define MAX_THREADS (64)


#define _DEBUG_TESTING_ 1
/*
static const std::size_t S_CACHE_PADDING = 128;
static const std::size_t S_CACHE_SIZE = 64;
std::uint8_t padding[S_CACHE_PADDING - (sizeof(Object) % S_CACHE_PADDING)];
*/



// USING
using namespace std;
// 

// Bringing in code from libhhash  // until further changes...
template<typename T>
struct KEY_VALUE {
	T			value;
	T			key;
};


template<typename T>
struct BITS_KEY {
	T			bits;
	T			key;
};

typedef struct BITS_KEY<uint32_t> cbits_key;


template<typename T>
struct MEM_VALUE {
	T			taken;
	T			value;
};

typedef struct MEM_VALUE<uint32_t> taken_values;


typedef struct HH_element {
	cbits_key			c;
	taken_values		tv;
} hh_element;


typedef enum {
	HH_FROM_EMPTY,
	HH_FROM_BASE,
	HH_FROM_BASE_AND_WAIT,
	HH_FROM_BASE_USURP,
	HH_FROM_USURP_AND_WAIT,
	HH_ADDER_STATES
} hh_adder_states;


/*
	QueueEntryHolder ...
*/

/** 
 * q_entry is struct Q_ENTRY
*/
typedef struct Q_ENTRY {
	public:
		//
		hh_element 		*hash_ref;
		uint32_t 		h_bucket;
		uint64_t 		loaded_value;
		hh_element		*buffer;
		hh_element		*end;
		uint8_t			hole;
		uint8_t			which_table;
		uint8_t			thread_id;
		hh_adder_states	update_type;
		//
} q_entry;



/** 
 * q_entry is struct Q_ENTRY
*/
typedef struct C_ENTRY {
	public:
		//
		hh_element 		*hash_ref;
		hh_element		*buffer;
		hh_element		*end;
		uint32_t 		cbits;
		uint8_t			which_table;
		//
} crop_entry;


/**
 * QueueEntryHolder uses q_entry in SharedQueue_SRSW<q_entry,ExpectedMax>
*/

template<uint16_t const ExpectedMax = 100>
class QueueEntryHolder : public  SharedQueue_SRSW<q_entry,ExpectedMax> {

	bool		compare_key(uint32_t key,q_entry *el,uint32_t &value) {
		auto V = el->loaded_value;
		uint32_t v = (uint32_t)(V & UINT32_MAX);
		return (v == key);
	}

};



/**
 * QueueEntryHolder uses q_entry in SharedQueue_SRSW<q_entry,ExpectedMax>
*/

template<uint16_t const ExpectedMax = 64>
class CropEntryHolder : public  SharedQueue_SRSW<crop_entry,ExpectedMax> {

};




typedef struct PRODUCT_DESCR {
	//
	uint32_t						partner_thread;
	uint32_t						stats;
	ThreadActivity::ta_ops			op;
	QueueEntryHolder<>				_process_queue[2];
	CropEntryHolder<>				_to_cropping[2];

} proc_descr;





inline uint8_t get_b_offset_update(uint32_t &c) {
	uint8_t offset = countr_zero(c);
	c = c & (~((uint32_t)0x1 << offset));
	return offset;
}

inline uint8_t search_range(uint32_t c) {
	uint8_t max = countl_zero(c);
	return max;
}


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static constexpr uint32_t zero_levels[32] {
	~(0xFFFFFFFF << 1), ~(0xFFFFFFFF << 2), ~(0xFFFFFFFF << 3), ~(0xFFFFFFFF << 4), ~(0xFFFFFFFF << 5),
	~(0xFFFFFFFF << 6), ~(0xFFFFFFFF << 7), ~(0xFFFFFFFF << 8), ~(0xFFFFFFFF << 9), ~(0xFFFFFFFF << 10),
	~(0xFFFFFFFF << 11),~(0xFFFFFFFF << 12),~(0xFFFFFFFF << 13),~(0xFFFFFFFF << 14),~(0xFFFFFFFF << 15),
	~(0xFFFFFFFF << 16),~(0xFFFFFFFF << 17),~(0xFFFFFFFF << 18),~(0xFFFFFFFF << 19),~(0xFFFFFFFF << 20),
	~(0xFFFFFFFF << 21),~(0xFFFFFFFF << 22),~(0xFFFFFFFF << 23),~(0xFFFFFFFF << 24),~(0xFFFFFFFF << 25),
	~(0xFFFFFFFF << 26),~(0xFFFFFFFF << 27),~(0xFFFFFFFF << 28),~(0xFFFFFFFF << 29),~(0xFFFFFFFF << 30),
	~(0xFFFFFFFF << 31), 0xFFFFFFFF
};


static constexpr uint32_t one_levels[32] {
	(0xFFFFFFFF << 1), (0xFFFFFFFF << 2), (0xFFFFFFFF << 3), (0xFFFFFFFF << 4), (0xFFFFFFFF << 5),
	(0xFFFFFFFF << 6), (0xFFFFFFFF << 7), (0xFFFFFFFF << 8), (0xFFFFFFFF << 9), (0xFFFFFFFF << 10),
	(0xFFFFFFFF << 11),(0xFFFFFFFF << 12),(0xFFFFFFFF << 13),(0xFFFFFFFF << 14),(0xFFFFFFFF << 15),
	(0xFFFFFFFF << 16),(0xFFFFFFFF << 17),(0xFFFFFFFF << 18),(0xFFFFFFFF << 19),(0xFFFFFFFF << 20),
	(0xFFFFFFFF << 21),(0xFFFFFFFF << 22),(0xFFFFFFFF << 23),(0xFFFFFFFF << 24),(0xFFFFFFFF << 25),
	(0xFFFFFFFF << 26),(0xFFFFFFFF << 27),(0xFFFFFFFF << 28),(0xFFFFFFFF << 29),(0xFFFFFFFF << 30),
	(0xFFFFFFFF << 31), 0
};

//
static uint32_t zero_above(uint8_t hole) {
	if ( hole >= 31 ) {
		return  0xFFFFFFFF;
	}
	return zero_levels[hole];
}


		//
static uint32_t ones_above(uint8_t hole) {
	if ( hole >= 31 ) {
		return  0;
	}
	return one_levels[hole];
}



/**
 * el_check_end
*/

static inline hh_element *el_check_end(hh_element *ptr, hh_element *buffer, hh_element *end) {
	if ( ptr >= end ) return buffer;
	return ptr;
}


/**
 * el_check_beg_wrap
*/

static inline hh_element *el_check_beg_wrap(hh_element *ptr, hh_element *buffer, hh_element *end) {
	if ( ptr < buffer ) return (end - buffer + ptr);
	return ptr;
}


/**
 * el_check_end_wrap
*/

static inline hh_element *el_check_end_wrap(hh_element *ptr, hh_element *buffer, hh_element *end) {
	if ( ptr >= end ) {
		uint32_t diff = (ptr - end);
		return buffer + diff;
	}
	return ptr;
}


/**
 * stamp_offset
*/

static inline uint32_t stamp_offset(uint32_t time,[[maybe_unused]]uint8_t offset) {
	return time;
}




typedef enum {
	HH_OP_NOOP,
	HH_OP_CREATION,
	HH_OP_USURP,
	HH_OP_MEMBER_IN,
	HH_OP_MEMBER_OUT,
	HH_ALL_OPS
} hh_creation_ops;


// ---- ---- ---- ---- ---- ----  HHash
// HHash <- HHASH



template<const uint32_t NEIGHBORHOOD = 32>
class HH_map_structure : public HMap_interface, public Random_bits_generator<> {

	public:

		// LRU_cache -- constructor
		HH_map_structure(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) {
			_reason = "OK";
			//
			_region = region;
			_endof_region = _region + seg_sz;
			//
			_num_threads = num_threads;
			_sleeping_reclaimer.clear();  // atomic that pauses the relcaimer thread until set.
			_sleeping_cropper.clear();
			//
			_status = true;
			_initializer = am_initializer;
			_max_count = max_element_count;
			//
			uint8_t sz = sizeof(HHash);
			uint8_t header_size = (sz  + (sz % sizeof(uint64_t)));
			//
			// initialize from constructor
			this->setup_region(am_initializer,header_size,(max_element_count/2),num_threads);
			//
			_random_gen_region->store(0);

		}


		virtual ~HH_map_structure() {
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// REGIONS...

		/**
		 * setup_region -- part of initialization if the process is the intiator..
		 * -- header_size --> HHash
		 *  the regions are setup as, [values 1][buckets 1][values 2][buckets 2][controls 1 and 2]
		*/
		void setup_region(bool am_initializer,uint8_t header_size,uint32_t max_count,uint32_t num_threads) {
			// ----
			uint8_t *start = _region;

			_rand_gen_thread_waiting_spinner = (atomic_flag *)start;
			_random_share_lock = (atomic_flag *)(_rand_gen_thread_waiting_spinner + 1);
			_random_gen_region = (atomic<uint32_t> *)(_random_share_lock + 1);

			_rand_gen_thread_waiting_spinner->clear();
			_random_share_lock->clear();

			// start is now passed the atomics...
			start = (uint8_t *)(_random_gen_region + 1);

			HHash *T = (HHash *)(start);

			//
			this->_max_n = max_count;

			//
			_T0 = T;   // keep the reference handy
			//
			uint32_t vh_region_size = (sizeof(hh_element)*max_count);
			uint32_t c_regions_size = (sizeof(uint32_t)*max_count);

			auto hv_offset_past_header_from_start = header_size;
			//
			auto next_hh_offset = (hv_offset_past_header_from_start + vh_region_size);  // now, the next array of buckets and values (controls are the very end for both)
			auto c_offset = next_hh_offset;  // from the second hh region start == start2
			//

			// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

			T->_HV_Offset = hv_offset_past_header_from_start;
			T->_C_Offset = c_offset*2;   // from start

			// # 1
			// ----
			if ( am_initializer ) {
				T->_count = 0;
				T->_max_n = max_count;
				T->_neighbor = NEIGHBORHOOD;
				T->_control_bits = 0;
			} else {
				max_count = T->_max_n;	// just in case
			}

			// # 1
			//
			_region_HV_0 = (hh_element *)(start + hv_offset_past_header_from_start);  // start on word boundary
			_region_HV_0_end = _region_HV_0 + max_count;

			//
			if ( am_initializer ) {
				if ( check_end((uint8_t *)_region_HV_0) && check_end((uint8_t *)_region_HV_0_end) ) {
					memset((void *)(_region_HV_0),0,vh_region_size);
				} else {
					throw "hh_map (1) sizes overrun allocated region determined by region_sz";
				}
			}

			// # 2
			// ----
			T = (HHash *)(start + next_hh_offset);
			_T1 = T;   // keep the reference handy

			if ( am_initializer ) {
				T->_count = 0;
				T->_max_n = max_count;
				T->_neighbor = NEIGHBORHOOD;
				T->_control_bits = 1;
			} else {
				max_count = T->_max_n;	// just in case
			}

			// # 2
			//
			_region_HV_1 = (hh_element *)(_region_HV_0_end);  // start on word boundary
			_region_HV_1_end = _region_HV_1 + max_count;

			auto hv_offset_2 = next_hh_offset + header_size;

			T->_HV_Offset = hv_offset_2;  // from start
			T->_C_Offset = hv_offset_2 + c_offset;  // from start
			//
			//
			_cbits_temporary_store = (uint32_t *)(_region_HV_1_end);  // these start at the same place offset from start of second table
			_end_cbits_temporary_store = _cbits_temporary_store + _num_threads;  // *sizeof(uint32_t)
			//
			//
			_tbits_temporary_store = (uint32_t *)(_end_cbits_temporary_store);  // these start at the same place offset from start of second table
			_end_tbits_temporary_store = _tbits_temporary_store + _num_threads;  // *sizeof(uint32_t)
			//
			_key_value_temporary_store = (pair<uint32_t,uint32_t>s *)(_end_tbits_temporary_store);  // these start at the same place offset from start of second table
			_end_key_value_temporary_store = _key_value_temporary_store + _num_threads;  // *sizeof(uint32_t)


// _tbits_temporary_store



		// threads ...
			auto proc_regions_size = num_threads*sizeof(proc_descr);
			_process_table = (proc_descr *)(_end_cbits_temporary_store);
			_end_procs = _process_table + num_threads;

			//
			if ( am_initializer ) {
				//
				if ( check_end((uint8_t *)_region_HV_1) && check_end((uint8_t *)_region_HV_1_end) ) {
					memset((void *)(_region_HV_1),0,vh_region_size);
				} else {
					throw "hh_map (2) sizes overrun allocated region determined by region_sz";
				}
				// one 16 bit word for two counters
				if ( check_end((uint8_t *)_cbits_temporary_store) && check_end((uint8_t *)_end_cbits_temporary_store) ) {
					memset((void *)(_cbits_temporary_store), 0, c_regions_size);
				} else {
					cout << "WRONG HERE: " << ((uint8_t *)_end_cbits_temporary_store - _endof_region) << endl;
					throw "hh_map (3) sizes overrun allocated region determined by region_sz";
				}
				// two 32 bit words for processes/threads
				if ( check_end((uint8_t *)_process_table) && check_end((uint8_t *)_end_procs,true) ) {
					memset((void *)(_process_table), 0, proc_regions_size);
				} else {
					throw "hh_map (4) sizes overrun allocated region determined by region_sz";
				}
			}
			//
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * check_expected_hh_region_size
		*/


		static uint32_t check_expected_hh_region_size(uint32_t els_per_tier, uint32_t num_threads) {

			uint8_t atomics_size = 2*sizeof(atomic_flag) + sizeof(atomic<uint32_t>);
			//
			uint8_t sz = sizeof(HHash);
			uint8_t header_size = (sz  + (sz % sizeof(uint64_t)));
			
			auto max_count = els_per_tier/2;
			//
			uint32_t vh_region_size = (sizeof(hh_element)*max_count);
			uint32_t c_regions_size = (sizeof(uint32_t)*max_count);
			//
			uint32_t hv_offset_1 = header_size;
			uint32_t next_hh_offset = (hv_offset_1 + vh_region_size);  // now, the next array of buckets and values (controls are the very end for both)
			uint32_t proc_region_size = num_threads*sizeof(proc_descr);
			//
			uint32_t predict = atomics_size + next_hh_offset*2 + c_regions_size + proc_region_size;
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



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * bucket_at 
		 * 
		 * bucket_at(buffer, h_start) -- does not check wraop
		 * bucket_at(buffer, h_start, end)  -- check wrap
		*/

		inline hh_element *bucket_at(hh_element *buffer,uint32_t h_start) {
			return (hh_element *)(buffer) + h_start;
		}

		inline hh_element *bucket_at(hh_element *buffer,uint32_t h_start,hh_element *end) {
			hh_element *el = buffer + h_start;
			el =  el_check_end_wrap(el,buffer,end);
			return el;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * share_lock -- implement Random_bits_generator locks
		 * 
		 * override the lock for shared random bits (in parent this is a no op)
		 * 
		 * This version of critical region protection may not be needed if only one thread ever
		 * fills up the random buffers for other the whole system. However, the region being regenerated
		 * may be read by other threads. Yet, it may be possible to ensure that the region being filled is
		 * managed ahead of any reads first by generating a region as soon as is used up. Also, it is expected
		 * that the usage of a region will be improbible enough that buffer generation will always finished before the currently 
		 * available buffer is used up.
		*/
		void share_lock(void) {
#ifndef __APPLE__
				while ( _random_share_lock->test() ) {  // if not cleared, then wait
					_random_share_lock->wait(true);
				};
				while ( !_random_share_lock->test_and_set() );
#else
				while ( _random_share_lock->test_and_set() ) {
					thread_sleep(10);
				};
#endif			
		}


		/**
		 * share_unlock 
		 * 
		 * override the lock for shared random bits (in parent this is a no op)
		*/
		void share_unlock(void) {
#ifndef __APPLE__
			while ( _random_share_lock->test() ) {
				_random_share_lock->clear();
			};
			_random_share_lock->notify_one();
#else
			while ( _random_share_lock->test() ) {   // make sure it clears
				_random_share_lock->clear();
			};
#endif
		}


		/**
		 * random_generator_thread_runner 
		 * 
		 * override the lock for shared random bits (in parent this is a no op)
		*/

		void random_generator_thread_runner() {
			while ( true ) {
#ifndef __APPLE__
				do {
					_rand_gen_thread_waiting_spinner->wait(false);
				} while ( !_rand_gen_thread_waiting_spinner->test() );
#else
				while ( _rand_gen_thread_waiting_spinner->test_and_set() ) {
					thread_sleep(10);
				}
#endif
				bool which_region = _random_gen_region->load(std::memory_order_acquire);
				this->regenerate_shared(which_region);		
			}
		}



		/**
		 * wakeup_random_generator
		*/
		void wakeup_random_generator(uint8_t which_region) {   //
			//
			_random_gen_region->store(which_region);
#ifndef __APPLE__
			while ( !(_rand_gen_thread_waiting_spinner->test_and_set()) );
			_rand_gen_thread_waiting_spinner->notify_one();
#else
			_rand_gen_thread_waiting_spinner->clear(std::memory_order_release);
#endif
		}




		/**
		 * clear
		*/
		void clear(void) {
			if ( _initializer ) {
				uint8_t sz = sizeof(HHash);
				uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
				this->setup_region(_initializer,header_size,_max_count,_num_threads);
			}
		}



	public:

		// ---- ---- ---- ---- ---- ---- ----
		//
		bool							_status;
		const char 						*_reason;
		//
		bool							_initializer;
		uint32_t						_max_count;
		uint32_t						_max_n;   // max_count/2 ... the size of a slice.
		uint32_t						_num_threads;
		//
		uint8_t		 					*_region;
		uint8_t		 					*_endof_region;
		//
		HHash							*_T0;
		HHash							*_T1;
		hh_element		 				*_region_HV_0;
		hh_element		 				*_region_HV_1;
		hh_element		 				*_region_HV_0_end;
		hh_element		 				*_region_HV_1_end;
		//
		// ---- These two regions are interleaved in reality
		uint32_t 						*_cbits_temporary_store;    // a shared structure
		uint32_t		 				*_end_cbits_temporary_store;
		//
		uint32_t 						*_tbits_temporary_store;    // a shared structure
		uint32_t		 				*_end_tbits_temporary_store;
		// 
		pair<uint32_t,uint32_t>			*_key_value_temporary_store;    // a shared structure
		pair<uint32_t,uint32_t>		 	*_end_key_value_temporary_store;
		
		// threads ...
		proc_descr						*_process_table;						
		proc_descr						*_end_procs;
		uint8_t							_round_robbin_proc_table_threads{0};

		atomic<uint32_t>				*_random_gen_region;

};







template<const uint32_t NEIGHBORHOOD = 32>
class HH_map_atomic_no_wait : public HH_map_structure<NEIGHBORHOOD> {
	public:

		// LRU_cache -- constructor
		HH_map_atomic_no_wait(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) :
		 								HH_map_structure<NEIGHBORHOOD>(region, seg_sz, max_element_count, num_threads, am_initializer) {
		}

		virtual ~HH_map_atomic_no_wait() {
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// C BITS

		uint32_t fetch_real_cbits(uint32_t cbit_carrier) {
			auto thrd = cbits_thread_id_of(cbit_carrier);
			return _cbits_temporary_store[thrd];
		}


		void store_real_cbits(uint32_t cbits,uint8_t thrd) {
			_cbits_temporary_store[thrd] = cbits;
		}


		// T BITS

		uint32_t fetch_real_tbits(uint32_t tbit_carrier) {
			auto thrd = cbits_thread_id_of(tbit_carrier);
			return _cbits_temporary_store[thrd];
		}


		void store_real_tbits(uint32_t tbits,uint8_t thrd) {
			_tbits_temporary_store[thrd] = tbits;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void stash_key_value(uint32_t key,uint32_t value,uint8_t thread_id) {
			_key_value_temporary_store[thread_id].first = key;
			_key_value_temporary_store[thread_id].second = value;
		}

		void erase_stashed_key_value(uint8_t thread_id) {
			_key_value_temporary_store[thread_id].first = UINT32_MAX;
			_key_value_temporary_store[thread_id].second = 0;
		}

		uint32_t check_key_value(uint32_t key,uint8_t thread_id) {
			if ( _key_value_temporary_store[thread_id].first == key ) {
				return _key_value_temporary_store[thread_id].second;
			}
			return UINT32_MAX;
		}

		
		uint32_t search_key_value(uint32_t key) {
			for ( uint8_t thread_id = 0; thread_id < _num_threads; thread_id++ ) {
				if ( _key_value_temporary_store[thread_id].first == key ) {
					return _key_value_temporary_store[thread_id].second;
				}
			}
			return UINT32_MAX;
		}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		atomic<uint32_t>  *load_stable_key(hh_element *bucket,uint32_t &save_key) {
			atomic<uint32_t>  *stable_key = (atomic<uint32_t>  *)bucket->c.key);
			save_key = stable_key->load(std::memory_order_acquire);
			return stable_key;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * empty_bucket
		 * 
		*/

		inline bool empty_bucket(atomic<uint32_t> *a_c,hh_element *base,uint32_t &cbits) {
			cbits = a_c->load(std::memory_order_acquire);
			if ( is_empty_bucket(cbits) ) return true;  // no members and no back ref either (while this test for backref may be uncommon)
			if ( !(is_base_noop(cbits)) && (is_member_in_mobile_predelete(cbits) || is_deleted(cbits)) ) return true;
			if ( is_base_noop(cbits) && (base->tv.value == 0) ) return true;
			return false;
		}


		/**
		 * clear_bucket
		 * 
		*/

		inline void clear_bucket(hh_element *base) {   // called under lock
			atomic<uint32_t> *a_c = (atomic<uint32_t> *)(&base->c.bits);
			a_c->store(0,std::memory_order_release);
			atomic<uint32_t> *a_t = (atomic<uint32_t> *)(&base->tv.taken);
			a_t->store(0,std::memory_order_release);
			base->c.key = 0;
			base->->tv.value = 0;
		}
 
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * is_base_bucket_cbits
		 * 
		*/

		bool is_base_bucket_cbits(uint32_t cbits) {
			if ( is_base_noop(cbits) ) return true;
			if ( base_in_operation(cbits) ) return true;
			return false;
		}

		/**
		 * bucket_is_base
		 * 
		*/
		bool bucket_is_base(hh_element *hash_ref) {
			atomic<uint32_t>  *a_c_bits = (atomic<uint32_t>  *)hash_ref->c.bits);
			auto cbits = a_c_bits->load(std::memory_order_acquire);
			return is_base_bucket_cbits(cbits);
		}


		/**
		 * bucket_is_base
		 * 
		*/
		bool bucket_is_base(hh_element *hash_ref,uint32_t &cbits) {
			atomic<uint32_t>  *a_c_bits = (atomic<uint32_t>  *)hash_ref->c.bits);
			cbits = a_c_bits->load(std::memory_order_acquire);
			if ( is_base_bucket_cbits(cbits) ) {
				if ( !(is_base_noop(cbits)) ) {
					cbits = fetch_real_cbits(cbits);
				}
				return true;
			}
			return false;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * tbits_add_reader
		 * 
		 * 
		 * When reading, readers will move passed the base quickly, but they should put themselve into a semaphore
		 * designed to keep trailing editors in a wait mode while allowing current editors to finish 
		 * cooperatively with the readers. 
		 * 
		 * The readers will require fast access to the cbits of the base. They will slow down only if an editor 
		 * has taken ownership of the base. But, editors will need to check the reader semaphore especially when 
		 * preparing to update the tbits. If the tbits are not to be touched, they may be used to mark reading 
		 * and keep a semaphore. The last readers will have to restore the tbits and allow editors to change it.
		 * 
		 * 
		*/

		uint32_t tbits_add_reader(hh_element *base, uint8_t thread_id, uint32_t original_tbits = 0) {
			//
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(base->tv.taken);
			//
			auto tbits = (original_tbits == 0) ? a_tbits->load(std::memory_order_acquire) : original_tbits;
			if ( tbits == 0 ) return 0;
			auto reader_tbits = tbits;  // may already be reader tbits with a positive semaphore...
			if ( tbits & 0x1 ) {
				reader_tbits = store_real_tbits(a_tbits,tbits,thread_id);
			}
			//
			if ( !(tbits_sem_at_max(reader_tbits)) ) {
				a_tbits->fetch_add(2,std::memory_order_acq_rel);
			}
			//
			return tbits;
		}



		/**
		 * tbits_remove_reader
		*/

		void tbits_remove_reader(atomic<uint32_t> *a_tbits, uint8_t thread_id) {
			//
			auto tbits = a_tbits->load(std::memory_order_acquire);
			//
			auto reader_tbits = tbits;
			if ( tbits & 0x1 ) {
				return;  // the tbits are where they are supposed to be
			}
			//
			if ( !(tbits_sem_at_zero(tbits)) ) {
				a_tbits->fetch_sub(2,std::memory_order_acq_rel);
			}
			//
			if ( tbits_sem_at_zero(tbits - 2) ) {
				tbits = fetch_real_tbits(reader_cbits);
				auto expect_bits = tbits - 2;
				while ( !(a_tbits->compare_exchange_weak(expect_bits,tbits,2,std::memory_order_acq_rel)) ) {
					if ( expect_bits & 0x1 ) return; // if there was a change, the pattern was updated.
				}
			}
			//
			return;
		}


		/**
		 * tbits_wait_for_readers
		*/

		void tbits_wait_for_readers(atomic<uint32_t> *a_tbits) {
			auto tbits = a_tbits->load(std::memory_order_acquire);
			while ( base_reader_sem_count(tbits) > 0 ) {
				tick();
			}
		}



		/**
		 * become_bucket_state_master
		 * 
		 * must preserve reader semaphore if it is in use
		 * 
		*/

		bool become_bucket_state_master(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t locking_bits, uint8_t thread_id) {
			//
			if ( control_bits->compare_exchange_weak(cbits,locking_bits,std::memory_order_acq_rel) ) {
				cbits = control_bits->load(std::memory_order_acquire);
				if ( cbits != locking_bits ) return false;
				store_real_cbits(0x1,thread_id);
			}
			//
			return true;
		}




	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		virtual void wakeup_value_restore(hh_adder_states update_type,hh_element *hash_ref, uint32_t h_start, uint64_t loaded_value, uint8_t hole, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end) = 0;


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		bool no_cbit_change(atomic<uint32_t> *base_bits,uint32_t original_bits) {
			auto current_bits = base_bits->load(std::memory_order_acquire);
			return ( current_bits == original_bits );
		}


		void install_base_cbit_update(hh_element *cells_base,uint32_t &update_base_cbits,uint32_t &base_taken_bits) {

		}




		/**
		 * _swappy_search_ref
		 * 
		 *	NOTES: By default the base bucket is immobile. It may be read immediately unless it is being deleted.
		 			The base may be owned by an editor. If it is, then `_swappy_search_ref` will be invoked.
					The cbits of the base causing `_swappy_search_ref` to be called will be indicating operation by
					and editor. And, the orginal cbits will be in the `_cbits_temporary_store` indexed by the thread
					carried in the current cbits.

				There are several strategies for delete at the base. 
				One: is to leave the root in editor operation. The reader may complete the swap ahead of the deleter.
				Two: is to leave the root cbits as the membership map. But, examine the key to see if it is max.
					If it is max, then perform the first swap, moving the next element into the root position.
		*/



		hh_element *_swappy_search_ref(uint32_t el_key, hh_element *base, uint32_t c,uint8_t thread_id) {
			hh_element *next = base;
			while ( c ) {
				next = base;
				uint8_t offset = get_b_offset_update(c);
				next += offset;
				next = el_check_end_wrap(next,buffer,end);
				// is this being usurped or swapped or deleted at this moment? Search on this range is locked
				//
				atomic<uint64_t> *a_bits_n_ky = (atomic<uint64_t> *)(&(next->c));
				//
				auto c_n_ky = a_bits_n_ky->load(std::memory_order_acquire);
				uint32_t nxt_bits = (uint32_t)(c_n_ky & UINT32_MAX);
				auto ky = (c_n_ky >> sizeof(uint32_t)) & UINT32_MAX;

				wait_until_immobile(a_bits_n_ky,nxt_bits,thread_id,ky);
				
				//
				if ( (ky == UINT32_MAX) && c ) {  // start a swap if this is not the end
					auto use_next_c = c;
					uint8_t offset_follow = get_b_offset_update(use_next_c);  // chops off the next position
					hh_element *f_next += offset_follow;
					f_next = el_check_end_wrap(f_next,buffer,end);
					//
					atomic<uint64_t> *a_bits_fn_ky = (atomic<uint64_t> *)(&(f_next->c));
					auto c_fn_ky = a_bits_fn_ky->load(std::memory_order_acquire);
					uint32_t f_nxt_bits = (uint32_t)(c_n_ky & UINT32_MAX);
					auto fky = (c_n_ky >> sizeof(uint32_t)) & UINT32_MAX;

					auto value = f_next->->tv.value;
					auto taken = f_next->->tv.taken;
					//
					wait_until_immobile(a_bits_fn_ky,thread_id,fky);

					c_n_ky = (c_n_ky & ((uint64_t)UINT32_MAX << sizeof(uint32_t) )) | f_nxt_bits;
					c_fn_ky = (c_fn_ky & ((uint64_t)UINT32_MAX << sizeof(uint32_t) )) | nxt_bits;
					a_bits_n_ky->store(c_fn_ky);
					a_bits_fn_ky->store(c_n_ky);

					f_next->->tv.value = 0;
					remobalize(f_nexts);

					if ( fky != UINT32_MAX ) {  // move a real value closer
						next->->tv.value = value;
						next->->tv.taken = taken;
						if ( fky == el_key  ) {  // opportunistic reader helps delete with on swap and returns on finding its value
							remobalize(next);
							return next;
						}
					}
					// else resume with the nex position being the hole blocker... 
				} else if ( el_key == ky ) {
					remobalize(next);
					return next;
				}
				remobalize(next);
			}
			return UINT32_MAX;	
		}

		/**
		 * _editor_locked_search_ref
		 * 
		 * If in this method, then the original bits are a membership map.
		 * 
		 * 
		*/

		hh_element *_editor_locked_search_ref(atomic<uint32_t> *base_bits, uint32_t original_bits, hh_element *base, uint32_t el_key, uint8_t thread_id) {
			//
			uint32_t locking_bits = tbits_add_reader(base,thread_id);  // lockout ops that swap elements
			uint32_t H = original_bits;   // cbits are from a call to empty bucket always at this point
			//
			hh_element *next = base;
			do {
				uint32_t c = H;
				while ( c ) {
					next = base;
					uint8_t offset = get_b_offset_update(c);
					next += offset;
					next = el_check_end_wrap(next,buffer,end);
					// is this being usurped or swapped or deleted at this moment? Search on this range is locked
					auto a_n_b_n_key = (atomic<uint64_t> *)(&(next->c));
					auto chk_bits_n_key = a_n_b_n_key->load(std::memory_order_acquire);
					uint32_t n_cbits = (uint32_t)(chk_bits_n_key >> sizeof(uint32_t));
					uint32_t chk_key = ((uint32_t)chk_bits_n_key) & UINT32_MAX;
					if ( is_member_bucket(n_cbits) && !(is_cbits_deleted(n_cbits) || (is_cbits_in_mobile_predelete(uint32_t cbits))) ) {
						if ( el_key == chk_key ) {
							a_n_b_n_key |= (uint64_t)IMMOBILE_CBIT_SET
							tbits_remove_reader(base_bits);
							return next;
						}
					}
				}
				if ( no_cbit_change(base_bits,original_bits) ) {
					break;
				} else {
					if ( empty_bucket(a_c_bits,next,cbits) ) {
						tbits_remove_reader(base_bits);
						break;
					}
					H = is_base_noop(cbits) ? cbits : fetch_real_cbits(cbits);
				}
			} while ( true );  // if it changes try again 
			return nullptr;
		}



		hh_element *_get_bucket_reference(atomic<uint32_t> *bucket_bits, hh_element *base, uint32_t cbits, uint32_t h_bucket, uint32_t el_key, hh_element *buffer, hh_element *end,uint8_t thread_id) {
			//
			if ( el_key == UINT32_MAX ) return nullptr; // this should be worked out earlier but for completeness allow this check
			//
			// bucket_bits are the base of the bucket storing the state of cbits
			//
			if ( is_base_noop(cbits) ) {
											// no one is editing this bucket, make it exclusive to readers 
										  	// and don't make the readers do work on behalf of anyone
				return _editor_locked_search_ref(bucket_bits,cbits,base,el_key,thread_id);
			} else {
				auto original_cbits = fetch_real_cbits(cbits);   // Since the base is in operation, the orginal bits will be in the `_cbits_temporary_store`.
				auto ref = _swappy_search_ref(el_key, base, original_cbits,thread_id);
				return ref;
			}
			return nullptr;
		}



		/**
		 * _first_level_bucket_ops
		 * 
		*/

		uint64_t _first_level_bucket_ops(atomic<uint32_t> *control_bits,uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table, uint8_t thread_id, uint32_t cbits,hh_element *bucket,hh_element *buffer,hh_element *end_buffer) {
			//
			auto prev_bits = cbits;
			do {
				prev_bits = cbits;
				//
				auto locking_bits = gen_bits_editor_active(thread_id);   // editor active
				//
				if ( is_empty_bucket(cbits) ) {   // this happens if the bucket is completetly empty and not a member of any other bucket
					// if this fails to stay zero, then either another a hash collision has immediately taken place or another
					// bucket created a member.
					cbits = 0x1;
					if ( become_bucket_state_master(control_bits,cbits,locking_bits,thread_id) ) { // OWN THIS BUCKET
						//
						bucket->c.key = el_key;
						bucket->->tv.value = offset_value;
						//
						uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
						// -- in this case, `wakeup_value_restore` fixes the taken bits map
						wakeup_value_restore(HH_FROM_EMPTY,control_bits, cbits, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, thread_id, buffer, end);
					}
				} else if ( is_base_bucket_cbits(cbits) ) {  // NEW MEMBER: This is a base bucket (the new element should become a member)
					if ( is_base_noop(cbits) ) { // can capture this bucket to do an insert...
						if ( become_bucket_state_master(control_bits,cbits,locking_bits,thread_id) ) { // OWN THIS BUCKET
							//
							uint32_t tmp_value = bucket->->tv.value;
							uint32_t tmp_key = bucket->c.key;
							stash_key_value(tmp_key,tmp_value,thread_id);
							uint64_t current_loaded_value = (((uint64_t)tmp_key) << HALF) | tmp_value;
							//
							// results in reinsertion and search for a hole
							bucket->c.key = el_key;
							bucket->->tv.value = offset_value;
							wakeup_value_restore(HH_FROM_BASE,control_bits, cbits, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, thread_id, buffer, end);
						}
					} else {  // THIS BUCKET IS ALREADY BEING EDITED -- let the restore thread shift the new value in
						auto current_thread = cbits_thread_id_of(cbits);
						auto current_membership = fetch_real_cbits(cbits);
							// results in first time insertion and search for a hole (could alter time by a small amount to keep a sort)
						uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
						wakeup_value_restore(HH_FROM_BASE_AND_WAIT,control_bits, current_membership, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, current_thread, buffer, end);
					}
				} else {  // this bucket is a member and cbits carries a back_ref. (this should be usurped)
					// usurping gives priortiy to an entry whose hash target a bucket absolutely
					locking_bits = gen_bitsmember_usurped(thread_id,locking_bits);
					if ( become_bucket_state_master(control_bits,cbits,locking_bits,thread_id) ) { // OWN THIS BUCKET
						//
						uint8_t op_thread_id = 0;
						if ( is_member_usurped(cbits,op_thread_id) )  {  // attempting the same operation 
							uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
							auto current_membership = fetch_real_cbits(cbits);
							// wait for the bucket to become a base and then shift into the bucket 
							wakeup_value_restore(HH_FROM_USURP_AND_WAIT,control_bits, current_membership, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, op_thread_id, buffer, end);
						} else if ( is_member_in_mobile_predelete(cbits) || is_deleted(cbits) ) {
							// bucket is being abandoned -- store the value and lock out other editors -- 
							// no need to usurp... 
							// but, be careful to keep the taken maps and membership maps consistent
							bucket->c.key = el_key;
							bucket->->tv.value = offset_value;
							//
							uint8_t backref = 0;
							uint32_t base_cbits = 0;
							uint32_t base_taken_bits = 0;
							hh_element *cells_base = cbits_base_from_backref(backref,bucket,buffer,end_buffer,base_cbits,base_taken_bits);
							//
							auto update_base_cbits = clear_cbit_member(base_cbits,backref);  // perform delete op if not done already
							if ( update_base_cbits != base_cbits ) {
								base_taken_bits = base_taken_bits | (0x1 << backref);
								install_base_cbit_update(cells_base,update_base_cbits,base_taken_bits);  // rewrite base bits and taken bits
							}
							// complete the taken bit map for this new base
							uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
							// -- in this case, `wakeup_value_restore` fixes the taken bits map
							wakeup_value_restore(HH_FROM_EMPTY,control_bits, cbits, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, thread_id, buffer, end);
						} else {  // question about cell dynamics have been asked... 
							uint8_t backref = 0;
							uint32_t base_cbits = 0;
							uint32_t base_taken_bits = 0;
							hh_element *cells_base = cbits_base_from_backref(backref,bucket,buffer,end_buffer,base_cbits,base_taken_bits);
							//
							// The base of the cell holds he membership map in which this branch finds the cell.
							// The base will be altered... And, the value of the cell should be put back into the cell.
							// BECOME the editor of the base bucket... (any other ops should not be able to access the cell bucket 
							// 	but, the cells base might be busy in conflicting ways. This results in a delay for this condition.)
							atomic<uint32_t> *base_control_bits = (atomic<uint32_t> *)(&(cells_base->c.bits));
							auto cell_base_locking_bits = gen_bits_editor_active(thread_id);   // editor active
							while ( !become_bucket_state_master(base_control_bits,base_cbits,cell_base_locking_bits,thread_id) ) { tick() };
							//
							uint32_t save_key = 0;
							atomic<uint32_t>  *stable_key = load_stable_key(bucket,save_key);
							uint32_t save_value = bucket->->tv.value;
							stash_key_value(save_key,save_value,thread_id);
							// OWN THIS BUCKET
							store_real_cbits(0x1,thread_id);
							bucket->->tv.value = offset_value;
							stable_key->store(el_key);
							control_bits->store(fetch_real_cbits(prev_bits));  // the bucket is now a new base				
							// the usurped value can occupy another spot in its base
							uint64_t current_loaded_value = (((uint64_t)save_key) << HALF) | save_value;
							auto base_h_bucket = (h_bucket-backref);
							wakeup_value_restore(HH_FROM_BASE_USURP,base_control_bits, cells_base, base_h_bucket, current_loaded_value, which_table, thread_id, buffer, end);						
						}
					}

				}
			} while ( prev_bits != cbits );
		}




		/**
		 * _internal_update
		 * 
		 * 
		*/

		uint64_t _internal_update(atomic<uint32_t> *a_c_bits, hh_element *base, uint32_t cbits, uint32_t el_key, uint32_t h_bucket, uint32_t v_value, uint8_t thread_id, uint32_t el_key_update = 0) {
			//
			hh_element *storage_ref = _get_bucket_reference(a_c_bits, base, cbits, h_bucket, el_key, buffer, end, thread_id);  // search
			//
			if ( storage_ref != nullptr ) {
				// 
				if ( el_key_update != 0 ) {
					storage_ref->c.key = el_key_update;
				}
				//
				storage_ref->->tv.value = v_value;
				//
				uint64_t loaded_key = (((uint64_t)el_key) << HALF) | v_value; // LOADED
				loaded_key = stamp_key(loaded_key,selector);
				//
				// clear mobility lock
				//
				return(loaded_key);
			}
			//
			return(UINT64_MAX); // never locked
		}



};




template<const uint32_t NEIGHBORHOOD = 32>
class HH_map : public HH_map_atomic_no_wait<NEIGHBORHOOD> {
	//
	public:

		// LRU_cache -- constructor
		HH_map(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) :
		 								HH_map_atomic_no_wait<NEIGHBORHOOD>(region, seg_sz, max_element_count, num_threads, am_initializer) {
		}

		virtual ~HH_map() {
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		//
		/**
		 * prepare_for_add_key_value_known_refs 
		 * 
		*/
		bool prepare_for_add_key_value_known_refs(atomic<uint32_t> **control_bits_ref,uint32_t h_bucket,uint8_t thread_id,uint8_t &which_table,uint32_t &cbits,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref) {
			//
			atomic<uint32_t> *a_c_bits = _get_member_bits_slice_info(h_bucket,which_table,cbits,bucket_ref,buffer_ref,end_buffer_ref);
			//
			if ( a_c_bits == nullptr ) return false;
			*control_bits_ref = a_c_bits;

			return true;
		}


		/**
		 * add_key_value_known_refs
		 * 
		*/
		uint64_t add_key_value_known_refs(atomic<uint32_t> *control_bits,uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table = 0,uint8_t thread_id,uint32_t cbits,hh_element *bucket,hh_element *buffer,hh_element *end_buffer) {
			//
			uint8_t selector = 0x3;
			//
			if ( (offset_value != 0) &&  selector_bit_is_set(h_bucket,selector) ) {
				//
				return _first_level_bucket_ops(atomic<uint32_t> *control_bits,full_hash,h_bucket,offset,which_table,thread_id,cbits,bucket,buffer,end_buffer);
				//
			}
			return UINT64_MAX;
		}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * get
		*/
		uint32_t get(uint64_t key,uint8_t thread_id = 1) {
			uint32_t el_key = (uint32_t)((key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t hash = (uint32_t)(key & HASH_MASK);
			//
			return get(hash,el_key,thread_id);
		}

		// el_key == hull_hash (usually)

		uint32_t get(uint32_t el_key, uint32_t h_bucket, uint8_t thread_id) {  // full_hash,hash_bucket
			//
			uint8_t selector = 0;
			if ( selector_bit_is_set(h_bucket,selector) ) {
				h_bucket = clear_selector_bit(h_bucket);
			} else return UINT32_MAX;

			//
			hh_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			hh_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			hh_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			hh_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			hh_element *base = bucket_at(buffer, h_bucket, end);
			atomic<uint32_t> *a_c_bits = (atomic<uint32_t> *)(&(base->c.bits));
			uint32_t cbits = 0;
			if ( empty_bucket(a_c_bits,base,cbits) ) return UINT64_MAX;   // empty_bucket cbits by ref
			//
			hh_element *storage_ref = _get_bucket_reference(a_c_bits, base, cbits, h_bucket, el_key, buffer, end, thread_id);  // search
			//
			// clear mobility lock


			if ( storage_ref != nullptr ) {
				value = storage_ref->->tv.value;
			} else {
				// search in reinsertion queue
				// maybe wait a tick and search again...
				return UINT32_MAX;
			}
			//
			return value;
		}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * update
		 * 
		 * Note that for this method the element key contains the selector bit for the even/odd buffers.
		 * The hash value and location must alread exist in the neighborhood of the hash bucket.
		 * The value passed is being stored in the location for the key...
		 * 
		*/
		// el_key == hull_hash (usually)
		uint64_t update(uint32_t el_key, uint32_t h_bucket, uint32_t v_value, uint8_t thread_id, uint32_t el_key_update = 0) {
			//
			if ( v_value == 0 ) return UINT64_MAX;
			//
			uint8_t selector = 0;
			if ( selector_bit_is_set(h_bucket,selector) ) {
				h_bucket = clear_selector_bit(h_bucket);
			} else return UINT64_MAX;
			//
			hh_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			hh_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			hh_element *base = bucket_at(buffer, h_bucket, end);
			atomic<uint32_t> *a_c_bits = (atomic<uint32_t> *)(&(base->c.bits));
			uint32_t cbits = 0;
			if ( empty_bucket(a_c_bits,base,cbits) ) return UINT64_MAX;   // empty_bucket cbits by ref
			// CALL UPDATE (VALUE UPDATE NO KEY UPDATE)
			return _internal_update(a_c_bits, base, cbits, el_key, h_bucket, v_value, thread_id);
		}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * del
		*/

		uint32_t del(uint64_t loaded_key,uint8_t thread_id = 1) {
			uint32_t el_key = (uint32_t)((loaded_key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t h_bucket = (uint32_t)(loaded_key & HASH_MASK);
			return del(el_key, h_bucket,thread_id);
		}


		uint32_t del(uint32_t el_key, uint32_t h_bucket,uint8_t thread_id) {
			//
			uint8_t selector = 0;
			if ( selector_bit_is_set(h_bucket,selector) ) {
				h_bucket = clear_selector_bit(h_bucket);
			} else return UINT32_MAX;
			//
			hh_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			hh_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			hh_element *base = bucket_at(buffer, h_bucket, end);  
			atomic<uint32_t> *a_c_bits = (atomic<uint32_t> *)(&(base->c.bits));
			uint32_t cbits = 0;
			if ( empty_bucket(a_c_bits,base,cbits) ) return UINT32_MAX;   // empty_bucket cbits by ref
			//
			// set that edit is taking place
			// CALL UPDATE (VALUE UPDATE PLUS KEY UPDATE) -- key update is special value
			uint64_t loaded = _internal_update(a_c_bits, base, cbits, el_key, h_bucket, 0, thread_id, UINT32_MAX);
			if ( loaded == UINT64_MAX ) return UINT32_MAX;
			//
			submit_for_cropping(base,cbits,buffer,end,selector);  // after a total swappy read, all BLACK keys will be at the end of members
			return 
		}


		/**
		 * set_random_bits
		*/
		// 4*(this->_bits.size() + 4*sizeof(uint32_t))

		void set_random_bits(void *shared_bit_region) {
			uint32_t *bits_for_test = (uint32_t *)(shared_bit_region);
			for ( int i = 0; i < _max_r_buffers; i++ ) {
				this->set_region(bits_for_test,i);    // inherited method set the storage (otherwise null and not operative)
				this->regenerate_shared(i);
				bits_for_test += this->_bits.size() + 4*sizeof(uint32_t);  // 
			}
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// THREAD CONTROL

		void tick() {
			this_thread::sleep_for(chrono::nanoseconds(20));
		}

		void thread_sleep(uint8_t ticks) {
			microseconds us = microseconds(ticks);
			auto start = high_resolution_clock::now();
			auto end = start + us;
			do {
				std::this_thread::yield();
			} while ( high_resolution_clock::now() < end );
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * wait_notification_restore - put the restoration thread into a wait state...
		*/
		void  wait_notification_restore() {
#ifndef __APPLE__
			do {
				_sleeping_reclaimer.wait(false);
			} while ( _sleeping_reclaimer.test(std::memory_order_acquire) );
#else
			while ( _sleeping_reclaimer.test_and_set() ) __libcpp_thread_yield();
#endif
		}

		/**
		 * wake_up_one_restore -- called by the requesting thread looking to have a value put back in the table
		 * after its temporary removal.
		*/
		void wake_up_one_restore(void) {
#ifndef __APPLE__
			do {
				_sleeping_reclaimer.test_and_set();
			} while ( !(_sleeping_reclaimer.test(std::memory_order_acquire)) );
			_sleeping_reclaimer.notify_one();
#else
			_sleeping_reclaimer.clear();
#endif
		}

		/**
		 * is_restore_queue_empty - check on the state of the restoration queue.
		*/
		bool is_restore_queue_empty(uint8_t which_table) {
			bool is_empty = _process_queue[which_table].empty();
#ifndef __APPLE__

#endif
			return is_empty;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



		/**
		 * bit patterns
		 * 
		*/


		/**
		 * short_list_old_entry - calls on the application's method for puting displaced values, new enough to be kept,
		 * into a short list that may be searched in the order determined by the application (e.g. first for preference, last
		 * for normal lookup except fails.)
		 * 
		*/
		bool short_list_old_entry([[maybe_unused]] uint64_t loaded_value,[[maybe_unused]] uint32_t store_time) {
			return false;
		}


		/**
		 * value_restore_runner   --- a thread method...
		 * 
		 * One loop of the main thread for restoring values that have been replaced by a new value.
		 * The element being restored may still be in play, but 
		*/
		void value_restore_runner(uint8_t slice_for_thread, uint8_t assigned_thread_id) {
			hh_element *hash_ref = nullptr;
			uint32_t h_bucket = 0;
			uint64_t loaded_value = 0;
			uint8_t hole = 0;
			uint8_t which_table = slice_for_thread;
			uint8_t thread_id = 0;
			hh_element *buffer = nullptr;
			hh_element *end = nullptr;
			hh_adder_states update_type;
			//
			while ( is_restore_queue_empty(assigned_thread_id,slice_for_thread) ) wait_notification_restore();
			//
			dequeue_restore(update_type,&hash_ref, h_bucket, loaded_value, hole, which_table, thread_id, assigned_thread_id, &buffer, &end);
			// store... if here, it should be locked
			//
			uint32_t store_time = now_time(); // now

/*
	HH_FROM_EMPTY,
	HH_FROM_BASE,
	HH_FROM_BASE_AND_WAIT,
	HH_FROM_BASE_USURP,
	HH_FROM_USURP_AND_WAIT,
	HH_ADDER_STATES
*/



			// hole -- try to gather all the work for a single bucket from the queue...
			bool quick_put_ok = pop_until_oldest(hash_ref, loaded_value, store_time, which_table, buffer, end, thread_id);
			//
			if ( quick_put_ok ) {   // now unlock the bucket... 
				//this->slice_unlock_counter(controller,which_table,thread_id);
			} else {
				// unlock the bucket... the element did not get back into the bucket, but another attempt may be made
				// this->slice_unlock_counter(controller,which_table,thread_id);
				//
				// if the entry can be entered onto the short list (not too old) then allow it to be added back onto the restoration queue...
				if ( short_list_old_entry(loaded_value, store_time) ) {
					uint32_t el_key = (uint32_t)((loaded_value >> HALF) & HASH_MASK);  // just unloads it (was index)
					uint32_t offset_value = (loaded_value & UINT32_MAX);
					a_place_in_bucket(el_key, h_bucket, offset_value, which_table, thread_id, N, buffer, end);
				}
				//
			}

		}


		/**
		 * enqueue_restore
		*/
		void enqueue_restore(hh_adder_states update_type, hh_element *hash_ref, uint32_t h_bucket, uint64_t loaded_value, uint8_t hole, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end) {
			q_entry get_entry;
			//
			get_entry.update_type = update_type;
			get_entry.hash_ref = hash_ref;
			get_entry.h_bucket = h_bucket;
			get_entry.loaded_value = loaded_value;
			get_entry.hole = hole;
			get_entry.which_table = which_table;
			get_entry.buffer = buffer;
			get_entry.end = end;
			get_entry.thread_id = thread_id;
			//
			proc_descr *p = _process_table + _round_robbin_proc_table_threads;
			//
			_round_robbin_proc_table_threads++;
			if ( _round_robbin_proc_table_threads > _num_threads ) _round_robbin_proc_table_threads = 0;
			//
			p->_process_queue[which_table].push(get_entry); // by ref
		}

		/**
		 * dequeue_restore
		*/
		void dequeue_restore(hh_adder_states &update_type,hh_element **hash_ref_ref, uint32_t &h_bucket, uint64_t &loaded_value, uint8_t &hole, uint8_t &which_table, uint8_t &thread_id, uint8_t assigned_thread_id , hh_element **buffer_ref, hh_element **end_ref) {
			//
			q_entry get_entry;
			//
			proc_descr *p = _process_table + assigned_thread_id;
			//
			p->_process_queue[which_table].pop(get_entry); // by ref
			//
			hh_element *hash_ref = get_entry.hash_ref;
			h_bucket = get_entry.h_bucket;
			loaded_value = get_entry.loaded_value;
			hole = get_entry.hole;
			which_table = get_entry.which_table;
			hh_element *buffer = get_entry.buffer;
			hh_element *end = get_entry.end;
			thread_id = get_entry.thread_id;
			update_type = get_entry.update_type
			//
			*hash_ref_ref = hash_ref;
			*buffer_ref = buffer;
			*end_ref = end;
		}





		/**
		 * wakeup_value_restore
		*/

		void wakeup_value_restore(hh_adder_states update_type,hh_element *hash_ref, uint32_t h_start, uint64_t loaded_value, uint8_t hole, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end) {
			// this queue is jus between the calling thread and the service thread belonging to just this process..
			// When the thread works it may content with other processes for the hash buckets on occassion.
			enqueue_restore(update_type,hash_ref,h_start,loaded_value,hole,which_table,thread_id,buffer,end);
			wake_up_one_restore();
		}





	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * cropper_runner   --- a thread method...
		 * 
		 * One loop of the main thread for restoring values that have been replaced by a new value.
		 * The element being restored may still be in play, but 
		*/
		void cropper_runner(uint8_t slice_for_thread, uint8_t assigned_thread_id) {
			hh_element *base = nullptr;
			uint32_t cbits = 0;
			uint8_t which_table = slice_for_thread;
			hh_element *buffer = nullptr;
			hh_element *end = nullptr;
			//
			while ( is_cropping_queue_empty(assigned_thread_id,slice_for_thread) ) wait_notification_cropping();
			//
			dequeue_cropping(&base, cbits, which_table, assigned_thread_id, &buffer, &end);
			// store... if here, it should be locked
			//
			_cropper(base, cbits, buffer, end, assigned_thread_id);
		}


		/**
		 * enqueue_cropping
		*/
		void enqueue_cropping(hh_element *hash_ref,uint32_t cbits,hh_element *buffer,hh_element *end,uint8_t which_table) {
			crop_entry get_entry;
			//
			get_entry.hash_ref = hash_ref;
			get_entry.cbits = cbits;
			get_entry.buffer = buffer;
			get_entry.end = end;
			get_entry.which_table = which_table;
			//
			proc_descr *p = _process_table + _round_robbin_proc_table_threads;
			//
			_round_robbin_proc_table_threads++;
			if ( _round_robbin_proc_table_threads > _num_threads ) _round_robbin_proc_table_threads = 0;
			//
			p->_to_cropping[which_table].push(get_entry); // by ref
		}

		/**
		 * dequeue_cropping
		*/
		void dequeue_cropping(hh_element **hash_ref_ref, uint32_t &cbits, uint8_t &which_table, uint8_t assigned_thread_id , hh_element **buffer_ref, hh_element **end_ref) {
			//
			crop_entry get_entry;
			//
			proc_descr *p = _process_table + assigned_thread_id;
			//
			p->_to_cropping[which_table].pop(get_entry); // by ref
			//
			hh_element *hash_ref = get_entry.hash_ref;
			cbits = get_entry.cbits;
			which_table = get_entry.which_table;
			//
			hh_element *buffer = get_entry.buffer;
			hh_element *end = get_entry.end;
			//
			*hash_ref_ref = hash_ref;
			*buffer_ref = buffer;
			*end_ref = end;
		}





		/**
		 * wait_notification_restore - put the restoration thread into a wait state...
		*/
		void  wait_notification_cropping() {
#ifndef __APPLE__
			do {
				_sleeping_cropper.wait(false);
			} while ( _sleeping_cropper.test(std::memory_order_acquire) );
#else
			while ( _sleeping_cropper.test_and_set() ) __libcpp_thread_yield();
#endif
		}

		/**
		 * wake_up_one_restore -- called by the requesting thread looking to have a value put back in the table
		 * after its temporary removal.
		*/
		void wake_up_one_cropping(void) {
#ifndef __APPLE__
			do {
				_sleeping_cropper.test_and_set();
			} while ( !(_sleeping_cropper.test(std::memory_order_acquire)) );
			_sleeping_cropper.notify_one();
#else
			_sleeping_cropper.clear();
#endif
		}

		/**
		 * is_restore_queue_empty - check on the state of the restoration queue.
		*/
		bool is_cropping_queue_empty(uint8_t which_table) {
			bool is_empty = _to_cropping[which_table].empty();
#ifndef __APPLE__

#endif
			return is_empty;
		}


		/**
		 * submit_for_cropping
		*/

		void submit_for_cropping(hh_element *base,uint32_t cbits,hh_element *buffer,hh_element *end,uint8_t which_table) {
			enqueue_restore(base,cbits,h_start,buffer,end,which_table);
			wake_up_one_cropping();
		}



#ifdef _DEBUG_TESTING_
	public:			// these may be used in a test class...
#else
	protected:
#endif


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



		uint32_t obtain_cell_key(atomic<uint32_t> *a_ky) {
			uint32_t real_ky = (UINT32_MAX-1);
			while ( a_ky->compare_exchange_weak(real_ky,(UINT32_MAX-1)) && (real_ky == (UINT32_MAX-1)));
			return real_ky;
		}
	

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



		bool search_writing_process_queues(uint32_t el_key,uint32_t &value,uint8_t thread_id,uint8_t selector) {
			proc_descr *p = _process_table;
			for ( uint8_t t = 0; t < _num_threads; t++ ) {
				if ( !(p->_process_queue[selector].empty()) ) {
					auto el = p->_process_queue[selector]._r_cached;
					auto stop = p->_process_queue[selector]._w_cached;
					auto end = p->_process_queue[selector]._end;
					auto beg = p->_process_queue[selector]._beg;
					while ( el != stop ) {
						uint32_t k = (uint32_t)(el->loaded_value & UINT32_MAX)
						if ( k == el_key ) {
							value = ((el->loaded_value >> sizeof(uint32_t)) & UINT32_MAX);
							return true;
						}
						el++;
						if ( (el == end) && (end != stop) ) el = beg;
					}
				}
				p++;
			}
			return false;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	public:


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * place_taken_spots
		 * 
		 * c contains bucket starts..  Each one of these may be busy....
		 * but, this just sets the taken spots; so, the taken spots may be treated as atomic...
		 * 
		*/
		void place_taken_spots(hh_element *hash_ref, uint32_t hole, uint32_t c, hh_element *buffer, hh_element *end) {
			hh_element *vb_probe = nullptr;
			//
			c = c & zero_above(hole);
			while ( c ) {
				vb_probe = hash_ref;
				uint8_t offset = get_b_offset_update(c);			
				vb_probe += offset;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				//
				// may check to see if the value settles down...
				vb_probe->->tv.taken |= (1 << (hole - offset));   // the bit is not as far out
			}
			//
			place_back_taken_spots(hash_ref, hole, buffer, end);
		}


		/**
		 * place_back_taken_spots
		*/

		void place_back_taken_spots(hh_element *hash_base, uint32_t dist_base, hh_element *buffer, hh_element *end) {
			//
			uint8_t g = NEIGHBORHOOD - 1;
			auto last_view = (g - dist_base);
			//
			if ( last_view >  0 ) { // can't influence any change below the bucket
				hh_element *vb_probe = hash_base - last_view;
				vb_probe = el_check_beg_wrap(vb_probe,buffer,end);
				uint32_t c = 0;
				uint8_t k = g;     ///  (dist_base + last_view) :: dist_base + (g - dist_base) ::= dist_base + (NEIGHBORHOOD - 1) - dist_base ::= (NEIGHBORHOOD - 1) ::= g
				while ( vb_probe != hash_base ) {
					if ( vb_probe->c.bits & 0x1 ) {
						vb_probe->->tv.taken |= ((uint32_t)0x1 << k);  // the bit is not as far out
						c = vb_probe->c.bits;
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & vb_probe->->tv.taken;
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					if ( base_probe->c.bits & 0x1 ) {
						auto j = k;
						j -= offset_nxt;
						base_probe->->tv.taken |= ((uint32_t)0x1 << j);  // the bit is not as far out
						c = c & ~(base_probe->c.bits);  // no need to look at base probe members anymore ... remaining bits are other buckets
					}
				}
			}
		}


		/**
		 * remove_back_taken_spots
		*/

		void remove_back_taken_spots(hh_element *hash_base, uint32_t dist_base, hh_element *buffer, hh_element *end) {
			//
			uint8_t g = NEIGHBORHOOD - 1;
			auto last_view = (g - dist_base);
			//
			if ( last_view >  0 ) { // can't influence any change below the bucket
				hh_element *vb_probe = hash_base - last_view;
				vb_probe = el_check_beg_wrap(vb_probe,buffer,end);
				uint32_t c = 0;
				uint8_t k = g;     ///  (dist_base + last_view) :: dist_base + (g - dist_base) ::= dist_base + (NEIGHBORHOOD - 1) - dist_base ::= (NEIGHBORHOOD - 1) ::= g
				while ( vb_probe != hash_base ) {
					if ( vb_probe->c.bits & 0x1 ) {
						vb_probe->->tv.taken &= (~((uint32_t)0x1 << k));  // the bit is not as far out
						c = vb_probe->c.bits;
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & vb_probe->->tv.taken;
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					if ( base_probe->c.bits & 0x1 ) {
						auto j = k;
						j -= offset_nxt;
						base_probe->->tv.taken &= (~((uint32_t)0x1 << j));  // the bit is not as far out
						c = c & ~(base_probe->c.bits);  // no need to look at base probe members anymore ... remaining bits are other buckets
					}
				}
			}
		}


		/**
		 * shift_membership_spots
		*/
		hh_element *shift_membership_spots(hh_element *hash_base,hh_element *vb_nxt,uint32_t c, hh_element *buffer, hh_element *end) {
			while ( c ) {
				hh_element *vb_probe = hash_base;
				auto offset = get_b_offset_update(c);
				if ( c ) {
					vb_probe += offset;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					//
					swap(vb_nxt->c.key,vb_probe->c.key);				
					swap(vb_nxt->->tv.value,vb_probe->->tv.value);				
					// not that the c_bits are note copied... for members, it is the offset to the base, while the base stores the memebership bit pattern
					swap(vb_nxt->->tv.taken,vb_probe->->tv.taken);  // when the element is not a bucket head, this is time...
					vb_nxt = vb_probe;
				} else {
					return vb_nxt;
				}
			}
			return nullptr;
		}

		/**
		 * remove_bucket_taken_spots
		*/
		void remove_bucket_taken_spots(hh_element *hash_base,uint8_t nxt_loc,uint32_t a,uint32_t b, hh_element *buffer, hh_element *end) {
			auto c = a ^ b;   // now look at the bits within the range of the current bucket indicating holdings of other buckets.
			c = c & zero_above(nxt_loc);  // buckets after the removed location do not see the location; so, don't process them
			while ( c ) {
				hh_element *vb_probe = hash_base;
				auto offset = get_b_offset_update(c);
				vb_probe += offset;
				if ( vb_probe->c.bits & 0x1 ) {
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					vb_probe->->tv.taken &= (~((uint32_t)0x1 << (nxt_loc - offset)));   // the bit is not as far out
				}
			}
		}

		/**
		 * removal_taken_spots
		*/
		void removal_taken_spots(hh_element *hash_base,hh_element *hash_ref,uint32_t c_pattern,hh_element *buffer, hh_element *end) {
			//
			uint32_t a = c_pattern; // membership mask
			uint32_t b = hash_base->->tv.taken;   // load this or rely on lock
			//

			//
			hh_element *vb_last = nullptr;

			auto c = a;   // use c as temporary
			//
			if ( hash_base != hash_ref ) {  // if so, the bucket base is being replaced.
				uint32_t offset = (hash_ref - hash_base);
				c = c & ones_above(offset);
			}
			//
			// membership bits are currently locked preventing a read..
			vb_last = shift_membership_spots(hash_base,hash_ref,c,buffer,end);   // SHIFT leaving the hole at the end
			if ( vb_last == nullptr ) return;

			// vb_probe should now point to the last position of the bucket, and it can be cleared...
			vb_last->c.bits = 0;
			vb_last->->tv.taken = 0;
			vb_last->c.key = 0;
			vb_last->->tv.value = 0;

			uint8_t nxt_loc = (vb_last - hash_base);   // the spot that actually cleared...

			// recall, a and b are from the base
			// clear the bits for the element being removed
			UNSET(a,nxt_loc);
			UNSET(b,nxt_loc);
			hash_base->c.bits = a;
			hash_base->->tv.taken = b;

			// OTHER buckets
			// now look at the bits within the range of the current bucket indicating holdings of other buckets.
			// these buckets are ahead of the base, but behind the cleared position...
			remove_bucket_taken_spots(hash_base,nxt_loc,a,b,buffer,end);
			// look for bits preceding the current bucket
			remove_back_taken_spots(hash_base, nxt_loc, buffer, end);
		}


		/**
		 * seek_next_base
		*/
		hh_element *seek_next_base(hh_element *base_probe, uint32_t &c, uint32_t &offset_nxt_base, hh_element *buffer, hh_element *end) {
			hh_element *hash_base = base_probe;
			while ( c ) {
				auto offset_nxt = get_b_offset_update(c);
				base_probe += offset_nxt;
				base_probe = el_check_end_wrap(base_probe,buffer,end);
				if ( base_probe->c.bits & 0x1 ) {
					offset_nxt_base = offset_nxt;
					return base_probe;
				}
				base_probe = hash_base;
			}
			return base_probe;
		}

		/**
		 * seek_min_member
		 * 
		 * The min member could change if there is a usurpation.
		*/
		void seek_min_member(hh_element **min_probe_ref, uint32_t &min_base_offset, hh_element *base_probe, uint32_t time, uint32_t offset, uint32_t offset_nxt_base, hh_element *buffer, hh_element *end) {
			atomic<uint32_t> *cbits = (atomic<uint32_t> *)(&(base_probe->c.bits));
			auto c = cbits->load(std::memory_order_acquire);		// nxt is the membership of this bucket that has been found
			c = c & (~((uint32_t)0x1));   // the interleaved bucket does not give up its base...
			if ( offset_nxt_base < offset ) {
				c = c & ones_above(offset - offset_nxt_base);  // don't look beyond the window of our base hash bucket
			}
			c = c & zero_above((NEIGHBORHOOD-1) - offset_nxt_base); // don't look outside the window
			//
			while ( c ) {			// 
				auto vb_probe = base_probe;
				auto offset_min = get_b_offset_update(c);
				vb_probe += offset_min;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				if ( vb_probe->->tv.taken < time ) {
					// if ( !check_slice_lock(vb_probe,which_table,thread_id) ) continue;  // if the element is being usurped, then if it is min OK another min remains, else the min is futher down the list
					// if ( *min_probe_ref != nullptr ) slice_unlock(*min_probe_ref)
					time = vb_probe->->tv.taken;
					*min_probe_ref = vb_probe;
					min_base_offset = offset_min;
				}
				// free lock this slice vb_probe,  when the above call returned true
			}
			//
		}


		/**
		 * full_bucket_so_usurp_loosest_tooth
		*/
		void full_bucket_so_usurp_loosest_tooth(hh_element *hash_base,uint32_t c,uint64_t &v_passed,uint32_t &time,uint32_t offset,hh_element *buffer, hh_element *end) {
			//
			uint32_t min_base_offset = 0;
			uint32_t offset_nxt_base = 0;
			auto min_probe = hash_base;
			auto min_base = hash_base;
			auto base_probe = hash_base;
			//
			while ( c ) {
				base_probe = seek_next_base(hash_base, c, offset_nxt_base, buffer, end);
				// look for a bucket within range that may give up a position
				if ( c ) {		// landed on a bucket (now what about it)
					// lock on base_probe (slice lock counter ...)
					if ( popcount(base_probe->c.bits) > 1 ) {
						seek_min_member(&min_probe, min_base_offset, base_probe, time, offset, offset_nxt_base, buffer, end);
					}
				}
				// unlock slice counter base_probe
			}
			if ( base_probe != hash_base ) {  // found a place in the bucket that can be moved... (steal this spot)
				//
				uint32_t pky = (uint32_t)((v_passed >> HALF) & HASH_MASK);  // just unloads it (was index)
				uint32_t pval = (v_passed & UINT32_MAX);
				min_probe->c.key = pk;
				min_probe->->tv.value = pval;

				min_probe->c.bits = (min_base_offset + offset_nxt_base) << 1;  // offset is stored one bit up
				time = stamp_offset(time,(min_base_offset + offset_nxt_base));  // offset is now
				min_probe->->tv.taken = time;  // when the element is not a bucket head, this is time... 
				// slice_unlock(min_probe)
				atomic<uint64_t> *c_aptr = (atomic<uint64_t> *)(min_base->c.bits);
				auto m_c_bits = c_aptr->load();
				auto m_c_bits_prev = m_c_bits;
				m_c_bits &= ~(0x1 << min_base_offset);
				while ( !(c_aptr->compare_exchange_weak(m_c_bits_prev,m_c_bits,std::memory_order_release)) ) { m_c_bits = m_c_bits_prev; m_c_bits &= ~(0x1 << min_base_offset); }
				// min_base->c.bits &= ~(0x1 << min_base_offset);  // turn off the bit (steal bit away)
				//
				atomic<uint64_t> *h_c_aptr = (atomic<uint64_t> *)(hash_base->c.bits);
				auto h_c_bits = h_c_aptr->load();
				auto h_c_bits_prev = h_c_bits;
				h_c_bits |= (0x1 << (min_base_offset + offset_nxt_base));
				while ( !(h_c_aptr->compare_exchange_weak(h_c_bits_prev,h_c_bits,std::memory_order_release)) ) { h_c_bits = h_c_bits_prev; h_c_bits |= (0x1 << (min_base_offset + offset_nxt_base)); }
				// hash_base->c.bits |= (0x1 << (min_base_offset + offset_nxt_base));  // keep it here
				// taken spots is black, so we don't reset values after this...
			}
		}


		/**
		 * inner_bucket_time_swaps
		 * 
		 * Only one thread can time swap its own elements.
		 * But, a usurping thread can interfere with one element.
		*/

		uint32_t inner_bucket_time_swaps(hh_element *hash_ref, uint32_t c_bits, uint8_t hole, uint64_t &v_passed, uint32_t &time, uint8_t which_table, hh_element *buffer, hh_element *end, [[maybe_unused]] uint8_t thread_id) {
			uint32_t a = c_bits;
			a = a & zero_above(hole);  // this is the list of bucket members 
			//
			// unset the first bit of the indexing bits (which indicates this position starts a bucket)
			uint32_t offset = 0;
			hh_element *vb_probe = nullptr;
			//
			a = a & (~((uint32_t)0x1));  // skipping the base (the new element is there)
			while ( a ) {		// walk the list of bucket members
				vb_probe = hash_ref;
				offset = get_b_offset_update(a);
				vb_probe += offset;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				// if this is being usurpred, then it can't be accessible... check thread_id
				// check lock this slice vb_probe (wait)
				// check for usurpation.. the lock should be locked in that case and the swap 
				// should be skipped
				// if ( !check_slice_lock(which_table,thread_id) ) continue;
				atomic<uint32_t> *k_aptr = (atomic<uint32_t> *)(&(vb_probe->c.key));

				uint32_t pky = (uint32_t)((loaded_value >> HALF) & HASH_MASK);  // just unloads it (was index)
				uint32_t pval = (loaded_value & UINT32_MAX);

				auto ky = k_aptr->exchange(pky,std::memory_order_acq_rel);
				//
				atomic<uint32_t> *v_aptr = (atomic<uint32_t> *)(&(vb_probe->->tv.value));
				auto val = v_aptr->exchange(pval,std::memory_order_acq_rel);

				time = stamp_offset(time,offset);
				atomic<uint32_t> *t_aptr = (atomic<uint32_t> *)(vb_probe->->tv.taken);
				time = t_aptr->exchange(time,std::memory_order_acq_rel);
				//swap(time,vb_probe->->tv.taken);  // when the element is not a bucket head, this is time... 
				// free lock this slice vb_probe
			}
			return offset;
		}


		/**
		 * pop_until_oldest
		 * 
		 * v_passed is gauranteed to be newer than all the remaining values...
		 * v_passed is gauranteed to be the displaced value from the bucket head position (hash_ref)
		 * 
		 * hash_ref is gauraneed to be the group leader, the first bit is set and 
		 * a newer element is already stored; so, this will skip the first element and 
		 * move everything until the end... 
		 * 
		 * Look at all the set bits of membership...
		 * 
		 * This is certainly slower than doing something random, but information can be gained along the way.
		 * 
		*/

		bool pop_until_oldest(hh_element *hash_base, uint64_t &v_passed, uint32_t &time, uint8_t which_table, hh_element *buffer, hh_element *end_buffer, uint8_t thread_id) {
			if ( v_passed == 0 ) return false;  // zero indicates empty...
			// note: the c_bits should be locked .. hence ops on the c_bits should be without underlying changes.. 
			atomic<uint32_t> *hb_c_bits = (atomic<uint32_t> *)(&hash_base->c.bits);
			uint32_t a = hb_c_bits->load(std::memory_order_acquire); // membership mask
			atomic<uint32_t> *hb_t_bits = (atomic<uint32_t> *)(&hash_base->->tv.taken);
			uint32_t b = hb_t_bits->load(std::memory_order_acquire);
			//
			uint8_t hole = countr_one(b);
			if ( hole < 32 ) {   // if all the spots are already taken, no need to reserve it.
				auto a_prev = a;
				auto b_prev = b;
				//
				uint32_t hbit = (1 << hole);
				a = a | hbit;
				b = b | hbit;
				//  may guard against underlying changes anyway .. may not be necessary..
				while ( !(hb_c_bits->compare_exchange_weak(a_prev,a,std::memory_order_release)) ) { a = a_prev | hbit; }
				while ( !(hb_c_bits->compare_exchange_weak(b_prev,b,std::memory_order_release)) ) { b = b_prev | hbit; }
			}
			// swapping takes place only on the cbits. Yet a position among the c_bits may be locked if 
			// a usurpation is in progress. The lock can be checked...
			uint32_t offset = inner_bucket_time_swaps(hash_base,a,hole,v_passed,time,which_table,buffer,end_buffer,thread_id);  // thread id
			// offset will be > 0

			// now the oldest element in this bucket is in hand and may be stored if interleaved buckets don't
			// preempt it.

			uint32_t c = ( hole < 32 ) ? (a ^ b) : ~a;

			// c contains bucket starts.. offset to those to update the occupancy vector if they come between the 
			// hash bucket and the hole.

			if ( b == UINT32_MAX ) {  // v_passed can't be zero. Instead it is the oldest of the (maybe) full bucket.
				// however, other buckets are stored interleaved. These buckets have not been examined for maybe 
				// swapping with the current bucket oldest value.  (enter into territory of another bucket).
				full_bucket_so_usurp_loosest_tooth(hash_base,c,v_passed,time,offset,buffer,end_buffer);
				//
				//h_bucket += offset;
				return false;  // this region has no free space...
			} else {
				hh_element *vb_probe = hash_base + hole;
				vb_probe->c.bits = (hole << 1);  // this is the offset stored one bit up...
				vb_probe->->tv.taken = time;
				place_taken_spots(hash_base, hole, c, buffer, end_buffer);  // c contains bucket starts..
			}

			// the oldest element should now be free cell or at least (if it was something deleted) all the older values
			// have been shifted. v_passed should be zero. 
			return true;	// there are some free cells.
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	public:

		// ---- ---- ---- STATUS

		bool ok(void) {
			return(this->_status);
		}

	public:


	// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------

		/**
		 * _get_member_bits_slice_info
		 * 
		 * 	<-- called by prepare_for_add_key_value_known_refs -- which is application facing.
		*/
		atomic<uint32_t> *_get_member_bits_slice_info(uint32_t h_bucket,uint8_t &which_table,uint32_t &c_bits,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref) {
			//
			if ( h_bucket >= _max_n ) {
				h_bucket -= _max_n;  // let it be circular...
			}
			uint8_t count0{0};
			uint8_t count1{0};
			//
			hh_element *buffer0		= _region_HV_0;
			hh_element *end_buffer0	= _region_HV_0_end;
			//
			hh_element *el_0 = buffer0 + h_bucket;
			atomic<uint32_t> *cbits0 = (atomic<uint32_t> *)(&(el_0->c.bits));
			auto c0 = cbits0->load(std::memory_order_acquire);
			if ( is_base_noop(c0) ) {
				count0 = popcount(c0);
			} else {
				if ( base_in_operation(c0) ) {
					auto real_bits = fetch_real_cbits(c0);
					count0 = popcount(real_bits);
				}
			}
			//
			hh_element *buffer1		= _region_HV_1;
			hh_element *end_buffer1	= _region_HV_1_end;
			//
			hh_element *el_1 = buffer1 + h_bucket;
			atomic<uint32_t> *cbits1 = (atomic<uint32_t> *)(&(el_1->c.bits));
			auto c1 = cbits1->load(std::memory_order_acquire);
			if ( is_base_noop(c1) ) {
				count1 = popcount(c1);
			} else {
				if ( base_in_operation(c1) ) {
					auto real_bits = fetch_real_cbits(c1);
					count1 = popcount(real_bits);
				}
			}
			//
			auto selector = _hlpr_select_insert_buffer(count0,count1);
			if ( selector == 0xFF ) return nullptr;
			//
			which_table = selector;
			if ( selector == 0 ) {				// select data structures
				*bucket_ref = el_0;
				*buffer_ref = buffer0;
				*end_buffer_ref = end_buffer0;
				c_bits = c0;
				return cbits0;
			} else {
				*bucket_ref = el_1;
				*buffer_ref = buffer1;
				*end_buffer_ref = end_buffer1;
				c_bits = c1;
				return cbits1;
			}
			//
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * create_base_taken_spots
		*/

		void create_base_taken_spots(hh_element *hash_ref, uint32_t value, uint32_t el_key, hh_element *buffer, hh_element *end_buffer) {
			hh_element *base_ref = hash_ref;
			uint32_t c = 1;				// c will be the new base element map of taken positions
			for ( uint8_t i = 1; i < NEIGHBORHOOD; i++ ) {
				hash_ref = el_check_end_wrap( ++hash_ref, buffer, end ); // keep walking forward to the end of the window
				if ( bucket_is_base(hash_ref) ) {			// this is a base bucket with information beyond the window
					atomic<uint32_t> *hn_t_bits = (atomic<uint32_t> *)(&hash_ref->->tv.taken);
					auto taken = hn_t_bits->load(std::memory_order_acquire);
					c |= (taken << i); // this is the record, but shift it....
					break;								// don't need to get more information at this point
				} else if ( hash_ref->c.bits != 0 ) {
					SET(c,i);							// this element belongs to a base below the new base
				}
			}
			atomic<uint32_t> *b_t_bits = (atomic<uint32_t> *)(&base_ref->->tv.taken);
			b_t_bits->store(c,std::memory_order_release);
		}


		/**
		 * reserve_membership - is called at a point after cbits have been altered in the shared position 
		 * and original bits is the membership mask that must change
		*/
		bool reserve_membership(atomic<uint32_t> *control_bits, atomic<uint32_t> *tbits, uint32_t cbits, uint32_t original_cbits, uint8_t &hole, uint8_t thread_id, hh_element *buffer, hh_element *end_buffer ) {
			// first unload the taken spots... 
			auto a = original_cbits;    // these should be stored in the thread table at the time of call ... cbits indicate the ownership of the cell
			uint32_t b = tbits->load(std::memory_order_acquire);
			//
			hole = countr_one(b);
			if ( hole < sizeof(uint32_t) ) {
				auto b_prev = b;
				//
				uint32_t hbit = (1 << hole);
				b = b | hbit;
				while ( !(tbits->compare_exchange_weak(b_prev,b,std::memory_order_release)) ) {
					if ( (prev_b & hbit) != 0 ) {
						hole = countr_one(b);   // try for another hole ... someone else go last attempt
						if ( hole >= sizeof(uint32_t) ) {
							break;
						}
						hbit = (1 << hole);
					}
					b = b_prev | hbit; 
				}
				//
				if ( hole < sizeof(uint32_t) ) {
					auto c = (a ^ b);
					place_taken_spots(hash_base, hole, c, buffer, end_buffer);  // c contains bucket starts..
					a = a | hbit;
					store_real_cbits(a,thread_id);
				}
				return true
			}
			//
			return false;
		}


		void complete_bucket_creation(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t locking_bits, hh_element *bucket, hh_element *buffer, hh_element *end_buffer) {
			//
			a_place_back_taken_spots(bucket, 0, buffer, end);
			create_base_taken_spots(bucket, buffer, end);   // this bucket is locked 
			//
			cbits = fetch_real_cbits(locking_bits);
			control_bits->store(cbits);
			//
		}


		void complete_add_bucket_member(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t locking_bits, hh_element *bucket, hh_element *buffer, hh_element *end_buffer) {
			atomic<uint32_t> *tbits = (atomic<uint32_t> *)(&bucket->->tv.taken);
			uint8_t hole;
			if ( reserve_membership(control_bits,tbits,cbits,prev_bits,hole,thread_id,buffer,end_buffer) ) {
				//
				store_real_cbits(cbits,current_thread);   // reserve a spot using prev bits and take spots
				inner_bucket_time_swaps(hash_base,a,hole,v_passed,time,which_table,buffer,end_buffer,thread_id);  // thread id
				//
			} else {
				// this bucket is full.. except there may be something that can budge and is within the window.
				full_bucket_so_usurp_loosest_tooth(hash_base,c,v_passed,time,offset,buffer,end_buffer);
			}
		}


		void complete_add_bucket_member_late(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t locking_bits, hh_element *bucket, hh_element *buffer, hh_element *end_buffer) {
			atomic<uint32_t> *tbits = (atomic<uint32_t> *)(&bucket->->tv.taken);
			uint8_t hole;
			if ( reserve_membership(control_bits,tbits,cbits,prev_bits,hole,thread_id,buffer,end_buffer) ) {
				//
				store_real_cbits(cbits,current_thread);   // reserve a spot using prev bits and take spots
				inner_bucket_time_swaps(hash_base,a,hole,v_passed,time,which_table,buffer,end_buffer,thread_id);  // thread id
				//
			} else {
				full_bucket_so_usurp_loosest_tooth(hash_base,c,v_passed,time,offset,buffer,end_buffer);
			}
		}








		/**
		 * place_back_taken_spots
		*/

		atomic<uint32_t> *lock_taken_spots(hh_element *vb_probe,uint32_t &taken) {
			atomic<uint32_t> *a_taken = (atomic<uint32_t> *)(&(vb_probe->->tv.taken));
			taken = a_taken->load(std::memory_order_acquire);
			if ( !(taken & 0x1) ) {
				while ( a_taken->compare_exchange_weak(taken,taken,std::memory_order_acq_rel) || !(taken & 0x1) );
			}
			auto prep_taken = taken & (UINT32_MAX-1);
			while ( !a_taken->compare_exchange_weak(taken,prep_taken,std::memory_order_acq_rel) ) {
				prep_taken = taken & (UINT32_MAX-1);
			}

			return a_taken;
		}


		/**
		 * a_place_back_taken_spots
		 * 
		 * 
		 * Marking a spot as taken... 
		 * 
		 * On getting a new base spot, each base in a window will get new take_spots marking indicating the new spot. 
		 * If the spot is marked during the time the new base entry is being established, then the taken spot will
		 * not change, (ursurpation does not change the taken spots). The new base entry will continue to take the spot
		 * and usurp the element being moved into its spot. Of necessity, the element being shifted into the spot will be older 
		 * since new members are pushed into the base. So, the usurpation may lead to an eviction if the element cannot be 
		 * reinsterted into the bucket that changed the taken spot. 
		 * 
		*/

		void a_place_back_taken_spots(hh_element *hash_base, uint32_t dist_base, hh_element *buffer, hh_element *end) {
			//
			uint8_t g = NEIGHBORHOOD - 1;
			auto last_view = (g - dist_base);
			uint32_t all_taken_spots[32];
			atomic<uint32_t> *all_taken_spot_refs[32];
			uint8_t count_bases = 0;
			//
			if ( last_view > 0 ) { // can't influence any change below the bucket
				hh_element *vb_probe = hash_base - last_view;
				vb_probe = el_check_beg_wrap(vb_probe,buffer,end);
				uint32_t c = 0;
				uint8_t k = g;     ///  (dist_base + last_view) :: dist_base + (g - dist_base) ::= dist_base + (NEIGHBORHOOD - 1) - dist_base ::= (NEIGHBORHOOD - 1) ::= g
				// go until vb_probe hits on a base
				while ( vb_probe != hash_base ) {
					if ( bucket_is_base(vb_probe,c) ) {
						uint32_t taken;
						all_taken_spot_refs[count_bases] = lock_taken_spots(vb_probe,taken);
						all_taken_spots[count_bases++] = (taken | ((uint32_t)0x1 << k));
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				// get a map of all the base elements above the lowest bucket found
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & vb_probe->->tv.taken;
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					if ( bucket_is_base(base_probe,c2) ) {
						auto j = k;
						j -= offset_nxt;
						uint32_t taken;
						all_taken_spot_refs[count_bases] = lock_taken_spots(base_probe,taken);
						all_taken_spots[count_bases++] = (taken | ((uint32_t)0x1 << k));
						c = c & ~(c2);  // no need to look at base probe members anymore ... remaining bits are other buckets
					}
				}

				for ( uint8_t i = 0; i < count_bases; i++ ) {
					all_taken_spot_refs[i]->store(all_taken_spots[i],std::memory_order_release);
				}

			}
		}






		/**
		 * a_remove_back_taken_spots
		*/

		void a_remove_back_taken_spots(hh_element *hash_base, uint32_t dist_base, hh_element *buffer, hh_element *end) {
			//
			uint8_t g = NEIGHBORHOOD - 1;
			auto last_view = (g - dist_base);	// if at the base, this is the full window. 
												// For members, find bases a full window from the member, up from the base
			//
			if ( last_view >  0 ) { // can't influence any change below the bucket
				hh_element *vb_probe = hash_base - last_view;
				vb_probe = el_check_beg_wrap(vb_probe,buffer,end);
				uint32_t c = 0;
				uint8_t k = g;     ///  (dist_base + last_view) :: dist_base + (g - dist_base) ::= dist_base + (NEIGHBORHOOD - 1) - dist_base ::= (NEIGHBORHOOD - 1) ::= g
				while ( vb_probe != hash_base ) {
					if ( vb_probe->c.bits & 0x1 ) {
						vb_probe->->tv.taken &= (~((uint32_t)0x1 << k));  // the bit is not as far out
						c = vb_probe->c.bits;
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & vb_probe->->tv.taken;
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					if ( base_probe->c.bits & 0x1 ) {
						auto j = k;
						j -= offset_nxt;
						base_probe->->tv.taken &= (~((uint32_t)0x1 << j));  // the bit is not as far out
						c = c & ~(base_probe->c.bits);  // no need to look at base probe members anymore ... remaining bits are other buckets
					}
				}
			}
		}


		/**
		 * shift_membership_spots  -- do the swappy read
		*/
		hh_element *a_shift_membership_spots(hh_element *hash_base,hh_element *vb_nxt,uint32_t c, hh_element *buffer, hh_element *end) {
			while ( c ) {
				hh_element *vb_probe = hash_base;
				auto offset = get_b_offset_update(c);
				if ( c ) {
					vb_probe += offset;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					//
					
					atomic<uint32_t> *k_aptr = (atomic<uint32_t> *)(&(vb_probe->c.key));
					atomic<uint32_t> *nk_aptr = (atomic<uint32_t> *)(&(vb_nxt->c.key));
					auto ky = k_aptr->exchange(pky,std::memory_order_acq_rel);
					nk_aptr->store(ky);
					//
					atomic<uint32_t> *v_aptr = (atomic<uint32_t> *)(&(vb_probe->->tv.value));
					atomic<uint32_t> *nv_aptr = (atomic<uint32_t> *)(&(vb_nxt->->tv.value));
					auto val = v_aptr->exchange(pval,std::memory_order_acq_rel);
					nv_aptr->store(val);
					//
					// not that the c_bits are note copied... for members, it is the offset to the base, while the base stores the memebership bit pattern
					swap(vb_nxt->->tv.taken,vb_probe->->tv.taken);  // when the element is not a bucket head, this is time...
					vb_nxt = vb_probe;
				} else {
					return vb_nxt;
				}
			}
			return nullptr;
		}

		/**
		 * remove_bucket_taken_spots
		*/
		void a_remove_bucket_taken_spots(hh_element *hash_base,uint8_t nxt_loc,uint32_t a,uint32_t b, hh_element *buffer, hh_element *end) {
			auto c = a ^ b;   // now look at the bits within the range of the current bucket indicating holdings of other buckets.
			c = c & zero_above(nxt_loc);  // buckets after the removed location do not see the location; so, don't process them
			while ( c ) {
				hh_element *vb_probe = hash_base;
				auto offset = get_b_offset_update(c);
				vb_probe += offset;
				if ( vb_probe->c.bits & 0x1 ) {
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					vb_probe->->tv.taken &= (~((uint32_t)0x1 << (nxt_loc - offset)));   // the bit is not as far out
				}
			}
		}

		/**
		 * removal_taken_spots
		*/
		void a_removal_taken_spots(hh_element *hash_base,hh_element *hash_ref,uint32_t c_pattern,hh_element *buffer, hh_element *end) {
			//
			uint32_t a = c_pattern; // membership mask
			uint32_t b = hash_base->->tv.taken;   // load this or rely on lock
			//

			//
			hh_element *vb_last = nullptr;

			auto c = a;   // use c as temporary
			//
			if ( hash_base != hash_ref ) {  // if so, the bucket base is being replaced.
				uint32_t offset = (hash_ref - hash_base);
				c = c & ones_above(offset);
			}
			//
			// membership bits are currently locked preventing a read..
			vb_last = shift_membership_spots(hash_base,hash_ref,c,buffer,end);   // SHIFT leaving the hole at the end
			if ( vb_last == nullptr ) return;

			// vb_probe should now point to the last position of the bucket, and it can be cleared...
			vb_last->c.bits = 0;
			vb_last->->tv.taken = 0;
			vb_last->c.key = 0;
			vb_last->->tv.value = 0;

			uint8_t nxt_loc = (vb_last - hash_base);   // the spot that actually cleared...

			// recall, a and b are from the base
			// clear the bits for the element being removed
			UNSET(a,nxt_loc);
			UNSET(b,nxt_loc);
			hash_base->c.bits = a;
			hash_base->->tv.taken = b;

			// OTHER buckets
			// now look at the bits within the range of the current bucket indicating holdings of other buckets.
			// these buckets are ahead of the base, but behind the cleared position...
			remove_bucket_taken_spots(hash_base,nxt_loc,a,b,buffer,end);
			// look for bits preceding the current bucket
			remove_back_taken_spots(hash_base, nxt_loc, buffer, end);
		}



		/**
		 * _cropper
		 * 
		 * 
		*/

		void _cropper(hh_element *base, uint32_t cbits, hh_element *buffer, hh_element *end,uint8_t thread_id) {
			//
			atomic<uint32_t> *base_bits = (atomic<uint32_t> *)(&(base->c.bits));
			atomic<uint32_t> *base_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			uint32_t h_bucket = (uint32_t)(base - buffer);

			auto locking_bits = gen_bits_editor_active(thread_id);   // editor active
			
			while( !become_bucket_state_master(base_bits, cbits, locking_bits, thread_id) ) {
				tick();
			}
			//
			hh_element *href = _get_bucket_reference(base_bits, base, cbits, h_bucket, UINT32_MAX, buffer, end, thread_id);
			if ( href ) {
				//
				// clear mobility lock

				// mark as deleted 
				tbits_wait_for_readers(base_tbits);
				uint8_t offset = href - base;

				hh_element *deletions[NEIGHBORHOOD];
				uint8_t del_count = 0;

				auto c =  ones_above(offset-1) & cbits;
				auto c_end = c;
				while ( c ) {

					auto vb_probe = base;
					auto offset = get_b_offset_update(c);
					vb_probe += offset;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					//
					atomic<uint32_t> *probe_bits = (atomic<uint32_t> *)(&(vb_probe->c.bits));
					uint32_t p_cbits = 0;
					if ( ready_for_delete(probe_bits,p_cbits) ) {
						//
						cbits = cbit_clear_bit(cbits,offset);
						//
					}

				}

				update_cbits(base,cbits);			// update cbits

				for ( uint8_t i = 0; i < del_count; i++ ) {
					auto vb_probe = deletions[i];
					p_cbits = gen_bitsdeleted(thread_id,p_cbits);
					atomic<uint32_t> *probe_bits = (atomic<uint32_t> *)(&(vb_probe->c.bits));
					probe_bits->store(p_cbits);
				}

				a_removal_taken_spots(base, href, cbits, buffer, end);

				for ( uint8_t i = 0; i < del_count; i++ ) {
					auto vb_probe = deletions[i];
					atomic<uint64_t> *bnky = (atomic<uint64_t> *)(&(vb_probe->c));
					bnky->store(0);
					atomic<uint64_t> *tv = (atomic<uint64_t> *)(&(vb_probe->tv));
					tv->store(0);
				}

			}
		}


		/**
		 * usurp_membership_position_hold_key 
		 * 
		 * When usurping, the taken spots of all bases within the window don't change. 
		 * The new base needs map of the current taken spots, which it takes from the other
		 * bases around it. Since the base including the usurped position in its members is below 
		 * the the new base position, the taken positions at the new position and above can be taken 
		 * from the upper part of the base's taken positions. 
		 * 
		 * c_bits - the c_bits field from hash_ref
		*/

		uint8_t usurp_membership_position_hold_key(hh_element *hash_ref, uint32_t c_bits, uint32_t el_key, uint32_t offset_value, uint8_t which_table, hh_element *buffer, hh_element *end) {
			uint8_t k = 0xFF & (c_bits >> 1);  // have stored the offsets to the bucket head
			// Original base, he bucket head
			hh_element *base_ref = (hash_ref - k);  // base_ref ... the base that owns the spot
			base_ref = el_check_beg_wrap(base_ref,buffer,end);  // and maybe wrap
			//
			atomic<uint32_t> *b_c_bits = (atomic<uint32_t> *)(&base_ref->c.bits);
			auto b_c = b_c_bits->load(std::memory_order_acquire);
			auto b_c_prev = b_c;
			UNSET(b_c,k);   // the element has been usurped...
			while ( !(b_c_bits->compare_exchange_weak(b_c_prev,b_c,std::memory_order_release)) ) { b_c = b_c_prev; UNSET(b_c,k); }

			// Need ... the allocation map for the new base element.
			// BUILD taken spots for the new base being constructed in the usurped position
			uint32_t ts = 1 | (base_ref->->tv.taken >> k);   // start with as much of the taken spots from the original base as possible
			auto hash_nxt = base_ref + (k + 1);
			for ( uint8_t i = (k + 1); i < NEIGHBORHOOD; i++, hash_nxt++ ) {
				if ( hash_nxt->c.bits & 0x1 ) { // if there is another bucket, use its record of taken spots
					// atomic<uint32_t> *hn_t_bits = (atomic<uint32_t> *)(&);  // hn_t_bits->load(std::memory_order_acquire);
					//
					auto taken = hash_nxt->->tv.taken;
					ts |= (taken << i); // this is the record, but shift it....
					//
					break;
				} else if ( hash_nxt->->tv.value != 0 ) {  // set the bit as taken
					SET(ts,i);
				}
			}
			hash_ref->->tv.taken = ts;  // a locked position
			hash_ref->c.bits = 0x1;

			hash_ref->->tv.value = offset_value;  // put in the new values
			return k;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	public: 

		/**
		 * _hlpr_select_insert_buffer
		 * 
		 * 	helper method that looks at the bucket count from two differnt slices and returns the choice for insertion.
		 * 	If the counts are the same, it calls the random bit generator to settle the choice.
		 * 
		 * 	parameters:
		 * 		count0 : max 32 int == number elements in slice 0 bucket
		 * 		count1 : max 32 in  == number elements in slice 1 bucket
		 * 
		 * 		<-- called by _get_member_bits_slice_info
		 * 	returns 1 or 0 as the slice choice. returns UINT8_MAX for out of bounds
		*/
		uint8_t _hlpr_select_insert_buffer(uint8_t count0,uint8_t count1) {
			uint8_t = which_table = 0;
			if ( (count_0 >= COUNT_MASK) && (count_1 >= COUNT_MASK) ) {
				return UINT8_MAX;
			}
			if ( count_0 >= COUNT_MASK ) {
				which_table = 1;
			}
			if ( count_1 >= COUNT_MASK ) {
				which_table = 0;
			} else {
				if ( count_1 < count_0 ) {
					which_table = 1;
				} else if ( count_0 < count_1 ) {
					which_table = 0;
				} else {
					uint8_t bit = pop_shared_bit();
					if ( bit ) {
						which_table = 1;
					}
				}
			}
			//
			return which_table;
		}

	public:

		atomic_flag		 				*_rand_gen_thread_waiting_spinner;
		atomic_flag		 				*_random_share_lock;
		//
		atomic_flag						_sleeping_reclaimer;
		atomic_flag						_sleeping_cropper;

};


#endif // _H_HOPSCOTCH_HASH_SHM_