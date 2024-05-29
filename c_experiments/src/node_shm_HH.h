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

typedef struct KEY_VALUE<uint32_t> key_value;


typedef struct HH_element {
	uint32_t			taken_spots;
	uint32_t			c_bits;   // control bit mask
	union {
		key_value		_kv;
		uint64_t		_V;
	};
} hh_element;





/*
	QueueEntryHolder ...
*/

/** 
 * q_entry is struct Q_ENTRY
*/
typedef struct Q_ENTRY {
	public:
		//
		hh_element 	*hash_ref;
		uint32_t 	h_bucket;
		uint64_t 	loaded_value;
		hh_element	*buffer;
		hh_element	*end;
		uint8_t		hole;
		uint8_t		which_table;
		uint8_t		thread_id;
		//
		uint8_t __rest[1];
} q_entry;


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



typedef struct PRODUCT_DESCR {
	//
	uint32_t						partner_thread;
	uint32_t						stats;
	ThreadActivity::ta_ops			op;
	QueueEntryHolder<>				_process_queue[2];

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
class HH_map : public HMap_interface, public Random_bits_generator<> {
	//
	public:

		// LRU_cache -- constructor
		HH_map(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) {
			_reason = "OK";
			//
			_region = region;
			_endof_region = _region + seg_sz;
			//
			_num_threads = num_threads;
			_sleeping_reclaimer.clear();
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


		virtual ~HH_map() {
		}


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
			
			//
			_region_C = (uint32_t *)(_region_HV_1_end);  // these start at the same place offset from start of second table
			_end_region_C = _region_C + max_count;  // *sizeof(uint32_t)

		// threads ...
			auto proc_regions_size = num_threads*sizeof(proc_descr);
			_process_table = (proc_descr *)(_end_region_C);
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
				if ( check_end((uint8_t *)_region_C) && check_end((uint8_t *)_end_region_C) ) {
					memset((void *)(_region_C), 0, c_regions_size);
				} else {
					cout << "WRONG HERE: " << ((uint8_t *)_end_region_C - _endof_region) << endl;
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
		 * clear
		*/
		void clear(void) {
			if ( _initializer ) {
				uint8_t sz = sizeof(HHash);
				uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
				this->setup_region(_initializer,header_size,_max_count,_num_threads);
			}
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




		inline hh_element *bucket_at(hh_element *buffer,uint32_t h_start) {
			return (hh_element *)(buffer) + h_start;
		}



		/**
		 * bucket_at 
		 * 
		 * bucket_at(buffer, h_start, end)
		*/

		inline hh_element *bucket_at(hh_element *buffer,uint32_t h_start,hh_element *end) {
			hh_element *el = buffer + h_start;
			el =  el_check_end_wrap(el,buffer,end);
			return el;
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
			nanosleep(&request, &remaining);
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


	
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


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


	
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



		// HH_map method -- 
		//
		/**
		 * the offset_value is taken from the memory allocation ... it is the index into the LRU array of objects
		 * 
		 * The contention here is not between the two threads that each examine different memory regions.
		 * However, other processes may content for the same buckets that these threads attempt to manipulate.
		 * 
		*/

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * slice_bucket_lock
		*/

		atomic<uint32_t> * slice_bucket_lock(uint32_t h_bucket,uint8_t which_table,uint8_t thread_id = 1) {   // where are the bucket counts??
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			return slice_bucket_lock(controller,which_table,thread_id);
		}


		/**
		 * slice_bucket_lock
		*/
		atomic<uint32_t> *slice_bucket_lock(atomic<uint32_t> *controller,uint8_t which_table, uint8_t thread_id = 1) {
			if ( controller == nullptr ) return nullptr;
			//
			auto controls = controller->load(std::memory_order_acquire);
			this->bucket_lock(controller,controls,thread_id);
			this->to_slice_transition(controller,controls,which_table,thread_id);
			//
			return controller;
		}



		/**
		 * slice_unlock_counter --
		*/

		void slice_unlock_counter(uint32_t h_bucket,uint8_t which_table,uint8_t thread_id = 1) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			slice_unlock_counter(controller,which_table,thread_id);
		}


		/**
		 * slice_unlock_counter_controls -- 
		*/
		void slice_unlock_counter_controls(atomic<uint32_t> *controller,uint32_t controls,uint8_t which_table,uint8_t thread_id = 1) {
			if ( controller == nullptr ) return;
			controls = which_table ? (controls & FREE_BIT_ODD_SLICE_MASK) : (controls & FREE_BIT_EVEN_SLICE_MASK);
			store_and_unlock_controls(controller,controls,thread_id);
		}


		/**
		 * slice_unlock_counter_controls -- 
		*/
		void slice_unlock_counter_controls(uint32_t h_bucket, uint32_t controls, uint8_t which_table,uint8_t thread_id = 1) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			slice_unlock_counter_controls(controller,controls,which_table,thread_id);
		}

		/**
		 * slice_unlock_counter -- 
		*/
		void slice_unlock_counter(atomic<uint32_t> *controller,uint8_t which_table,uint8_t thread_id = 1) {
			if ( controller == nullptr ) return;
			auto controls = controller->load(std::memory_order_acquire);
			controls = which_table ? (controls & FREE_BIT_ODD_SLICE_MASK) : (controls & FREE_BIT_EVEN_SLICE_MASK);
			store_and_unlock_controls(controller,controls,thread_id);
		}


		/**
		 * slice_bucket_count_incr_unlock
		*/
		void slice_bucket_count_incr_unlock(uint32_t h_bucket, uint8_t which_table, uint8_t thread_id) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			slice_bucket_count_incr_unlock(controller,which_table,thread_id);
		}

		/**
		 * slice_bucket_count_incr_unlock
		*/
		void slice_bucket_count_incr_unlock(atomic<uint32_t> *controller, uint8_t which_table,uint8_t thread_id) {
			if ( controller == nullptr ) return;
			uint8_t count_0 = 0;
			uint8_t count_1 = 0;
			uint32_t controls = 0;
			bucket_counts_lock(controller, controls, thread_id, count_0, count_1);
			if ( which_table ) {
				if ( count_1 < COUNT_MASK ) count_1++;
			} else {
				if ( count_0 < COUNT_MASK ) count_0++;
			}
			controls = update_slice_counters(controls,count_0,count_1);
			slice_unlock_counter_controls(controller, controls, which_table, thread_id);
		}


		/**
		 * slice_bucket_count_decr_unlock
		*/
		void slice_bucket_count_decr_unlock(uint32_t h_bucket, uint8_t which_table, uint8_t thread_id) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			slice_bucket_count_decr_unlock(controller,which_table,thread_id);
		}


		void slice_bucket_count_decr_unlock(atomic<uint32_t> *controller, uint8_t which_table, uint8_t thread_id) {
			//
			uint8_t count_0 = 0;
			uint8_t count_1 = 0;
			uint32_t controls = 0;
			bucket_counts_lock(controller, controls, thread_id, count_0, count_1);
			if ( which_table ) {
				if ( count_1 > 0 ) count_1--;
			} else {
				if ( count_0 > 0 ) count_0--;
			}
			///
			controls = update_slice_counters(controls,count_0,count_1);
			slice_unlock_counter_controls(controller, controls, which_table, thread_id);
		}




		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * bit patterns
		 * 
		*/

		// thread id's must be one based... (from 1)

		/**
		 * thread_id_of
		*/
		uint32_t thread_id_of(uint32_t controls) {
			uint32_t thread_id =  ((controls & THREAD_ID_SECTION) >> THREAD_ID_SHIFT);
			return thread_id & 0xFF;
		}

		/**
		 * bitp_stamp_thread_id - bit partner stamp thread id
		*/

		uint32_t bitp_stamp_thread_id(uint32_t controls,uint32_t thread_id) {
			controls = controls & THREAD_ID_SECTION_CLEAR_MASK; // CLEAR THE SECTION
			controls = controls | ((thread_id & THREAD_ID_BASE) << THREAD_ID_SHIFT) | RESIDENT_BIT_SET;  // this thread is now resident
			return controls;
		}

		/**
		 * check_thread_id
		*/
		bool check_thread_id(uint32_t controls,uint32_t lock_on_controls) {
			uint32_t vCHECK_MASK = THREAD_ID_SECTION | HOLD_BIT_SET;
			bool check = ((controls & vCHECK_MASK) == (lock_on_controls & vCHECK_MASK));
			return check;
		}




		/**
		 * bucket_blocked
		*/
		// The control bit and the shared bit are mutually exclusive. 
		// They can bothe be off at the same time, but not both on at the same time.

		bool bucket_blocked(uint32_t controls,uint32_t &thread_id) {
			if ( (controls & HOLD_BIT_SET) || (controls & RESIDENT_BIT_SET) ) return true;
			if ( (controls & THREAD_ID_SECTION) != 0 ) {  // no locks but there is an occupant (for a slice level)
				if ( thread_id_of(controls) == thread_id ) return true;  // the calling thread is that occupant
			}
			return false; // no one is there or it is another thread than the calling thread and it is at the slice level.
		}

		/**
		 * bitp_partner_thread -- make the thread pattern that this will have if it were to be the last to a partnership.
		*/
		uint32_t bitp_partner_thread(uint32_t thread_id,uint32_t controls,uint32_t &thread_salvage) {
			thread_salvage = thread_id_of(controls);  //
			// overwrite the stored thread id (effectively loses the currently resident thread, but yeilds that to the caller)
			auto p_controls = bitp_stamp_thread_id(controls,thread_id) | PARTNERED_BIT_SET;  // both resident bit and partner
			return p_controls;
		}


		/**
		 * bitp_clear_thread_stamp_unlock
		*/
		uint32_t bitp_clear_thread_stamp_unlock(uint32_t controls) {
			controls = (controls & (THREAD_ID_SECTION_CLEAR_MASK & FREE_HOLD_BIT_MASK)); // CLEAR THE SECTION
			return controls;
		}

		/**
		 * table_store_partnership -- 
		*/
		void table_store_partnership(uint32_t ctrl_thread,uint32_t thread_id) {
			_process_table[ctrl_thread].partner_thread = thread_id;
			_process_table[thread_id].partner_thread = ctrl_thread;
		}

		/**
		 * table_store_partnership -- 
		*/
		bool partnered(uint32_t controls, uint32_t ctrl_thread, uint32_t thread_id) {
			if ( controls & PARTNERED_BIT_SET ) {
				auto t_check = thread_id_of(controls);
				if ( (t_check == ctrl_thread) || (t_check == thread_id) ) {
					if ( _process_table[ctrl_thread].partner_thread == thread_id ) {
						if ( _process_table[thread_id].partner_thread == ctrl_thread ) {
							return true;
						}
					}
				}
			}
			return false;
		}


		bool threads_are_partnered(uint32_t ctrl_thread, uint32_t &partner_thrd_id) {
			if ( _process_table[ctrl_thread].partner_thread == 0 ) {
				return false;
			}
			partner_thrd_id =  _process_table[ctrl_thread].partner_thread;  // output partner id
			if ( _process_table[partner_thrd_id].partner_thread == ctrl_thread ) {
				return true;  // there was a thread id and the relationship is not broken
			}
			return false;
		}


		bool partnered_at_all(uint32_t controls, uint32_t ctrl_thread, uint32_t thread_id) {
			if ( controls & RESIDENT_BIT_SET ) {
				return ( partnered(controls, ctrl_thread, thread_id) );
			}
			return false;
		}


		uint32_t ensure_shared_bits_set_atomic(atomic<uint32_t> *controller, uint32_t set_bits) {
			auto controls = controller->load(std::memory_order_acquire);
			auto update_ctrls = controls | set_bits;
			while ( !controller->compare_exchange_weak(controls,update_ctrls,std::memory_order_acq_rel) );
			return controller->load();
		}



		/**
		 * store_relinquish_partnership
		 * 
		 * the method, partnered, should have already been called 
		*/
		//
		void store_relinquish_partnership(atomic<uint32_t> *controller, uint32_t controls, uint32_t thread_id) {
			auto hold_bit = controls & HOLD_BIT_SET;
			auto ctrl_thread = thread_id_of(controls);
			if ( ctrl_thread == thread_id ) {
				auto partner = _process_table[thread_id].partner_thread;  // this partner still has his slice.
				auto partner_controls = bitp_stamp_thread_id(controls,partner);
				if ( partner != 0 ) {
					partner_controls = (partner_controls | RESIDENT_BIT_SET | hold_bit) & QUIT_PARTNERED_BIT_MASK;
				} else {
					partner_controls = (partner_controls & QUIT_PARTNERED_BIT_MASK & THREAD_ID_SECTION_CLEAR_MASK);
				}
				auto prev_controls = controls;
				while ( !controller->compare_exchange_weak(controls,partner_controls,std::memory_order_acq_rel) && (prev_controls == controls) );
			} else {
				if ( _process_table[thread_id].partner_thread == thread_id ) {
					_process_table[thread_id].partner_thread = 0;
					auto partner_controls = (controls  | RESIDENT_BIT_SET | hold_bit) & QUIT_PARTNERED_BIT_MASK;
					auto prev_controls = controls;
					while ( !controller->compare_exchange_weak(controls,partner_controls,std::memory_order_acq_rel) && (prev_controls == controls) );
				}
			}
		}

/// 			auto controls = controller->load(std::memory_order_acquire);


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// store_and_unlock_controls ----
		uint32_t store_and_unlock_controls(atomic<uint32_t> *controller,uint32_t controls,uint8_t thread_id) {
			//
			if ( !(controls & RESIDENT_BIT_SET) ) {  // this has been relinquished by any number of partners
				// just one thread id in play at this bucket...  (hold bit is to be unlocked) 
				auto cleared_controls = bitp_clear_thread_stamp_unlock(controls);
				while ( !(controller->compare_exchange_weak(controls,cleared_controls,std::memory_order_acq_rel,std::memory_order_relaxed)) ) {
					// Some thread (not the caller) has validly acquired the bucket.
					if (( thread_id_of(controls) != thread_id) && (controls & HOLD_BIT_SET) ) {
						break;
					}
				}
			} else if ( controls & PARTNERED_BIT_SET ) {
				store_relinquish_partnership(controller, controls, thread_id);
			}



			if ( (controls & HOLD_BIT_SET) || (controls & RESIDENT_BIT_SET) ) {
				//
				if ( !(controls & PARTNERED_BIT_SET) ) {
					// just one thread id in play at this bucket...  (only hold bit)
					auto cleared_controls = bitp_clear_thread_stamp_unlock(controls);
					while ( !(controller->compare_exchange_weak(controls,cleared_controls,std::memory_order_acq_rel,std::memory_order_relaxed)) ) {
						// Some thread (not the caller) has validly acquired the bucket.
						if (( thread_id_of(controls) != thread_id) && (controls & HOLD_BIT_SET) ) {
							break;
						}
					}
				} else if ( controls & HOLD_BIT_SET ) {
					// the thread id might not be in the controls word.
					// but, there should be a relationship in which the thread id is either referenced by thread_id_of(controls)
					// or the thread id is in the control word and it references a partner...
					// If the thread id does not reference a partner, any partnership info should be cleared out anyway.
					auto partner = _process_table[thread_id].partner_thread;  // this partner still has his slice.
					auto partner_controls = bitp_stamp_thread_id(controls,partner);
					partner_controls = partner_controls & FREE_HOLD_BIT_MASK;   // leave the shared bit alone... 
					if ( partner == 0 ) {
						partner_controls = partner_controls & QUIT_SHARE_BIT_MASK;  // about the same state as in the above condition.
					}
					_process_table[thread_id].partner_thread = 0;		// but break the partnership
					_process_table[partner].partner_thread = 0;
					while ( !(controller->compare_exchange_weak(controls,partner_controls,std::memory_order_acq_rel,std::memory_order_relaxed)) ) {
						// Some thread (not the caller) has validly acquired the bucket.
						if (( thread_id_of(controls) != thread_id) && ((controls & HOLD_BIT_SET) || (controls & RESIDENT_BIT_SET)) ) {
							break;  // The shared bit might be set if partner_controls succeeded yielding the bucket to the one partner.
									// or, some other thread moved into position.
						}
					}
				}
				//
			}
			return controls;
		}

 

		/**
		 * bucket_lock
		*/

		atomic<uint32_t> * bucket_lock(uint32_t h_bucket,uint32_t &ret_control,uint8_t thread_id) {   // where are the bucket counts??
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			return bucket_lock(controller,ret_control,thread_id);
		}


		atomic<uint32_t> * bucket_lock(atomic<uint32_t> *controller,uint32_t &ret_control,uint8_t thread_id) {   // where are the bucket counts??
			//
			uint32_t controls = controller->load(std::memory_order_acquire);  // one controller for two slices
			//
			uint32_t lock_on_controls = 0;  // used after the outter loop
			uint8_t thread_salvage = 0;
			do {
				while ( controls & HOLD_BIT_SET ) {  // while some other process is using this count bucket
					//
					controls = controller->load(std::memory_order_acquire);
					//  -------------------
					if ( controls & HOLD_BIT_SET ) {
						auto ctrl_thread = thread_id_of(controls);
						if ( (ctrl_thread == thread_id) && (controls & RESIDENT_BIT_SET) ) {  // this thread owns its part here..
							// Clear the hold bit in any way possible (because thread_id is blocking itself)
							// store_unlock_controller will either (1) clear the control word of any thread
							// or (2) remove thread_id from a partnership. 
							auto pbit = controls & PARTNERED_BIT_SET; // don't lose this information...
							ret_control = ensure_shared_bits_set_atomic(controller, HOLD_BIT_SET | RESIDENT_BIT_SET | pbit);
							return controller;
						}
						if ( partnered_at_all(controls, ctrl_thread, thread_id) ) {  // that is, still one of the residents (moved partner)
							ret_control = ensure_shared_bits_set_atomic(controller, HOLD_BIT_SET | PARTNERED_BIT_SET | RESIDENT_BIT_SET );
							return controller;
						}
					}
					tick(); // OK to let other things run 
					// else continue waiting for something to release the hold...
				}

				//  At this point the bucket is cleared of thread id. And the hold bit is not set.
				// 	Make sure the expected pattern of storage makes sense. And, write the sensible pattern 
				//	indicating pure lock or shared lock.  (Prepare to update the the controller in shared memory.)
				// 
				if ( !(controls & RESIDENT_BIT_SET) ) {  // no one is in the bucket
					lock_on_controls = bitp_stamp_thread_id(controls,thread_id) | HOLD_BIT_SET;  // make sure that this is the thread that sets the bit high
				} else if ( !(controls & PARTNERED_BIT_SET) ) {   // Indicates that another thread (stored in controls) can share.
					// This means there is a free slice and thread_id could access that slice.
					// So, produce the bit pattern that will indicate sharing from this point,
					// i.e., overwrite, thread_id will be in the control word while the current one will be moved to salvage (in tables later)
					// pass the `controls` word (it has info that must be preserved) ... don't change the process tables yet
					lock_on_controls = bitp_partner_thread(thread_id,controls,thread_salvage) | HOLD_BIT_SET; // SETS SHARED BIT
				} // otherwise, the partner is someone else thread_id has to wait until one of them exists. But, those threads might have exited...
				//
				// (cew) returns true if the value changes ... meaning the lock is acquired by someone since the last load
				// i.e. loop exits if the value changes from no thread id and clear bit to at least the bit
				// or it changes from the partner thread and shared bit set to thread_id and hold bit set and shared bit still set.
				auto prev_controls = controls;
				while ( !(controller->compare_exchange_weak(controls,lock_on_controls,std::memory_order_release,std::memory_order_relaxed)) && (prev_controls == controls) );
				// but we care if this thread is the one that gets the bit.. so check to see that `controls` contains 
				// this thread id and the `HOLD_BIT_SET`
				controls = controller->load();  // guarding against suprious completion of storage...
			  // allow for the possibility that another thread got in
			} while ( !(check_thread_id(controls,lock_on_controls)) );  // failing this, we wait on the other thread to give up the lock..
			//
			// The thread is locked and if partnered, the partnership can be stored without interference at this point.
			if ( controls & PARTNERED_BIT_SET ) {  // at this point thread_id is stored in controls, and if shared bit, then partnership
				// if here, then two threads are controlling the bucket but in different slices
				table_store_partnership(thread_id,thread_salvage); // thread_salvage has to have been set.
			}
			// at this point this thread should own the bucket...
			ret_control = lock_on_controls;  // return the control word that was loaded and change to carrying this thread id and lock bit
			return controller;
		}

		/**
		 * to_slice_transition
		 * 
		 *			this hold bit must be set prior to calling this method.
		*/

		void to_slice_transition(atomic<uint32_t> *controller, uint32_t controls, uint8_t which_table, uint32_t thread_id) {
			auto pre_store = controller->load(std::memory_order_acquire);
			if ( !(controls & RESIDENT_BIT_SET) ) {
				controls = controls | RESIDENT_BIT_SET;
			}
			if ( thread_id_of(controls) != thread_id ) {
				table_store_partnership(thread_id,thread_id_of(controls)); // thread_salvage has to have been set.
			}
			controls = which_table ? (controls | HOLD_BIT_ODD_SLICE) : (controls | HOLD_BIT_EVEN_SLICE);
			while ( !(controller->compare_exchange_weak(pre_store,controls,std::memory_order_acq_rel,std::memory_order_relaxed)) );  //  && (count_loops_2++ < 1000)  && !(controls & HOLD_BIT_SET)
		}



		/**
		 * store_unlock_controller
		*/

		uint32_t store_unlock_controller(uint32_t h_bucket,uint8_t thread_id) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			return store_unlock_controller(controller,thread_id);
		}

		uint32_t store_unlock_controller(atomic<uint32_t> *controller,uint8_t thread_id) {
			uint32_t controls = controller->load(std::memory_order_acquire);
			return store_and_unlock_controls(controller,controls,thread_id);
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		// get an idea, don't try locking or anything  (per section counts)

		/**
		 * get_bucket_counts - hash bucket offset
		 * 
		 * 		Accesses the bucket counts stored in the controller.
		 * 		get_bucket_counts methods do not lock the hash bucket
		*/
		pair<uint8_t,uint8_t> get_bucket_counts(uint32_t h_bucket) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			return get_bucket_counts(controller);
		}

		/**
		 * get_bucket_counts - controller
		*/
		pair<uint8_t,uint8_t> get_bucket_counts(atomic<uint32_t> *controller) {
			uint32_t controls = controller->load(std::memory_order_acquire);
			pair<uint8_t,uint8_t> p;
			p.first  = controls & COUNT_MASK;
			p.second = (controls>>EIGHTH) & COUNT_MASK;
			return p;
		}


		// shared bucket count for both segments....

		/**
		 * bucket_counts
		 * 		Accesses the bucket counts stored in the controller.
		 * 		Leaves the bucket lock upon return. 
		 * 		The caller must call a method to unlock the buffer. 
		*/
		atomic<uint32_t> *bucket_counts_lock(uint32_t h_bucket, uint32_t &controls, uint8_t thread_id, uint8_t &count_0, uint8_t &count_1) {   // where are the bucket counts??
			//
			auto controller = this->bucket_lock(h_bucket,controls,thread_id);
			//
			count_0 = (controls & COUNT_MASK);
			count_1 = (controls>>EIGHTH) & COUNT_MASK;
			//
			return (controller);
		}

		atomic<uint32_t> *bucket_counts_lock(atomic<uint32_t> *controller, uint32_t &controls, uint8_t thread_id, uint8_t &count_0, uint8_t &count_1) {   // where are the bucket counts??
			//
			this->bucket_lock(controller,controls,thread_id);
			//
			count_0 = (controls & COUNT_MASK);
			count_1 = (controls>>EIGHTH) & COUNT_MASK;
			//
			return (controller);
		}
		//  ----


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * update_slice_counters
		*/
		uint32_t update_slice_counters(uint32_t controls,uint32_t count_0,uint32_t count_1) {
			//
			auto counter = controls & COUNT_MASK;
			if ( counter < (uint8_t)COUNT_MASK ) {
				controls = (controls & ~COUNT_MASK) | (COUNT_MASK & count_0);
			}

			counter = (controls>>EIGHTH) & COUNT_MASK;
			if ( counter < (uint8_t)COUNT_MASK ) {
				uint32_t update = (count_1 << EIGHTH) & HI_COUNT_MASK;
				controls = (controls & ~HI_COUNT_MASK) | update;
			}
			//
			return controls;
		}

		/**
		 * update_count_incr -- updates the counter grand total over segments
		*/

		uint32_t update_count_incr(uint32_t controls) {
			auto counter = ((controls & DOUBLE_COUNT_MASK) >> QUARTER);   // update the counter for both buffers.
			if ( counter < (uint8_t)DOUBLE_COUNT_MASK_BASE ) {
				counter++;
				uint32_t update = (counter << QUARTER) & DOUBLE_COUNT_MASK;
				controls = (controls & ~DOUBLE_COUNT_MASK) | update;
			}
			return controls;
		}



		/**
		 * update_count_decr
		*/

		uint32_t update_count_decr(uint32_t controls) {
			auto counter = ((controls & DOUBLE_COUNT_MASK) >> QUARTER);   // update the counter for both buffers.
			if ( counter > 0 ) {
				counter--;
				uint32_t update = (counter << QUARTER) & DOUBLE_COUNT_MASK;
				controls = (controls & ~DOUBLE_COUNT_MASK) | update;
			}
			return controls;
		}


		/**
		 * bucket_count_incr
		*/
		void bucket_count_incr(uint32_t h_bucket, uint8_t thread_id) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			bucket_count_incr(controller,thread_id);
		}


		uint32_t bucket_count_incr(atomic<uint32_t> *controller, uint8_t thread_id) {
			if ( controller == nullptr ) return 0;
			//
			uint32_t controls = controller->load(std::memory_order_acquire);
			auto pre_controls = controls;
			while ( !(controls & HOLD_BIT_SET) ) {	// should be the only one able to get here on this bucket.
				this->bucket_lock(controller,controls,thread_id);  // aqcuire the lock again... if somehow this unlocked
			}
			//
			controls = update_count_incr(controls);
			//
			while ( !(controller->compare_exchange_weak(pre_controls,controls,std::memory_order_acq_rel,std::memory_order_relaxed)) );
			return controls;
		}


		void bucket_count_incr_and_unlock(atomic<uint32_t> *controller, uint8_t thread_id) {
			if ( controller == nullptr ) return;
			auto controls = bucket_count_incr(controller, thread_id);
			if ( controls > 0 ) {
				store_and_unlock_controls(controller,controls,thread_id);
			}
		}


		/**
		 * bucket_count_decr
		*/
		void bucket_count_decr(uint32_t h_bucket, uint8_t thread_id) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			bucket_count_decr(controller,thread_id);
		}


		uint32_t bucket_count_decr(atomic<uint32_t> *controller, uint8_t thread_id) {
			if ( controller == nullptr ) return 0;
			//
			uint32_t controls = controller->load(std::memory_order_acquire);
			auto pre_controls = controls;
			while ( !(controls & HOLD_BIT_SET) ) {	// should be the only one able to get here on this bucket.
				this->bucket_lock(controller,controls,thread_id);  // aqcuire the lock again...
			}
			//
			controls = update_count_decr(controls);
			//
			while ( !(controller->compare_exchange_weak(pre_controls,controls,std::memory_order_acq_rel,std::memory_order_relaxed)) );
			return controls;
		}

		void bucket_count_decr_and_unlock(atomic<uint32_t> *controller, uint8_t thread_id) {
			if ( controller == nullptr ) return;
			auto controls = bucket_count_decr(controller, thread_id);
			store_and_unlock_controls(controller,controls,thread_id);
		}





		//
		/**
		 * wait_if_unlock_bucket_counts_refs
		*/
		bool wait_if_unlock_bucket_counts_refs(uint32_t h_bucket,uint8_t thread_id,HHash **T_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,uint8_t &which_table) {
			atomic<uint32_t> *controller = nullptr;
			return wait_if_unlock_bucket_counts_refs(&controller,thread_id,h_bucket,T_ref,buffer_ref,end_buffer_ref,which_table);
		}


		/**
		 * wait_if_unlock_bucket_counts_refs
		*/
		bool wait_if_unlock_bucket_counts_refs(atomic<uint32_t> **controller_ref,uint8_t thread_id,uint32_t h_bucket,HHash **T_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,uint8_t &which_table) {
			if ( wait_if_unlock_bucket_counts(controller_ref,thread_id,h_bucket,which_table) ) {
				this->set_thread_table_refs(which_table,T_ref,buffer_ref,end_buffer_ref);
				return true;
			}
			return false;
		}

		//
		/**
		 * wait_if_unlock_bucket_counts
		*/
		bool wait_if_unlock_bucket_counts(uint32_t h_bucket,uint8_t thread_id,uint8_t &which_table) {
			atomic<uint32_t> *controller = nullptr;
			return wait_if_unlock_bucket_counts(&controller,thread_id,h_bucket,which_table);
		}


		/**
		 * wait_if_unlock_bucket_counts
		*/
		bool wait_if_unlock_bucket_counts(atomic<uint32_t> **controller_ref,uint8_t thread_id,uint32_t h_bucket,uint8_t &which_table) {
			// ----
			which_table = 0;

			uint8_t count_0 = 0;		// favor the least full bucket ... but in case something doesn't move try both
			uint8_t count_1 = 0;
			uint32_t controls = 0;

			// ----
			atomic<uint32_t> *controller = this->bucket_counts_lock(h_bucket,controls,thread_id,count_0,count_1);
			*controller_ref = controller;

			if ( (count_0 >= COUNT_MASK) && (count_1 >= COUNT_MASK) ) {
				this->store_unlock_controller(controller,thread_id);
				return false;
			}

			// 1 odd 0 even 
	
			controls = controller->load();
			if ( controls & RESIDENT_BIT_SET ) {
				if ( (controls & HOLD_BIT_ODD_SLICE) && (count_1 < COUNT_MASK) ) {
					which_table = 0;
					count_0++;
				} else if ( (controls & HOLD_BIT_EVEN_SLICE)  && (count_0 < COUNT_MASK) ) {
					which_table = 1;
					count_1++;
				} else if ( ( controls & HOLD_BIT_ODD_SLICE ) && ( controls & HOLD_BIT_EVEN_SLICE ) ) {
					// should not be here
					this->store_unlock_controller(controller,thread_id);
					return false;
				} else {
					// running out of space....
					this->store_unlock_controller(controller,thread_id);
					return false;
				}
			} else {  // changing from T0 to T1 otherwise leave it alone.
				//
				if ( (count_1 < count_0) && (count_1 < COUNT_MASK) ) {
					which_table = 1;
					count_1++;
				} else if ( (count_1 == count_0) && (count_1 < COUNT_MASK) ) {
					uint8_t bit = pop_shared_bit();
					if ( bit ) {
						which_table = 1;
						count_1++;
					} else {
						count_0++;
					}
				} else if ( count_0 < COUNT_MASK ) {
					count_0++;
				} else return false;
				//
			}

			controls = this->update_slice_counters(controls,count_0,count_1);
			controls = this->update_count_incr(controls);   // does not lock -- may roll back if adding fails
			//
			this->to_slice_transition(controller,controls,which_table,thread_id);
			return true;
		}


		/**
		 * short_list_old_entry - calls on the application's method for puting displaced values, new enough to be kept,
		 * into a short list that may be searched in the order determined by the application (e.g. first for preference, last
		 * for normal lookup except fails.)
		 * 
		*/
		bool short_list_old_entry([[maybe_unused]] uint64_t loaded_value,[[maybe_unused]] uint32_t store_time) {
			return false;
		}

		// ----

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
		void wake_up_one_restore([[maybe_unused]] uint32_t h_start) {
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
			//
			while ( is_restore_queue_empty(assigned_thread_id,slice_for_thread) ) wait_notification_restore();
			//
			dequeue_restore(&hash_ref, h_bucket, loaded_value, hole, which_table, thread_id, assigned_thread_id, &buffer, &end);
			// store... if here, it should be locked
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			//
			uint32_t store_time = now_time(); // now
			// hole -- try to gather all the work for a single bucket from the queue...
			bool quick_put_ok = pop_until_oldest(hash_ref, loaded_value, store_time, which_table, buffer, end, thread_id);
			//
			if ( quick_put_ok ) {   // now unlock the bucket... 
				this->slice_unlock_counter(controller,which_table,thread_id);
			} else {
				// unlock the bucket... the element did not get back into the bucket, but another attempt may be made
				this->slice_unlock_counter(controller,which_table,thread_id);
				//
				// if the entry can be entered onto the short list (not too old) then allow it to be added back onto the restoration queue...
				if ( short_list_old_entry(loaded_value, store_time) ) {
					uint32_t el_key = (uint32_t)((loaded_value >> HALF) & HASH_MASK);  // just unloads it (was index)
					uint32_t offset_value = (loaded_value & UINT32_MAX);
					place_in_bucket(el_key, h_bucket, offset_value, which_table, thread_id, N, buffer, end);
				}
				//
			}

		}


		/**
		 * enqueue_restore
		*/
		void enqueue_restore(hh_element *hash_ref, uint32_t h_bucket, uint64_t loaded_value, uint8_t hole, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end) {
			q_entry get_entry;
			//
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
		void dequeue_restore(hh_element **hash_ref_ref, uint32_t &h_bucket, uint64_t &loaded_value, uint8_t &hole, uint8_t &which_table, uint8_t &thread_id, uint8_t assigned_thread_id , hh_element **buffer_ref, hh_element **end_ref) {
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
			//
			*hash_ref_ref = hash_ref;
			*buffer_ref = buffer;
			*end_ref = end;
		}





		/**
		 * wakeup_value_restore
		*/

		void wakeup_value_restore(hh_element *hash_ref, uint32_t h_start, uint64_t loaded_value, uint8_t hole, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end) {
			// this queue is jus between the calling thread and the service thread belonging to just this process..
			// When the thread works it may content with other processes for the hash buckets on occassion.
			enqueue_restore(hash_ref,h_start,loaded_value,hole,which_table,thread_id,buffer,end);
			wake_up_one_restore(h_start);
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
			atomic<uint32_t> *b_c_bits = (atomic<uint32_t> *)(&base_ref->c_bits);
			auto b_c = b_c_bits->load(std::memory_order_acquire);
			auto b_c_prev = b_c;
			UNSET(b_c,k);   // the element has been usurped...
			while ( !(b_c_bits->compare_exchange_weak(b_c_prev,b_c,std::memory_order_release)) ) { b_c = b_c_prev; UNSET(b_c,k); }

			// Need ... the allocation map for the new base element.
			// BUILD taken spots for the new base being constructed in the usurped position
			uint32_t ts = 1 | (base_ref->taken_spots >> k);   // start with as much of the taken spots from the original base as possible
			auto hash_nxt = base_ref + (k + 1);
			for ( uint8_t i = (k + 1); i < NEIGHBORHOOD; i++, hash_nxt++ ) {
				if ( hash_nxt->c_bits & 0x1 ) { // if there is another bucket, use its record of taken spots
					// atomic<uint32_t> *hn_t_bits = (atomic<uint32_t> *)(&);  // hn_t_bits->load(std::memory_order_acquire);
					//
					auto taken = hash_nxt->taken_spots;
					ts |= (taken << i); // this is the record, but shift it....
					//
					break;
				} else if ( hash_nxt->_kv.value != 0 ) {  // set the bit as taken
					SET(ts,i);
				}
			}
			hash_ref->taken_spots = ts;  // a locked position
			hash_ref->c_bits = 0x1;

			hash_ref->_kv.value = offset_value;  // put in the new values
			return k;
		}


		/**
		 * create_in_bucket_at_base
		*/

		void create_in_bucket_at_base(hh_element *hash_ref, uint32_t value, uint32_t el_key) {
			hash_ref->_kv.value = value;
			hash_ref->_kv.key = el_key;
			hash_ref->c_bits = 0x1;  // locked position
			hh_element *base_ref = hash_ref;
			uint32_t c = 1;				// c will be the new base element map of taken positions
			for ( uint8_t i = 1; i < NEIGHBORHOOD; i++ ) {
				hash_ref++;								// keep walking forward to the end of the window
				if ( hash_ref->c_bits & 0x1 ) {			// this is a base bucket with information beyond the window
					atomic<uint32_t> *hn_t_bits = (atomic<uint32_t> *)(&base_ref->taken_spots);
					auto taken = hn_t_bits->load(std::memory_order_acquire);
					c |= (taken << i); // this is the record, but shift it....
					break;								// don't need to get more information at this point
				} else if ( hash_ref->c_bits != 0 ) {
					SET(c,i);							// this element belongs to a base below the new base
				}
			}
			base_ref->taken_spots = c; // finally save the taken spots.
		}





		/**
		 * add_key_value
		 * 
		*/

		// el_key == hull_hash (usually)



		uint64_t add_key_value(uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t thread_id = 1) {
			//
			uint8_t selector = 0x3;
			if ( selector_bit_is_set(h_bucket,selector) ) {
				return UINT64_MAX;
			}

			//
			// Threads from this process will not contend with each other.
			// They will work on two different memory sections.
			// The favored should set the value first. 
			//			Check thread IDs in undecided state (can be atomic)

			// About waiting on threads from the current process. Two threads are assigned to the current table.
			// The process will lock a mutex for just those two threads for the tier that has been called upon. 
			// The mutex will lock more globally. It will not interfere with other processes requesting a conversation 
			// with the threads, just current process threads at the curren tier.
			//
			auto N = this->_max_n;
			uint32_t h_start = h_bucket % N;  // scale the hash .. make sure it indexes the array...  (should not have to do this)
			//
			HHash *T = _T0;
			hh_element *buffer	= _region_HV_0;
			hh_element *end	= _region_HV_0_end;
			uint8_t which_table = 0;
			//
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_start]);
			if ( (offset_value != 0) && wait_if_unlock_bucket_counts_refs(&controller,thread_id,h_start,&T,&buffer,&end,which_table) ) {
				return place_in_bucket(el_key, h_bucket, offset_value, which_table, thread_id, N, buffer, end);
			}

			return UINT64_MAX;
		}




		//
		/**
		 * prepare_add_key_value_known_slice  ALIAS for wait_if_unlock_bucket_counts
		*/
		bool prepare_add_key_value_known_slice(uint32_t h_bucket,uint8_t thread_id,uint8_t &which_table) {
			atomic<uint32_t> *controller = nullptr;
			return wait_if_unlock_bucket_counts(&controller,thread_id,h_bucket,which_table);
		}



		/**
		 * add_key_value_known_slice
		 * 
		 * This method should be caled after `prepare_add_key_value_known_slice` which locks the bucket found from `h_bucket`.
		*/
		uint64_t add_key_value_known_slice(uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table = 0,uint8_t thread_id = 1) {
			uint8_t selector = 0x3;
			//
			if ( (offset_value != 0) &&  selector_bit_is_set(h_bucket,selector) ) {
				auto N = this->_max_n;
				//
				HHash *T = _T0;
				hh_element *buffer = _region_HV_0;
				hh_element *end	= _region_HV_0_end;
				//
				this->set_thread_table_refs(which_table,&T,&buffer,&end);
				//
				return place_in_bucket(el_key, h_bucket, offset_value, which_table, thread_id, N, buffer, end);
			}
			//
			return UINT64_MAX;
		}



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
			atomic<uint32_t> *a_c_bits = (atomic<uint32_t> *)(&(base->c_bits));
			uint32_t cbits = 0;
			if ( empty_bucket(a_c_bits,base,cbits) ) return UINT64_MAX;   // empty_bucket cbits by ref
			//
			hh_element *storage_ref = get_bucket_reference(a_c_bits, base, cbits, h_bucket, el_key, buffer, end, thread_id);  // search
			//
			if ( storage_ref != nullptr ) {
				value = storage_ref->_kv.value;
			} else {
				// search in reinsertion queue
				// maybe wait a tick and search again...
				return UINT32_MAX;
			}
			//
			return value;
		}



		/**
		 * update
		 * 
		 * Note that for this method the element key contains the selector bit for the even/odd buffers.
		 * The hash value and location must alread exist in the neighborhood of the hash bucket.
		 * The value passed is being stored in the location for the key...
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
			atomic<uint32_t> *a_c_bits = (atomic<uint32_t> *)(&(base->c_bits));
			uint32_t cbits = 0;
			if ( empty_bucket(a_c_bits,base,cbits) ) return UINT64_MAX;   // empty_bucket cbits by ref
			return _internal_update(a_c_bits, base, cbits, el_key, h_bucket, v_value, thread_id);
		}



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
			atomic<uint32_t> *a_c_bits = (atomic<uint32_t> *)(&(base->c_bits));
			uint32_t cbits = 0;
			if ( empty_bucket(a_c_bits,base,cbits) ) return UINT32_MAX;   // empty_bucket cbits by ref
			//
			// set that edit is taking place
			uint64_t loaded = _internal_update(a_c_bits, base, cbits, el_key, h_bucket, 0, thread_id, UINT32_MAX);
			if ( loaded == UINT64_MAX ) return UINT32_MAX;
			//
			submit_for_cropping(base,cbits);  // after a total swappy read, all BLACK keys will be at the end of members
			return 
		}


	protected:			// these may be used in a test class...



		uint64_t _internal_update(atomic<uint32_t> *a_c_bits, hh_element *base, uint32_t cbits, uint32_t el_key, uint32_t h_bucket, uint32_t v_value, uint8_t thread_id, uint32_t el_key_update = 0) {
			//
			hh_element *storage_ref = get_bucket_reference(a_c_bits, base, cbits, h_bucket, el_key, buffer, end, thread_id);  // search
			//
			if ( storage_ref != nullptr ) {
				// 
				if ( el_key_update != 0 ) {
					storage_ref->_kv.key = el_key_update;
				}
				//
				storage_ref->_kv.value = v_value;
				//
				uint64_t loaded_key = (((uint64_t)el_key) << HALF) | v_value; // LOADED
				loaded_key = stamp_key(loaded_key,selector);
				//
				return(loaded_key);
			}
			//
			return(UINT64_MAX); // never locked
		}



		inline bool empty_bucket(hh_element *base,uint32_t &cbits) {
			atomic<uint32_t> *a_c = (atomic<uint32_t> *)(&base->c_bits);
			cbits = a_c->load(std::memory_order_acquire);
			if ( cbits == 0 ) return true;  // no members and no back ref either (while this test for backref may be uncommon)
			if ( (cbits & 0x1) && (base->_kv.key == UINT32_MAX) && (base->_kv.value == 0) ) return true;
			return false;
		}
		inline bool empty_bucket(atomic<uint32_t> *a_c,hh_element *base,uint32_t &cbits) {
			cbits = a_c->load(std::memory_order_acquire);
			if ( cbits == 0 ) return true;  // no members and no back ref either (while this test for backref may be uncommon)
			if ( (cbits & 0x1) && (base->_kv.key == UINT32_MAX) && (base->_kv.value == 0) ) return true;
			return false;
		}

		inline void clear_bucket(hh_element *base) {   // called under lock
			atomic<uint32_t> *a_c = (atomic<uint32_t> *)(&base->c_bits);
			a_c->store(0,std::memory_order_release);
			atomic<uint32_t> *a_t = (atomic<uint32_t> *)(&base->taken_spots);
			a_t->store(0,std::memory_order_release);
			base->_V = 0;
		}
 

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		hh_element *get_bucket_reference(atomic<uint32_t> *bucket_bits, hh_element *base, uint32_t cbits, uint32_t h_bucket, uint32_t el_key, hh_element *buffer, hh_element *end,uint8_t thread_id) {
			//
			hh_element *next = base;
			//
			// bucket_bits are the base of the bucket storing the state of cbits
			//
			uint32_t original_bits = cbits_add_reader(bucket_bits,thread_id,cbits);
			if ( original_bits & 0x1 ) {
				uint32_t H = original_bits;   // cbits are from a call to empty bucket always at this point
				//
				do {
					uint32_t c = H;
					while ( c ) {
						next = base;
						uint8_t offset = get_b_offset_update(c);
						next += offset;
						next = el_check_end_wrap(next,buffer,end);
						// is this being usurped or swapped or deleted at this moment? Search on this range is locked
						auto a_n_key = (atomic<uint32_t> *)(&(next->_kv.key));
						auto chk_key = a_n_key->load(std::memory_order_acquire);
						if ( el_key == chk_key ) {
							return next;
						}
					}										// H by reference against H constant  -- might be in the process of being entered
				} while ( !(bucket_bits->compare_exchange_weak(H,H,std::memory_order_acq_rel))  );  // if it changes try again 
			} else {
				auto original_cbits = fetch_real_cbits(cbits);
				return swappy_search_ref(el_key, base, original_cbits);
			}
			return nullptr;
		}



		uint32_t obtain_cell_key(atomic<uint32_t> *a_ky) {
			uint32_t real_ky = (UINT32_MAX-1);
			while ( a_ky->compare_exchange_weak(real_ky,(UINT32_MAX-1)) && (real_ky == (UINT32_MAX-1)));
			return real_ky;
		}
		



		void wait_for_readers(atomic<uint32_t> *controller) {
			auto controls = controller->load(std::memory_order_acquire);
			while ( controls & READER_MASK != 0 ) {
				__libcpp_thread_yield();
				controls = controller->load(std::memory_order_acquire);
			}
		}


		void total_swappy_read(hh_element *base,uint32_t c) {
			hh_element *next = base;
			uint8_t blocker_count = 1;
			auto save_c = c;
			bool handle_count = true;
			while ( blocker_count-- > 0 ) {  // sweep for each blocker until they are at the end..
				c = save_c;
				while ( c ) {
					next = base;
					uint8_t offset = get_b_offset_update(c);
					next += offset;
					next = el_check_end_wrap(next,buffer,end);
					// is this being usurped or swapped or deleted at this moment? Search on this range is locked
					//
					atomic<uint32_t> *a_ky = (atomic<uint32_t> *)(&next->_kv.key);
					auto ky = obtain_cell_key(a_ky);
					//
					if ( (ky == UINT32_MAX) && c ) {
						auto use_next_c = c;
						uint8_t offset_follow = get_b_offset_update(use_next_c);  // chops off the next position
						hh_element *f_next += offset_follow;
						f_next = el_check_end_wrap(f_next,buffer,end);
						//
						auto value = f_next->_kv.value;
						auto taken = f_next->taken_spots;
						//
						atomic<uint32_t> *a_f_ky = (atomic<uint32_t> *)(&f_next->_kv.key);
						uint32_t f_real_ky = obtain_cell_key(f_real_ky);
						//
						f_next->_V = next->_V;
						if ( f_real_ky != UINT32_MAX ) {
							next->_kv.value = value;
							next->taken_spots = taken;
							a_ky->store(f_real_ky);  // next->_kv.key = f_real_ky;
							a_f_ky->store(UINT32_MAX);
						} else {
							if ( blocker_count == 0 ) save_c = c;  // ???
							if ( handle_count ) blocker_count++;
						}
						//
					} else {
						a_ky->store(ky);
					}
				}
				handle_count = false;
			}
		}


		uint32_t swappy_read(uint32_t el_key, hh_element *base, uint32_t c) {
			hh_element *next = base;
			while ( c ) {
				next = base;
				uint8_t offset = get_b_offset_update(c);
				next += offset;
				next = el_check_end_wrap(next,buffer,end);
				// is this being usurped or swapped or deleted at this moment? Search on this range is locked
				//
				atomic<uint32_t> *a_ky = (atomic<uint32_t> *)(&next->_kv.key);
				
				uint32_t real_ky = (UINT32_MAX-1);
				while ( a_ky->compare_exchange_weak(real_ky,(UINT32_MAX-1)) && (real_ky == (UINT32_MAX-1)));
				auto ky = real_ky;
				//
				if ( (ky == UINT32_MAX) && c ) {
					auto use_next_c = c;
					uint8_t offset_follow = get_b_offset_update(use_next_c);  // chops off the next position
					hh_element *f_next += offset_follow;
					f_next = el_check_end_wrap(f_next,buffer,end);
					//
					atomic<uint32_t> *a_f_ky = (atomic<uint32_t> *)(&f_next->_kv.key);
					auto fky = a_f_ky->load(std::memory_order_acquire);
					auto value = f_next->_kv.value;
					auto taken = f_next->taken_spots;
					//
					uint32_t f_real_ky = (UINT32_MAX-1);
					while ( a_f_ky->compare_exchange_weak(f_real_ky,(UINT32_MAX-1)) && (f_real_ky == (UINT32_MAX-1)));
					f_next->_V = next->_V;

					if ( f_real_ky != UINT32_MAX ) {
						next->_kv.value = value;
						next->_kv.key = f_real_ky;
						next->taken_spots = taken;
						a_ky->store(f_real_ky);
						a_f_ky->store(UINT32_MAX);
						if ( f_real_ky == el_key  ) {  // opportunistic reader helps delete with on swap and returns on finding its value
							return value;
						}
					}
					//
					// else resume with the nex position being the hole blocker... 
				} else if ( el_key == ky ) {
					auto value = next->_kv.value;
					a_ky->store(ky);
					return value;
				} else {
					a_ky->store(ky);
				}
			}
			return UINT32_MAX;	
		}



		hh_element *swappy_search_ref(uint32_t el_key, hh_element *base, uint32_t c) {
			hh_element *next = base;
			while ( c ) {
				next = base;
				uint8_t offset = get_b_offset_update(c);
				next += offset;
				next = el_check_end_wrap(next,buffer,end);
				// is this being usurped or swapped or deleted at this moment? Search on this range is locked
				//
				atomic<uint32_t> *a_ky = (atomic<uint32_t> *)(&next->_kv.key);
				
				uint32_t real_ky = (UINT32_MAX-1);
				while ( a_ky->compare_exchange_weak(real_ky,(UINT32_MAX-1)) && (real_ky == (UINT32_MAX-1)));
				auto ky = real_ky;
				//
				if ( (ky == UINT32_MAX) && c ) {
					auto use_next_c = c;
					uint8_t offset_follow = get_b_offset_update(use_next_c);  // chops off the next position
					hh_element *f_next += offset_follow;
					f_next = el_check_end_wrap(f_next,buffer,end);
					//
					atomic<uint32_t> *a_f_ky = (atomic<uint32_t> *)(&f_next->_kv.key);
					auto fky = a_f_ky->load(std::memory_order_acquire);
					auto value = f_next->_kv.value;
					auto taken = f_next->taken_spots;
					//
					uint32_t f_real_ky = (UINT32_MAX-1);
					while ( a_f_ky->compare_exchange_weak(f_real_ky,(UINT32_MAX-1)) && (f_real_ky == (UINT32_MAX-1)));
					f_next->_V = next->_V;

					if ( f_real_ky != UINT32_MAX ) {
						next->_kv.value = value;
						next->_kv.key = f_real_ky;
						next->taken_spots = taken;
						a_ky->store(f_real_ky);
						a_f_ky->store(UINT32_MAX);
						if ( f_real_ky == el_key  ) {  // opportunistic reader helps delete with on swap and returns on finding its value
							return next;
						}
					}
					//
					// else resume with the nex position being the hole blocker... 
				} else if ( el_key == ky ) {
					a_ky->store(ky);
					return next;
				} else {
					a_ky->store(ky);
				}
			}
			return UINT32_MAX;	
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

		// del_ref
		//  // caller will decrement count
		//
		/**
		 * del_ref     
		 * 
		 * params: a_c_bits, base, cbits, el_key, buffer, end
		*/
		uint32_t del_ref(atomic<uint32_t> *a_c_bits, hh_element *base, uint32_t c, uint32_t el_key, hh_element *buffer, hh_element *end) {
			hh_element *next = base;
			//
			// register the key that is being deleted 
			//
			next = el_check_end_wrap(next,buffer,end);
			if ( el_key == next->_kv.key ) {  // check this first... is a special case and most common
				if ( popcount(c) > 1 ) {
					atomic<uint32_t> *ky = (atomic<uint32_t> *)(&base->_kv.key);
					ky->store(UINT32_MAX,std::memory_order_release);   // write the kill switch (under lock but not for reading)
					base->_kv.value = 0;
					// going to shift... as long as readers shift first...
					wait_for_readers(a_c_bits); // a semaphore wait (i.e. readers count down)
					total_swappy_read(base,c);
					submit_for_cropping(base,c);  // after a total swappy read, all BLACK keys will be at the end of members
				} else {					// the bucket will be gone...
					wait_for_readers(a_c_bits); // a semaphore wait (i.e. readers count down)
					clear_bucket(base);
					a_remove_back_taken_spots(a_c_bits, base, 0, buffer, end);
				}
				return 0;
			}
			auto c_pattern = c;
			while ( c ) {			// search for the bucket member that matches the key
				next = base;
				uint8_t offset = get_b_offset_update(c);
				next += offset;
				next = el_check_end_wrap(next,buffer,end);
				// is this being usurped or swapped or deleted at this moment? (structural change has no priority over search)
				if ( el_key == next->_kv.key ) {
					wait_for_readers(controller); // a semaphore wait (i.e. readers count down)
					total_swappy_read(base,c);
					submit_for_cropping(base,c);  // after a total swappy read, all BLACK keys will be at the end of members
					return offset;
				}
			}
			return UINT32_MAX;
		}


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
				vb_probe->taken_spots |= (1 << (hole - offset));   // the bit is not as far out
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
					if ( vb_probe->c_bits & 0x1 ) {
						vb_probe->taken_spots |= ((uint32_t)0x1 << k);  // the bit is not as far out
						c = vb_probe->c_bits;
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & vb_probe->taken_spots;
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					if ( base_probe->c_bits & 0x1 ) {
						auto j = k;
						j -= offset_nxt;
						base_probe->taken_spots |= ((uint32_t)0x1 << j);  // the bit is not as far out
						c = c & ~(base_probe->c_bits);  // no need to look at base probe members anymore ... remaining bits are other buckets
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
					if ( vb_probe->c_bits & 0x1 ) {
						vb_probe->taken_spots &= (~((uint32_t)0x1 << k));  // the bit is not as far out
						c = vb_probe->c_bits;
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & vb_probe->taken_spots;
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					if ( base_probe->c_bits & 0x1 ) {
						auto j = k;
						j -= offset_nxt;
						base_probe->taken_spots &= (~((uint32_t)0x1 << j));  // the bit is not as far out
						c = c & ~(base_probe->c_bits);  // no need to look at base probe members anymore ... remaining bits are other buckets
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
					swap(vb_nxt->_V,vb_probe->_V);					
					// not that the c_bits are note copied... for members, it is the offset to the base, while the base stores the memebership bit pattern
					swap(vb_nxt->taken_spots,vb_probe->taken_spots);  // when the element is not a bucket head, this is time...
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
				if ( vb_probe->c_bits & 0x1 ) {
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					vb_probe->taken_spots &= (~((uint32_t)0x1 << (nxt_loc - offset)));   // the bit is not as far out
				}
			}
		}

		/**
		 * removal_taken_spots
		*/
		void removal_taken_spots(hh_element *hash_base,hh_element *hash_ref,uint32_t c_pattern,hh_element *buffer, hh_element *end) {
			//
			uint32_t a = c_pattern; // membership mask
			uint32_t b = hash_base->taken_spots;   // load this or rely on lock
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
			vb_last->c_bits = 0;
			vb_last->taken_spots = 0;
			vb_last->_V = 0;

			uint8_t nxt_loc = (vb_last - hash_base);   // the spot that actually cleared...

			// recall, a and b are from the base
			// clear the bits for the element being removed
			UNSET(a,nxt_loc);
			UNSET(b,nxt_loc);
			hash_base->c_bits = a;
			hash_base->taken_spots = b;

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
				if ( base_probe->c_bits & 0x1 ) {
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
			atomic<uint32_t> *cbits = (atomic<uint32_t> *)(&(base_probe->c_bits));
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
				if ( vb_probe->taken_spots < time ) {
					// if ( !check_slice_lock(vb_probe,which_table,thread_id) ) continue;  // if the element is being usurped, then if it is min OK another min remains, else the min is futher down the list
					// if ( *min_probe_ref != nullptr ) slice_unlock(*min_probe_ref)
					time = vb_probe->taken_spots;
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
					if ( popcount(base_probe->c_bits) > 1 ) {
						seek_min_member(&min_probe, min_base_offset, base_probe, time, offset, offset_nxt_base, buffer, end);
					}
				}
				// unlock slice counter base_probe
			}
			if ( base_probe != hash_base ) {  // found a place in the bucket that can be moved... (steal this spot)
				//
				min_probe->_V = v_passed;
				min_probe->c_bits = (min_base_offset + offset_nxt_base) << 1;  // offset is stored one bit up
				time = stamp_offset(time,(min_base_offset + offset_nxt_base));  // offset is now
				min_probe->taken_spots = time;  // when the element is not a bucket head, this is time... 
				// slice_unlock(min_probe)
				atomic<uint64_t> *c_aptr = (atomic<uint64_t> *)(min_base->c_bits);
				auto m_c_bits = c_aptr->load();
				auto m_c_bits_prev = m_c_bits;
				m_c_bits &= ~(0x1 << min_base_offset);
				while ( !(c_aptr->compare_exchange_weak(m_c_bits_prev,m_c_bits,std::memory_order_release)) ) { m_c_bits = m_c_bits_prev; m_c_bits &= ~(0x1 << min_base_offset); }
				// min_base->c_bits &= ~(0x1 << min_base_offset);  // turn off the bit (steal bit away)
				//
				atomic<uint64_t> *h_c_aptr = (atomic<uint64_t> *)(hash_base->c_bits);
				auto h_c_bits = h_c_aptr->load();
				auto h_c_bits_prev = h_c_bits;
				h_c_bits |= (0x1 << (min_base_offset + offset_nxt_base));
				while ( !(h_c_aptr->compare_exchange_weak(h_c_bits_prev,h_c_bits,std::memory_order_release)) ) { h_c_bits = h_c_bits_prev; h_c_bits |= (0x1 << (min_base_offset + offset_nxt_base)); }
				// hash_base->c_bits |= (0x1 << (min_base_offset + offset_nxt_base));  // keep it here
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
				atomic<uint64_t> *V_aptr = (atomic<uint64_t> *)(vb_probe->_V);
				v_passed = V_aptr->exchange(v_passed,std::memory_order_acq_rel);
				//swap(v_passed,vb_probe->_V);
				time = stamp_offset(time,offset);
				atomic<uint64_t> *t_aptr = (atomic<uint64_t> *)(vb_probe->taken_spots);
				time = t_aptr->exchange(time,std::memory_order_acq_rel);
				//swap(time,vb_probe->taken_spots);  // when the element is not a bucket head, this is time... 
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
			atomic<uint32_t> *hb_c_bits = (atomic<uint32_t> *)(&hash_base->c_bits);
			uint32_t a = hb_c_bits->load(std::memory_order_acquire); // membership mask
			atomic<uint32_t> *hb_t_bits = (atomic<uint32_t> *)(&hash_base->taken_spots);
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
				vb_probe->c_bits = (hole << 1);  // this is the offset stored one bit up...
				vb_probe->taken_spots = time;
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


		uint32_t c_bits_temporary_store[MAX_THREADS];

		uint32_t fetch_real_cbits(uint32_t cbit_carrier) {
			auto thrd = cbits_thread_id_of(cbit_carrier);
			return c_bits_temporary_store[thrd];
		}

		void store_real_cbits(uint32_t cbits,uint8_t thrd) {
			c_bits_temporary_store[thrd] = cbits;
		}
 

		hh_element *select_bucket_for_slice(uint32_t h_bucket,uint8_t &which_table,HHash **T_ref,hh_element **buffer_ref,hh_element **end_buffer_ref) {
			//
			if ( h_bucket >= _max_n ) {
				h_bucket -= _max_n;  // let it be circular...
			}
			//
			uint8_t count_0;
			uint8_t count_1;
			//
			get_member_bits_counts(h_bucket,count_0,count_1);
			//
			if ( (count_0 >= COUNT_MASK) && (count_1 >= COUNT_MASK) ) {
				return nullptr;
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
			set_thread_table_refs(which_table, T_ref, buffer_ref, end_buffer_ref);
			hh_element *el = *buffer_ref + h_bucket;
			return el;
		}



		// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------

		/**
		 * set_thread_table_refs
		*/
		void set_thread_table_refs(uint8_t which_table, HHash **T_ref, hh_element **buffer_ref, hh_element **end_buffer_ref) {
			HHash *T = _T0;
			hh_element *buffer		= _region_HV_0;
			hh_element *end_buffer	= _region_HV_0_end;
			if ( which_table ) {
				T = _T1;
				buffer = _region_HV_1;
				end_buffer = _region_HV_1_end;
			}
			*T_ref = T;
			*buffer_ref = buffer;
			*end_buffer_ref = end_buffer;
		}



		void get_member_bits_counts(uint32_t h_bucket,uint8_t &count0,uint8_t &count1) {
			//
			hh_element *buffer		= _region_HV_0;
			hh_element *end_buffer	= _region_HV_0_end;
			//
			hh_element *el = buffer + h_bucket;
			if ( el < end_buffer ) {
				atomic<uint32_t> *cbits = (atomic<uint32_t> *)(&(el->c_bits));
				auto c = cbits->load(std::memory_order_acquire);
				if ( c & 0x1 ) {
					count0 = popcount(c);
				}
 			}
			//
			buffer		= _region_HV_1;
			end_buffer	= _region_HV_1_end;
			//
			el = buffer + h_bucket;
			if ( el < end_buffer ) {
				atomic<uint32_t> *cbits = (atomic<uint32_t> *)(&(el->c_bits));
				auto c = cbits->load(std::memory_order_acquire);
				if ( c & 0x1 ) {
					count1 = popcount(c);
				}
 			}
			//
		}


		uint8_t select_insert_buffer(uint8_t count0,uint8_t count1) {
			uint8_t = which_table = 0;
			if ( (count_0 >= COUNT_MASK) && (count_1 >= COUNT_MASK) ) {
				return 0xFF;
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


		atomic<uint32_t> *get_member_bits_slice_info(uint32_t h_bucket,uint8_t &which_table,uint32_t &c_bits,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref) {
			//
			if ( h_bucket >= _max_n ) {
				h_bucket -= _max_n;  // let it be circular...
			}
			uint8_t count0;
			uint8_t count1;
			//
			hh_element *buffer0		= _region_HV_0;
			hh_element *end_buffer0	= _region_HV_0_end;
			//
			hh_element *el_0 = buffer0 + h_bucket;
			atomic<uint32_t> *cbits0 = (atomic<uint32_t> *)(&(el_0->c_bits));
			auto c0 = cbits0->load(std::memory_order_acquire);
			if ( c0 & 0x1 ) {
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
			atomic<uint32_t> *cbits1 = (atomic<uint32_t> *)(&(el_1->c_bits));
			auto c1 = cbits1->load(std::memory_order_acquire);
			if ( c1 & 0x1 ) {
				count1 = popcount(c1);
			} else {
				if ( base_in_operation(c1) ) {
					auto real_bits = fetch_real_cbits(c1);
					count1 = popcount(real_bits);
				}
			}
			//
			auto selector = select_insert_buffer(count0,count1);
			if ( selector == 0xFF ) return nullptr;
			//
			which_table = selector;
			if ( selector == 0 ) {
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


		uint32_t cbits_add_reader(atomic<uint32_t> *a_cbits,uint8_t thread_id,uint32_t original_cbits = 0) {
			//
			auto cbits = (original_cbits == 0) ? a_cbits->load(std::memory_order_acquire) : original_cbits;
			if ( cbits == 0 ) return 0;
			//
			if ( cbits & 0x1 ) {   // This thread now knows
				//
				while ( cbits & 0x1 ) {
					auto save_cbits = cbits;
					uint32_t cbits_reader = 0;
					cbits_reader = (1 << CBIT_READER_SEMAPHORE_SHIFT)
									| cbit_thread_stamp(cbits_reader,thread_id)
									| READER_BIT_SET;
					while ( !(a_cbits->compare_exchange_weak(cbits,a_cbits)) && !(cbits & READER_BIT_SET) );
					if ( !(cbits & 0x1) ) {
						if ( (cbits_thread_id_of(cbits) != thread_id) && (cbits & READER_BIT_SET) ) {
							return cbits_add_reader(a_cbits,thread_id);
						} else {
							stash_reader_cbits(thead_id,save_cbits);
							return save_cbits;
						}
					}
				}
				//
			} else if ( (cbits & READER_BIT_SET )!= 0 ) {
				auto semcount = (cbits & CBIT_READER_SEMAPHORE_WORD) >> CBIT_READER_SEMAPHORE_SHIFT;
				while ( semcount >= CBIT_READ_MAX_SEMAPHORE ) {
					spin_wait();
					semcount = a_cbits->load(std::memory_order_acquire);
				}
				semcount++;
				semcount = (semcount << CBIT_READER_SEMAPHORE_SHIFT);
				auto cbits_update = (semcount | (CBIT_READER_SEMAPHORE_CLEAR & cbits) | READER_BIT_SET);
				while ( !(a_cbits->compare_exchange_weak(cbits,cbits_update,std::memory_order_acq_rel)) ) {
					semcount = (cbits & CBIT_READER_SEMAPHORE_WORD) >> CBIT_READER_SEMAPHORE_SHIFT;
					semcount++;
					semcount = (semcount << CBIT_READER_SEMAPHORE_SHIFT);
					cbits_update = (semcount | (CBIT_READER_SEMAPHORE_CLEAR & cbits) | READER_BIT_SET);
				}
			}
			//
			return cbits;
		}

		uint32_t cbits_remove_reader(atomic<uint32_t> *a_cbits) {
			//
			auto cbits = a_cbits->load(std::memory_order_acquire);
			if ( (cbits & READER_BIT_SET) == 0 ) return UINT32_MAX;			//
			//
			auto semcount = (cbits & CBIT_READER_SEMAPHORE_WORD) >> CBIT_READER_SEMAPHORE_SHIFT;
			if ( semcount > 0 ) {
				semcount--;
			}
			semcount = (semcount << CBIT_READER_SEMAPHORE_SHIFT);
			auto cbits_update = (semcount | (CBIT_READER_SEMAPHORE_CLEAR & cbits)) & READER_BIT_RESET;
			while ( !(a_cbits->compare_exchange_weak(cbits,cbits_update,std::memory_order_acq_rel)) ) {
				semcount = (cbits & CBIT_READER_SEMAPHORE_WORD) >> CBIT_READER_SEMAPHORE_SHIFT;
				if ( semcount > 0 ) {
					semcount--;
					if ( semcount == 0 ) break;
				}
				semcount = (semcount << CBIT_READER_SEMAPHORE_SHIFT);
				cbits_update = (semcount | (CBIT_READER_SEMAPHORE_CLEAR & cbits)) & READER_BIT_RESET;
			}
			if ( semcount == 0 ) {
				if ( !(cbits_update & EDITOR_BIT_SET) ) {
					auto thread_id = cbits_thread_id_of(cbits_update);
					auto cbits_restore = unstash_cbits(thread_id);
					while ( !(a_cbits->compare_exchange_weak(cbits,cbits_restore,std::memory_order_acq_rel)) );
					cbits = cbits_restore;
				}
			}
			//
			return cbits;
		}


		void cbits_wait_for_readers(atomic<uint32_t> *a_cbits,uint8_t thread_id) {
			auto cbits = a_cbits->load(std::memory_order_acquire);
			while ( (cbits & READER_BIT_SET) != 0 ) {
				tick();
				cbits = cbits & (~READER_BIT_SET & CBIT_READER_SEMAPHORE_CLEAR);
				auto ownership_control = cbit_thread_stamp(cbits,thread_id) | EDITOR_BIT_SET;
				//
				while ( a_c_bits->compare_exchange_weak(cbits,ownership_control,std::memory_order_acq_rel) && (cbits != ownership_control) ) {
					tick();  // do a faster timed spin
				}
			}
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----






		//
		/**
		 * prepare_add_key_value_known_slice  ALIAS for wait_if_unlock_bucket_counts
		*/
		bool prepare_add_key_value_known_slice(atomic<uint32_t> **control_bits_ref,uint32_t h_bucket,uint8_t thread_id,uint8_t &which_table,uint32_t &cbits,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref) {
			//
			atomic<uint32_t> *a_c_bits = get_member_bits_slice_info(h_bucket,which_table,cbits,bucket_ref,buffer_ref,end_buffer_ref);
			//
			if ( a_c_bits == nullptr ) return false;
			*control_bits_ref = a_c_bits;

			return true;
		}





		/**
		 * configure_taken_spots
		*/

		void configure_taken_spots(hh_element *hash_ref, uint32_t value, uint32_t el_key) {
			hash_ref->_kv.value = value;
			hash_ref->_kv.key = el_key;
			hash_ref->c_bits = 0x1;  // locked position
			hh_element *base_ref = hash_ref;
			uint32_t c = 1;				// c will be the new base element map of taken positions
			for ( uint8_t i = 1; i < NEIGHBORHOOD; i++ ) {
				hash_ref++;								// keep walking forward to the end of the window
				if ( hash_ref->c_bits & 0x1 ) {			// this is a base bucket with information beyond the window
					atomic<uint32_t> *hn_t_bits = (atomic<uint32_t> *)(&base_ref->taken_spots);
					auto taken = hn_t_bits->load(std::memory_order_acquire);
					c |= (taken << i); // this is the record, but shift it....
					break;								// don't need to get more information at this point
				} else if ( hash_ref->c_bits != 0 ) {
					SET(c,i);							// this element belongs to a base below the new base
				}
			}
			base_ref->taken_spots = c; // finally save the taken spots.
		}



		/**
		 * add_key_value_known_slice
		 * 
		 * This method should be caled after `prepare_add_key_value_known_slice` which locks the bucket found from `h_bucket`.
		*/
		uint64_t add_key_value_known_slice(uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table = 0,uint8_t thread_id = 1) {
			uint8_t selector = 0x3;
			//
			if ( (offset_value != 0) &&  selector_bit_is_set(h_bucket,selector) ) {
				auto N = this->_max_n;
				//
				HHash *T = _T0;
				hh_element *buffer = _region_HV_0;
				hh_element *end	= _region_HV_0_end;
				//
				this->set_thread_table_refs(which_table,&T,&buffer,&end);
				//
				return place_in_bucket(el_key, h_bucket, offset_value, which_table, thread_id, N, buffer, end);
			}
			//
			return UINT64_MAX;
		}


		void enqueue_taken_spots(atomic<uint32_t> *control_bits,uint8_t thread_id,uint32_t cbits,hh_element *bucket,hh_element *buffer,hh_element *end_buffer) {

		}

		void wakeup_memmap_manager() {

		}


		atomic<uint32_t>  *load_stable_key(hh_element *bucket,uint32_t &save_key) {
			atomic<uint32_t>  *stable_key = (atomic<uint32_t>  *)bucket->_kv.key);
			save_key = stable_key->load(std::memory_order_acquire);
			return stable_key;
		}



		/**
		 * bucket_is_base
		*/

		bool is_base_bucket_cbits(uint32_t cbits) {
			if ( cbits & 0x1 ) return true;
			if ( base_in_operation(cbits) ) return true;
			return false;
		}

		bool bucket_is_base(hh_element *hash_ref) {
			atomic<uint32_t>  *a_c_bits = (atomic<uint32_t>  *)hash_ref->c_bits);
			auto cbits = a_c_bits->load(std::memory_order_acquire);
			return is_base_bucket_cbits(cbits);
		}


		bool bucket_is_base(hh_element *hash_ref,uint32_t &cbits) {
			atomic<uint32_t>  *a_c_bits = (atomic<uint32_t>  *)hash_ref->c_bits);
			cbits = a_c_bits->load(std::memory_order_acquire);
			if ( is_base_bucket_cbits(cbits) ) {
				if ( !(cbits & 0x1) ) {
					cbits = fetch_real_cbits(cbits);
				}
				return true;
			}
			return false;
		}




		/**
		 * create_base_taken_spots
		*/

		void create_base_taken_spots(hh_element *hash_ref, uint32_t value, uint32_t el_key, hh_element *buffer, hh_element *end_buffer) {
			hh_element *base_ref = hash_ref;
			uint32_t c = 1;				// c will be the new base element map of taken positions
			for ( uint8_t i = 1; i < NEIGHBORHOOD; i++ ) {
				hash_ref = el_check_end_wrap( ++hash_ref, buffer, end ); // keep walking forward to the end of the window
				if ( bucket_is_base(hash_ref) ) {			// this is a base bucket with information beyond the window
					atomic<uint32_t> *hn_t_bits = (atomic<uint32_t> *)(&hash_ref->taken_spots);
					auto taken = hn_t_bits->load(std::memory_order_acquire);
					c |= (taken << i); // this is the record, but shift it....
					break;								// don't need to get more information at this point
				} else if ( hash_ref->c_bits != 0 ) {
					SET(c,i);							// this element belongs to a base below the new base
				}
			}
			atomic<uint32_t> *b_t_bits = (atomic<uint32_t> *)(&base_ref->taken_spots);
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
			atomic<uint32_t> *tbits = (atomic<uint32_t> *)(&bucket->taken_spots);
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
			atomic<uint32_t> *tbits = (atomic<uint32_t> *)(&bucket->taken_spots);
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



		uint64_t first_level_bucket_ops(atomic<uint32_t> *control_bits,uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table, uint8_t thread_id, uint32_t cbits,hh_element *bucket,hh_element *buffer,hh_element *end_buffer) {
			//
			auto prev_bits = cbits;
			do {
				prev_bits = cbits;
				//
				if ( cbits == 0 ) {   // this happens if the bucket is completetly empty and not a member of any other bucket
					// if this fails to stay zero, then either another a hash collision has immediately taken place or another
					// bucket created a member.
					auto locking_bits = cbit_thread_stamp(cbits,thread_id) | EDITOR_BIT_SET;
					if ( control_bits->compare_exchange_weak(cbits,locking_bits,std::memory_order_acq_rel) ) {
						cbits = control_bits->load(std::memory_order_acquire);
						if ( cbits != locking_bits ) continue;
						else {
							// OWN THIS BUCKET
							store_real_cbits(0x1,thread_id);
							//
							bucket->_kv.key = el_key;
							bucket->_kv.value = offset_value;
							//
							uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
							wakeup_value_restore(control_bits, cbits, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, thread_id, buffer, end);
						}
					}
				} else if ( is_base_bucket_cbits(cbits) ) {  // NEW MEMBER: This is a base bucket (the new element should become a member)
					auto locking_bits = cbit_thread_stamp(cbits,thread_id) | EDITOR_BIT_SET;
					if ( cbits & 0x1 ) { // can capture this bucket to do an insert...
						if ( control_bits->compare_exchange_weak(cbits,locking_bits,std::memory_order_acq_rel) ) {
							cbits = control_bits->load(std::memory_order_acquire);
							if ( cbits != locking_bits ) continue;
							else {
								// OWN THIS BUCKET
								uint32_t tmp_value = bucket->_kv.value;
								uint32_t tmp_key = bucket->_kv.key;
								//
								bucket->_kv.key = el_key;
								bucket->_kv.value = offset_value;
								wakeup_value_restore(control_bits, cbits, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, thread_id, buffer, end);
							}
						}
					} else {  // THIS BUCKET IS ALREADY BEING EDITED
						auto current_thread = cbits_thread_id_of(cbits);
						auto current_membership = fetch_real_cbits(cbits);
						wakeup_value_restore(control_bits, current_membership, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, current_thread, buffer, end);
					}
				} else {  // this bucket is a member and cbits carries a back_ref. (this should be usurped)
					auto locking_bits = cbit_thread_stamp(cbits,thread_id) | EDITOR_BIT_SET;
					if ( control_bits->compare_exchange_weak(cbits,locking_bits,std::memory_order_acq_rel) ) {
						cbits = control_bits->load(std::memory_order_acquire);
						if ( cbits != locking_bits ) continue;
						else {
							//
							uint8_t backref = 0;
							hh_element *cells_base = cbits_base_from_backref(prev_bits,backref,bucket,buffer,end_buffer);
							// load key
							// is the value to be reinserted changing? 
							// otherwise save it...
							uint32_t save_key = 0;
							atomic<uint32_t>  *stable_key = load_stable_key(bucket,save_key);
							uint32_t save_value = bucket->_kv.value;

							// OWN THIS BUCKET
							store_real_cbits(0x1,thread_id);
							bucket->_kv.value = offset_value;

							stable_key->store(el_key);
							control_bits->store(fetch_real_cbits(prev_bits));							

							uint64_t current_loaded_value = (((uint64_t)save_key) << HALF) | save_value;
							control_bits = (atomic<uint32_t> *)(&cells_base->c_bits);
							wakeup_value_restore(control_bits, cells_base, (h_bucket-backref), current_loaded_value, which_table, thread_id, buffer, end);						
						}
					}
				}
			} while ( prev_bits != cbits );
		}


/*


			if ( cbits & 0x1 ) {  // it is a based and is not being touched at the moment
				//
				uint32_t ownership_control = EDITOR_BIT_SET | READER_BIT_SET;
				ownership_control = cbit_thread_stamp(ownership_control,thread_id);
				//
				auto prev_cbits = cbits;  // the actual memory pattern of membership
				while ( !(a_c_bits->compare_exchange_weak(prev_cbits,ownership_control,std::memory_order_acq_rel)) ) {
					if ( prev_cbits & 0x1 ) cbits = prev_cbits;
					else {
						if ( (prev_cbits & READER_BIT_SET) && !(prev_cbits & EDITOR_BIT_SET) ) {  // preserve reader semaphore
							ownership_control = cbit_thread_stamp(prev_cbits,thread_id) | EDITOR_BIT_SET;
						} else {
							prev_cbits = cbits;
							tick(); // faster timed spin lock
						}
					}
				}
				uint32_t new_cbits = cbits_add_writer(a_c_bits,thread_id);
				cbits_wait_for_readers(new_cbits,thread_id); // count down reader semaphore...
				//
				// 
				//
				cbits_remove_writer(a_c_bits);
				//
			} else {
				if ( cbits == 0 ) {   // adding a new element here. First one gets the spot (new one should not be different)
					// two possibilities in play
					// 1) lower bucket will find this spot,
					// 2) this spot is being used for the first time
					uint32_t new_cbits = cbits_add_writer(a_c_bits,thread_id);
					cbits_wait_for_readers(new_cbits,thread_id); // some small probability that readers will exist at this point
					if ( is_back_ref(new_cbits) || another_base_slipped_in(new_cbits) ) {   // an element got added while trying to get the bucket
						// try again... the element may end up in the other slice or as a member
						cbits_remove_writer(a_c_bits);
						add_key_value_known_refs(full_hash,h_bucket,offset,which_table,thread_id,cbits,bucket,buffer,end_buffer);
						return;
					} else { // this writer owns the bucket and will create a new base						
						uint32_t update_cbits = 0x1;
						update_taken_spots(el);
						a_c_bits->store(update_cbits);
						while ( !a_c_bits->compare_exchange_weak(update_cbits,update_cbits,std::memory_order_release) );
					}
					cbits_remove_writer(a_c_bits);
				} else if ( is_back_ref(cbits) ) {
					auto info_bits = (cbits >> 1);
					// this is a backref (it may be swappy at the moment)
					usurp_membership_position_hold_key(el,cbits,el_key,offset_value,which_table,buffer,end_buffer);
					//
				} else if ( base_in_operation(cbits) ) {
					if ( no_duplicate_op(a_c_bits,cbits,thread_id) ) {
						cbits_wait_for_writers(new_cbits,thread_id); 
						cbits_wait_for_readers(new_cbits,thread_id); // some small probability that readers will exist at this point
						
					}
					cbits_remove_writer(a_c_bits);
				}
			}
			//
*/

		uint64_t add_key_value_known_refs(atomic<uint32_t> *control_bits,uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table = 0,uint8_t thread_id,uint32_t cbits,hh_element *bucket,hh_element *buffer,hh_element *end_buffer) {
			//
			uint8_t selector = 0x3;
			//
			if ( (offset_value != 0) &&  selector_bit_is_set(h_bucket,selector) ) {
				//
				return first_level_bucket_ops(atomic<uint32_t> *control_bits,full_hash,h_bucket,offset,which_table,thread_id,cbits,bucket,buffer,end_buffer);
				//
			}
			return UINT64_MAX;
		}



		/**
		 * place_back_taken_spots
		*/

		atomic<uint32_t> *lock_taken_spots(hh_element *vb_probe,uint32_t &taken) {
			atomic<uint32_t> *a_taken = (atomic<uint32_t> *)(&(vb_probe->taken_spots));
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
				c = c & vb_probe->taken_spots;
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
					if ( vb_probe->c_bits & 0x1 ) {
						vb_probe->taken_spots &= (~((uint32_t)0x1 << k));  // the bit is not as far out
						c = vb_probe->c_bits;
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & vb_probe->taken_spots;
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					if ( base_probe->c_bits & 0x1 ) {
						auto j = k;
						j -= offset_nxt;
						base_probe->taken_spots &= (~((uint32_t)0x1 << j));  // the bit is not as far out
						c = c & ~(base_probe->c_bits);  // no need to look at base probe members anymore ... remaining bits are other buckets
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
					swap(vb_nxt->_V,vb_probe->_V);					
					// not that the c_bits are note copied... for members, it is the offset to the base, while the base stores the memebership bit pattern
					swap(vb_nxt->taken_spots,vb_probe->taken_spots);  // when the element is not a bucket head, this is time...
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
				if ( vb_probe->c_bits & 0x1 ) {
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					vb_probe->taken_spots &= (~((uint32_t)0x1 << (nxt_loc - offset)));   // the bit is not as far out
				}
			}
		}

		/**
		 * removal_taken_spots
		*/
		void a_removal_taken_spots(hh_element *hash_base,hh_element *hash_ref,uint32_t c_pattern,hh_element *buffer, hh_element *end) {
			//
			uint32_t a = c_pattern; // membership mask
			uint32_t b = hash_base->taken_spots;   // load this or rely on lock
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
			vb_last->c_bits = 0;
			vb_last->taken_spots = 0;
			vb_last->_V = 0;

			uint8_t nxt_loc = (vb_last - hash_base);   // the spot that actually cleared...

			// recall, a and b are from the base
			// clear the bits for the element being removed
			UNSET(a,nxt_loc);
			UNSET(b,nxt_loc);
			hash_base->c_bits = a;
			hash_base->taken_spots = b;

			// OTHER buckets
			// now look at the bits within the range of the current bucket indicating holdings of other buckets.
			// these buckets are ahead of the base, but behind the cleared position...
			remove_bucket_taken_spots(hash_base,nxt_loc,a,b,buffer,end);
			// look for bits preceding the current bucket
			remove_back_taken_spots(hash_base, nxt_loc, buffer, end);
		}






		/**
		 * place_in_bucket
		 * 
		 * Either puts a new element into a hash bucket or makes use of a previously established bucket in one of two ways.
		 * One way is for the element to be placed in the bucket as a member, in which case newest element usurps the root 
		 * of the bae and then arranges for the old root to be put back into the bucket. The second way is for the new element
		 * to usurp the position of a member of some other bucket determined by the back reference from the member to the base 
		 * bucket. The usurping thread arranges for the usurped element to be placed back into the bucket of its base.
		*/

		uint64_t place_in_bucket(uint32_t el_key, uint32_t h_bucket, uint32_t offset_value, uint8_t which_table, uint8_t thread_id, uint32_t N,hh_element *buffer,hh_element *end) {

			uint32_t h_start = h_bucket % N;  // scale the hash .. make sure it indexes the array...  (should not have to do this)
			// hash_base -- for the base of the bucket
			hh_element *hash_base = bucket_at(buffer,h_start,end);  //  hash_base aleady locked (by counter)
			//
			uint32_t tmp_c_bits = hash_base->c_bits;
			uint32_t tmp_value = hash_base->_kv.value;
			//
			if ( !(tmp_c_bits & 0x1) && (tmp_value == 0) ) {  // empty bucket  (this is a new base)
				// put the value into the current bucket and forward
				create_in_bucket_at_base(hash_base,offset_value,el_key);   // this bucket is locked 
				// also set the bit in prior buckets....
				place_back_taken_spots(hash_base, 0, buffer, end);
				this->slice_bucket_count_incr_unlock(h_start,which_table,thread_id);
			} else {
				//	save the old value
				atomic<uint32_t> *h_ky = (atomic<uint32_t> *)(&hash_base->_kv.key);
				auto ky = obtain_cell_key(h_ky);
				//
				uint64_t current_loaded_value = (((uint64_t)ky) << HALF) | tmp_value;
				// new values
				//
				if ( tmp_c_bits & 0x1 ) {  // (this is actually a base) don't touch c_bits (restore will)
					// this base is for the new element's bucket where it belongs as a member.
					// usurp the position of the current bucket head,
					// then put the bucket head back into the bucket (allowing another thread to do the work)
					hash_base->_kv.value = offset_value;  // put in the new values
					h_ky->store(el_key,std::memory_order_release);  // hash_base->_kv.key = el_key;
					//
					wakeup_value_restore(hash_base, h_start, current_loaded_value, which_table, thread_id, buffer, end);
					// Note: this is controller's bucket and this head is locked 
				} else {   // `hash_base` is NOT A BASE 
					// -- some other bucket controls this positions for the moment.
					uint32_t *controllers = _region_C;
					auto controller = (atomic<uint32_t>*)(&controllers[h_start]);
					//
					// usurp the position from a bucket that has this position as a member..
					// 
					uint8_t k = usurp_membership_position_hold_key(hash_base,tmp_c_bits,el_key,offset_value,which_table,buffer,end);
					// k is the offset to the base in which `hash_base` WAS a member..
					h_ky->store(el_key,std::memory_order_release);   // release the key... (now the new element)
					//
					this->slice_unlock_counter(controller,which_table,thread_id);
					// hash the saved value back into its bucket if possible.
					h_start = (h_start > k) ? (h_start - k) : (N - k + h_start);     // bucket of backref 
					// PUT IT BACK ... try to put the element back...
					HHash *T = _T0;
					while ( !wait_if_unlock_bucket_counts_refs(&controller,thread_id,h_start,&T,&buffer,&end,which_table) );
					hash_base = bucket_at(buffer,h_start,end);
					// loaded_value was in the bucket before the usurp
					wakeup_value_restore(hash_base, h_start, current_loaded_value, which_table, thread_id, buffer, end);
				}
			}
			//
			h_bucket = stamp_key(h_bucket,which_table);
			uint64_t loaded_key = (((uint64_t)el_key) << HALF) | h_bucket; // LOADED
			return loaded_key;
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
		uint32_t		 				*_region_C;
		uint32_t		 				*_end_region_C;
		
		// threads ...
		proc_descr						*_process_table;						
		proc_descr						*_end_procs;
		uint8_t							_round_robbin_proc_table_threads{0};

		atomic_flag		 				*_rand_gen_thread_waiting_spinner;
		atomic_flag		 				*_random_share_lock;

		atomic<uint32_t>				*_random_gen_region;

		atomic_flag						_sleeping_reclaimer;

};


#endif // _H_HOPSCOTCH_HASH_SHM_