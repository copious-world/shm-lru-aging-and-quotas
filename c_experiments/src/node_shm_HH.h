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

 
#include "hmap_interface.h"
#include "random_selector.h"


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


typedef struct Q_ENTRY {
	public:
		//
		hh_element 	*hash_ref;
		uint32_t 	h_bucket;
		uint64_t 	loaded_value;
		hh_element	*buffer;
		hh_element	*end;
		uint8_t		which_table;
		uint8_t		thread_id;
		//
		uint8_t __rest[2];
} q_entry;



typedef struct PRODUCT_DESCR {
	uint32_t		partner_thread;
	uint32_t		stats;
} proc_descr;



template<uint16_t const ExpectedMax = 100>
class QueueEntryHolder {
	//
	public:
		//
		QueueEntryHolder() {
			_current = 0;
			_last = 0;
		}
		virtual ~QueueEntryHolder() {
		}

	public:

		void 		pop(q_entry &entry) {
			if ( _current == _last ) {
				memset(&entry,0,sizeof(q_entry));
				return;
			}
			entry = _entries[_current++];
			if ( _current == ExpectedMax ) {
				_current = 0;
			}
		}

		void		push(q_entry &entry) {
			_entries[_last++] = entry;
			if ( _last == ExpectedMax ) _last = 0;
		}

		bool empty() {
			return ( _current == _last);
		}

	public:

		q_entry _entries[ExpectedMax];
		uint16_t _current;
		uint16_t _last;

};





inline uint8_t get_b_offset_update(uint32_t &c) {
	uint8_t offset = countr_zero(c);
	c = c & (~((uint32_t)0x1 << offset));
	return offset;
}



//
inline uint32_t stamp_offset(uint32_t time,[[maybe_unused]]uint8_t offset) {
	return time;
}


// ---- ---- ---- ---- ---- ----  HHash
// HHash <- HHASH

template<const uint32_t NEIGHBORHOOD = 32>
class HH_map : public HMap_interface, public Random_bits_generator<> {
	//
	public:

		// LRU_cache -- constructor
		HH_map(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) {
			_reason = "OK";
			_restore_operational = true;
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

			_random_gen_region->store(0);
		}


		virtual ~HH_map() {
			_restore_operational = false;

			// join threads here...
		}


		// REGIONS...

		// setup_region -- part of initialization if the process is the intiator..
		// -- header_size --> HHash
		// the regions are setup as, [values 1][buckets 1][values 2][buckets 2][controls 1 and 2]
		void setup_region(bool am_initializer,uint8_t header_size,uint32_t max_count,uint32_t num_threads) {
			// ----
			uint8_t *start = _region;

			_random_gen_thread_lock = (atomic_flag *)start;
			_random_share_lock = (atomic_flag *)(_random_gen_thread_lock + 1);
			_random_gen_region = (atomic<uint32_t> *)(_random_share_lock + 1);

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
					memset((void *)(_end_region_C), 0, proc_regions_size);
				} else {
					throw "hh_map (4) sizes overrun allocated region determined by region_sz";
				}
			}
			//
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


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



		// clear
		void clear(void) {
			if ( _initializer ) {
				uint8_t sz = sizeof(HHash);
				uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
				this->setup_region(_initializer,header_size,_max_count,_num_threads);
			}
		}



		bool check_end(uint8_t *ref,bool expect_end = false) {
			if ( ref == _endof_region ) {
				if ( expect_end ) return true;
			}
			if ( (ref < _endof_region)  && (ref > _region) ) return true;
			return false;
		}


		inline hh_element *el_check_end(hh_element *ptr, hh_element *buffer, hh_element *end) {
			if ( ptr >= end ) return buffer;
			return ptr;
		}


		inline hh_element *el_check_beg_wrap(hh_element *ptr, hh_element *buffer, hh_element *end) {
			if ( ptr < buffer ) return (end - buffer + ptr);
			return ptr;
		}


		inline hh_element *el_check_end_wrap(hh_element *ptr, hh_element *buffer, hh_element *end) {
			if ( ptr >= end ) {
				uint32_t diff = (ptr - end);
				return buffer + diff;
			}
			return ptr;
		}


		// 4*(this->_bits.size() + 4*sizeof(uint32_t))
		void set_random_bits(void *shared_bit_region) {
			uint32_t *bits_for_test = (uint32_t *)(shared_bit_region);
			for ( int i = 0; i < 4; i++ ) {
				this->set_region(bits_for_test,i);
				this->regenerate_shared(i);
				bits_for_test += this->_bits.size() + 4*sizeof(uint32_t);
			}
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
		
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// THREAD CONTROL

		void tick() {
			nanosleep(&request, &remaining);
		}

		void wakeup_random_genator(uint8_t which_region) {   //
			//
			_random_gen_region->store(which_region);
#ifndef __APPLE__
			while ( !(_random_gen_thread_lock->test_and_set()) );
			_random_gen_thread_lock->notify_one();
#else
			while ( !(_random_gen_thread_lock->test_and_set()) );
#endif
		}

		void thread_sleep([[maybe_unused]] uint8_t ticks) {

		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void share_lock(void) {
#ifndef __APPLE__
				while ( _random_gen_thread_lock->test() ) {  // if not cleared, then wait
					_random_gen_thread_lock->wait();
				};
				while ( !_random_gen_thread_lock->test_and_set() );
#else
				while ( _random_gen_thread_lock->test() ) {
					thread_sleep(10);
				};
				while ( !_random_gen_thread_lock->test_and_set() );
#endif			
		}

		
		void share_unlock(void) {
#ifndef __APPLE__
			while ( _random_share_lock->test() ) {
				_random_share_lock->clear();
			};
			_random_share_lock->notify_one();
#else
			while ( _random_share_lock->test() ) {
				_random_share_lock->clear();
			};
#endif
		}


		void random_generator_thread_runner() {
			while ( true ) {
#ifndef __APPLE__
				do {
					_random_gen_thread_lock->wait();
				} while ( !_random_gen_thread_lock->test() )
#else
				do {
					thread_sleep(10);
				} while ( !_random_gen_thread_lock->test() );
#endif
				bool which_region = _random_gen_region->load(std::memory_order_acquire);
				this->regenerate_shared(which_region);		
				_random_gen_thread_lock->clear();		
			}
		}



	
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		// 
		uint64_t stamp_key(uint64_t loaded_key,uint8_t info) {
			uint64_t info64 = info & 0x00FF;
			info64 <<= 24;
			return loaded_key | info64;
		}


		// clear_selector_bit
		uint32_t clear_selector_bit(uint32_t h) {
			h = h & HH_SELECT_BIT_MASK;
			return h;
		}


		// clear_selector_bit64
		uint64_t clear_selector_bit64(uint64_t h) {
			h = (h & HH_SELECT_BIT_MASK64);
			return h;
		}




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
		 * store_unlock_controller
		*/

		void slice_unlock_counter(uint32_t h_bucket,uint8_t which_table,uint8_t thread_id = 1) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			slice_unlock_counter(controller,which_table,thread_id);
		}


		// slice_unlock_counter
		//
		void slice_unlock_counter_controls(atomic<uint32_t> *controller,uint32_t controls,uint8_t which_table,uint8_t thread_id = 1) {
			if ( controller == nullptr ) return;
			controls = which_table ? (controls & FREE_BIT_ODD_SLICE_MASK) : (controls & FREE_BIT_EVEN_SLICE_MASK);
			store_and_unlock_controls(controller,controls,thread_id);
		}


		void slice_unlock_counter_controls(uint32_t h_bucket, uint32_t controls, uint8_t which_table,uint8_t thread_id = 1) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			slice_unlock_counter_controls(controller,controls,which_table,thread_id);
		}


		// slice_unlock_counter
		//
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
		 * bucket_count_decr
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
		 * bitp_stamp_thread_id
		*/

		uint32_t bitp_stamp_thread_id(uint32_t controls,uint32_t thread_id) {
			controls = controls & THREAD_ID_SECTION_CLEAR_MASK; // CLEAR THE SECTION
			controls = controls | ((thread_id & THREAD_ID_BASE) << THREAD_ID_SHIFT);
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
			if ( (controls & HOLD_BIT_SET) || (controls & SHARED_BIT_SET) ) return true;
			if ( (controls & THREAD_ID_SECTION) != 0 ) {  // no locks but there is an occupant (for a slice level)
				if ( thread_id_of(controls) == thread_id ) return true;  // the calling thread is that occupant
			}
			return false; // no one is there or it is another thread than the calling thread and it is at the slice level.
		}


		// bitp_partner_thread
		//			make the thread pattern that this will have if it were to be the last to a partnership.
		uint32_t bitp_partner_thread(uint32_t thread_id,uint32_t controls) {  
			auto p_controls = controls | HOLD_BIT_SET | SHARED_BIT_SET;
			p_controls = bitp_stamp_thread_id(controls,thread_id);
			return p_controls;
		}

		uint32_t bitp_clear_thread_stamp_unlock(uint32_t controls) {
			controls = (controls & (THREAD_ID_SECTION_CLEAR_MASK & FREE_BIT_MASK)); // CLEAR THE SECTION
			return controls;
		}

		void table_store_partnership(uint32_t ctrl_thread,uint32_t thread_id) {
			_process_table[ctrl_thread].partner_thread = thread_id;
			_process_table[thread_id].partner_thread = ctrl_thread;
		}

		bool partnered(uint32_t controls, uint32_t ctrl_thread, uint32_t thread_id) {
			if ( controls & SHARED_BIT_SET ) {
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

		/**
		 * store_relinquish_partnership
		 * 
		 * the method, partnered, should have already been called 
		*/
		//
		void store_relinquish_partnership(atomic<uint32_t> *controller, uint32_t thread_id) {
			auto controls = controller->load(std::memory_order_acquire);
			store_and_unlock_controls(controller,controls,thread_id);
		}




		/**
		 * to_slice_transition
		 * 
		 *			this hold bit must be set prior to calling this method.
		*/

		void to_slice_transition(atomic<uint32_t> *controller, uint32_t controls, uint8_t which_table, uint32_t thread_id) {
			auto pre_store = controller->load(std::memory_order_acquire);
			if ( !(controls & SHARED_BIT_SET) ) {
				controls = controls | SHARED_BIT_SET;
			}
			if ( thread_id_of(controls) != thread_id ) {
				table_store_partnership(thread_id,thread_id_of(controls)); // thread_salvage has to have been set.
			}
			controls = which_table ? (controls | HOLD_BIT_ODD_SLICE) : (controls | HOLD_BIT_EVEN_SLICE);
			while ( !(controller->compare_exchange_weak(pre_store,controls,std::memory_order_acq_rel,std::memory_order_relaxed)) );  //  && (count_loops_2++ < 1000)  && !(controls & HOLD_BIT_SET)
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// store_and_unlock_controls ----
		uint32_t store_and_unlock_controls(atomic<uint32_t> *controller,uint32_t controls,uint8_t thread_id) {
			//
			if ( (controls & HOLD_BIT_SET) || (controls & SHARED_BIT_SET) ) {
				//
				if ( !(controls & SHARED_BIT_SET) ) {
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
					partner_controls = partner_controls & FREE_BIT_MASK;   // leave the shared bit alone... 
					if ( partner == 0 ) {
						partner_controls = partner_controls & QUIT_SHARE_BIT_MASK;  // about the same state as in the above condition.
					}
					_process_table[thread_id].partner_thread = 0;		// but break the partnership
					_process_table[partner].partner_thread = 0;
					while ( !(controller->compare_exchange_weak(controls,partner_controls,std::memory_order_acq_rel,std::memory_order_relaxed)) ) {
						// Some thread (not the caller) has validly acquired the bucket.
						if (( thread_id_of(controls) != thread_id) && ((controls & HOLD_BIT_SET) || (controls & SHARED_BIT_SET)) ) {
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
			uint32_t controls = controller->load(std::memory_order_acquire);
			//
			uint32_t lock_on_controls = 0;  // used after the outter loop
			uint8_t thread_salvage = 0;
			do {
				while ( controls & HOLD_BIT_SET ) {  // while some other process is using this count bucket
					//
					controls = controller->load(std::memory_order_acquire);
					//
					auto ctrl_thread = thread_id_of(controls);
					//
					if ( controls & HOLD_BIT_SET ) {
						// can fix this situation if thread_id holds the lock....
						// given the proper order of bucket locking, etc. At this point, this is a fix for failed locking...
						if ( ctrl_thread == thread_id ) {
							// Clear the hold bit in any way possible (because thread_id is blocking itself)
							// store_unlock_controller will either (1) clear the control word of any thread
							// or (2) remove thread_id from a partnership. 
							controls = store_unlock_controller(controller,thread_id);
						} else {  // another thread is in charge. But, thread_id may still be blocking
							// thread_id is partnered if the shared bit is set and crtr_thread indicates thread_id
							// in the process table.
							if ( partnered(controls,ctrl_thread,thread_id) ) {
								// the thread found leads to information indicating thread_id as being 
								// present in the bucket. So, quit the partnership thereby clearing the hold bit.
								store_relinquish_partnership(controller,thread_id);
							}

						}
					}
				}
				//  At this point the bucket is cleared of thread id. And the hold bit is not set.
				// 	Make sure the expected pattern of storage makes sense. And, write the sensible pattern 
				//	indicating pure lock or shared lock.
				// 
				if ( controls & SHARED_BIT_SET ) {   // Indicates that another thread (stored in controls) can share.
					// This means there is a free slice and thread_id could access that slice.
					// So, produce the bit pattern that will indicate sharing from this point, 
					// i.e. thread_id will be in the control word while the current one will be in salvage (in tables later)
					thread_salvage = thread_id_of(controls);  // in the mean time, we expect the partner to be stored.
					// pass the `controls` word (it has info that must be preserved) ... don't change the process tables yet
					lock_on_controls = bitp_partner_thread(thread_id,controls) | HOLD_BIT_SET; // 
				} else {
					lock_on_controls = bitp_stamp_thread_id(controls,thread_id) | HOLD_BIT_SET;  // make sure that this is the thread that sets the bit high
					controls = bitp_clear_thread_stamp_unlock(controls);   // desire that the controller is in this state (0s)
				}
				// (cew) returns true if the value changes ... meaning the lock is acquired by someone since the last load
				// i.e. loop exits if the value changes from no thread id and clear bit to at least the bit
				// or it changes from the partner thread and shared bit set to thread_id and hold bit set and shared bit still set.
				while ( !(controller->compare_exchange_weak(controls,lock_on_controls,std::memory_order_release,std::memory_order_relaxed)) );  //  && (count_loops_2++ < 1000)  && !(controls & HOLD_BIT_SET)
				// but we care if this thread is the one that gets the bit.. so check to see that `controls` contains 
				// this thread id and the `HOLD_BIT_SET`
			} while ( !(check_thread_id(controls,lock_on_controls)) );  // failing this, we wait on the other thread to give up the lock..
			//
			if ( controls & SHARED_BIT_SET ) {  // at this point thread_id is stored in controls, and if shared bit, then partnership
				table_store_partnership(thread_id,thread_salvage); // thread_salvage has to have been set.
			}
			// at this point this thread should own the bucket...
			ret_control = lock_on_controls;  // return the control word that was loaded and change to carrying this thread id and lock bit
			return controller;
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

		// get an idea, don't try locking or anything  (combined counts)
		uint32_t get_buckt_count(uint32_t h_bucket) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			return get_buckt_count(controller);
		}

		uint32_t get_buckt_count(atomic<uint32_t> *controller) {
			uint32_t controls = controller->load(std::memory_order_relaxed);
			uint8_t counter = (uint8_t)((controls & DOUBLE_COUNT_MASK) >> QUARTER);  
			return counter;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

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

		uint32_t update_count_incr(uint32_t controls) {
			auto counter = ((controls & DOUBLE_COUNT_MASK) >> QUARTER);   // update the counter for both buffers.
			if ( counter < (uint8_t)DOUBLE_COUNT_MASK_BASE ) {
				counter++;
				uint32_t update = (counter << QUARTER) & DOUBLE_COUNT_MASK;
				controls = (controls & ~DOUBLE_COUNT_MASK) | update;
			}
			return controls;
		}

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
		 * wait_if_unlock_bucket_counts
		*/
		bool wait_if_unlock_bucket_counts(uint32_t h_bucket,uint8_t thread_id,HHash **T_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,uint8_t &which_table) {
			atomic<uint32_t> *controller = nullptr;
			return wait_if_unlock_bucket_counts(&controller,thread_id,h_bucket,T_ref,buffer_ref,end_buffer_ref,which_table);
		}


		/**
		 * wait_if_unlock_bucket_counts
		*/
		bool wait_if_unlock_bucket_counts(atomic<uint32_t> **controller_ref,uint8_t thread_id,uint32_t h_bucket,HHash **T_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,uint8_t &which_table) {
			// ----
			HHash *T = _T0;
			hh_element *buffer		= _region_HV_0;
			hh_element *end_buffer	= _region_HV_0_end;
			//
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
			if ( controls & SHARED_BIT_SET ) {
				if ( (controls & HOLD_BIT_ODD_SLICE) && (count_1 < COUNT_MASK) ) {
					which_table = 1;
					T = _T1;
					buffer = _region_HV_1;
					end_buffer = _region_HV_1_end;			// otherwise, the 0 region remains in use
					count_1++;
				} else if ( (controls & HOLD_BIT_EVEN_SLICE)  && (count_0 < COUNT_MASK) ) {
					which_table = 0;
					count_0++;
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
					T = _T1;
					which_table = 1;
					buffer = _region_HV_1;
					end_buffer = _region_HV_1_end;
					count_1++;
				} else if ( (count_1 == count_0) && (count_1 < COUNT_MASK) ) {
					uint8_t bit = pop_shared_bit();
					if ( bit ) {
						T = _T1;
						which_table = 1;
						buffer = _region_HV_1;
						end_buffer = _region_HV_1_end;
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

			*T_ref = T;
			*buffer_ref = buffer;
			*end_buffer_ref = end_buffer;
			return true;
			//
		}


		bool short_list_old_entry([[maybe_unused]] uint64_t loaded_value,[[maybe_unused]] uint32_t store_time) {
			return false;
		}

		// ----

		void  wait_notification_restore() {
#ifndef __APPLE__
			do {
				_sleeping_reclaimer.wait(false);
			} while ( _sleeping_reclaimer.test(std::memory_order_acquire) );
#endif
		}


		void wake_up_one_restore([[maybe_unused]] uint32_t h_start) {
#ifndef __APPLE__
			do {
				_sleeping_reclaimer.test_and_set()
			} while ( !(_sleeping_reclaimer.test(std::memory_order_acquire)) );
			_sleeping_reclaimer.notify_one();
#endif
		}


		bool is_restore_queue_empty() {
			bool is_empty = _process_queue.empty();
#ifndef __APPLE__

#endif
			return is_empty;
		}


		/**
		 * value_restore_runner   --- a thread method...
		*/
		void value_restore_runner(void) {
			hh_element *hash_ref = nullptr;
			uint32_t h_bucket = 0;
			uint64_t loaded_value = 0;
			uint8_t which_table = 0;
			uint8_t thread_id = 0;
			hh_element *buffer = nullptr;
			hh_element *end = nullptr;
			while ( _restore_operational ) {
				while ( is_restore_queue_empty() && _restore_operational ) wait_notification_restore();
				if ( _restore_operational ) {
					dequeue_restore(&hash_ref, h_bucket, loaded_value, which_table, thread_id, &buffer, &end);
					// store... if here, it should be locked
					uint32_t *controllers = _region_C;
					auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
					//
					uint32_t store_time = 1000; // now
					bool quick_put_ok = pop_until_oldest(hash_ref, loaded_value, store_time, buffer, end, h_bucket);
					if ( quick_put_ok ) {
						this->slice_unlock_counter(controller,which_table,thread_id);
					} else {
						//
						this->slice_unlock_counter(controller,which_table,thread_id);
						//
						if ( short_list_old_entry(loaded_value, store_time) ) {
							//
							uint32_t el_key = (loaded_value & UINT32_MAX);
							uint32_t offset_value = ((loaded_value >> HALF) & UINT32_MAX);
							add_key_value(el_key,h_bucket, offset_value, thread_id);
						}
						//
					}
				}
			}
		}


		/**
		 * enqueue_restore
		*/
		void enqueue_restore(hh_element *hash_ref, uint32_t h_bucket, uint64_t loaded_value, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end) {
			q_entry get_entry;
			//
			get_entry.hash_ref = hash_ref;
			get_entry.h_bucket = h_bucket;
			get_entry.loaded_value = loaded_value;
			get_entry.which_table = which_table;
			get_entry.buffer = buffer;
			get_entry.end = end;
			get_entry.thread_id = thread_id;
			//
			_process_queue.push(get_entry);    // by ref
		}

		/**
		 * dequeue_restore
		*/
		void dequeue_restore(hh_element **hash_ref_ref, uint32_t &h_bucket, uint64_t &loaded_value, uint8_t &which_table, uint8_t &thread_id, hh_element **buffer_ref, hh_element **end_ref) {
			//
			q_entry get_entry;
			//
			_process_queue.pop(get_entry);    // by ref
			//
			hh_element *hash_ref = get_entry.hash_ref;
			h_bucket = get_entry.h_bucket;
			loaded_value = get_entry.loaded_value;
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

		void wakeup_value_restore(hh_element *hash_ref, uint32_t h_start, uint64_t loaded_value, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end) {
			// this queue is jus between the calling thread and the service thread belonging to just this process..
			// When the thread works it may content with other processes for the hash buckets on occassion.
			enqueue_restore(hash_ref,h_start,loaded_value,which_table,thread_id,buffer,end);
			wake_up_one_restore(h_start);
		}


		/**
		 * add_key_value
		 * 
		*/

		uint64_t add_key_value(uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t thread_id = 1) {
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
			uint32_t h_start = h_bucket % N;  // scale the hash .. make sure it indexes the array...
			//
			HHash *T = _T0;
			hh_element *buffer	= _region_HV_0;
			hh_element *end	= _region_HV_0_end;
			uint8_t which_table = 0;
			//
			uint64_t loaded_key = UINT64_MAX;
			atomic<uint32_t> *controller = nullptr;
			if ( (offset_value != 0) && wait_if_unlock_bucket_counts(&controller,thread_id,h_start,&T,&buffer,&end,which_table) ) {
				//
				hh_element *hash_ref = (hh_element *)(buffer) + h_start;  //  hash_ref aleady locked (by counter)
				//
				uint32_t tmp_c_bits = hash_ref->c_bits;
				uint32_t tmp_value = hash_ref->_kv.value;
				//
				if ( !(tmp_c_bits & 0x1) && (tmp_value == 0) ) {  // empty bucket
					// put the value into the current bucket and forward
					place_in_bucket_at_base(hash_ref,offset_value,el_key); 
					// also set the bit in prior buckets....
					place_back_taken_spots(hash_ref, 0, buffer, end);
					this->slice_bucket_count_incr_unlock(h_start,which_table,thread_id);
				} else {
					//	save the old value
					uint32_t tmp_key = hash_ref->_kv.key;
					uint64_t loaded_value = (((uint64_t)tmp_value) << HALF) | tmp_key;
					hash_ref->_kv.value = offset_value;  // put in the new values
					hash_ref->_kv.key = el_key;
					if ( tmp_c_bits & 0x1 ) {  // don't touch c_bits (restore will)
						// usurp the position of the current bucket head,
						// then put the bucket head back into the bucket (allowing another thread to do the work)
						wakeup_value_restore(hash_ref, h_start, loaded_value, which_table, thread_id, buffer, end);
					} else {
						uint32_t c = 1;
						hh_element *base_ref = hash_ref;
						for ( uint8_t i = 1; i < NEIGHBORHOOD; i++ ) {
							hash_ref++;
							if ( hash_ref->_kv.value != 0 ) {
								SET(c,i);
							} else if ( hash_ref->c_bits & 0x1 ) {
								c |= (hash_ref->taken_spots << i);
								break;
							}
						}
						base_ref->taken_spots = c;
						//
						// pull this value out of the bucket head (do a remove)
						uint8_t k = 0xFF & (tmp_c_bits >> 1);  // have stored the offsets to the bucket head
						h_start = (h_start > k) ? (h_start - k) : (N + h_start - k);
						hash_ref = buffer + h_start;
						UNSET(hash_ref->c_bits,k);   // the element has be usurped...
						this->slice_unlock_counter(controller,which_table,thread_id);
						// hash the saved value back into its bucket if possible.
						while ( !wait_if_unlock_bucket_counts(&controller,thread_id,h_start,&T,&buffer,&end,which_table) );
						wakeup_value_restore(hash_ref, h_start, loaded_value, which_table, thread_id, buffer, end);
					}
				}
				//
				loaded_key = (((uint64_t)el_key) << HALF) | h_bucket; // LOADED
				loaded_key = stamp_key(loaded_key,which_table);
			}

			return loaded_key;
		}



		// update
		// note that for this method the element key contains the selector bit for the even/odd buffers.
		// the hash value and location must alread exist in the neighborhood of the hash bucket.
		// The value passed is being stored in the location for the key...

		/**
		 * update
		*/
		uint64_t update(uint32_t h_bucket, uint32_t el_key, uint32_t v_value,uint8_t thread_id = 1) {
			//
			if ( v_value == 0 ) return UINT64_MAX;
			//
			uint8_t selector = ((el_key & HH_SELECT_BIT) == 0) ? 0 : 1;
			el_key = clear_selector_bit(el_key);
			//
			hh_element *buffer = (selector ? _region_HV_0 : _region_HV_1);
			hh_element *end = (selector ? _region_HV_0_end : _region_HV_1_end);
			//
			//uint64_t loaded_value = (((uint64_t)v_value) << HALF) | el_key;
			//
			atomic<uint32_t> *controller = this->slice_bucket_lock(h_bucket,selector,thread_id);
			if ( controller ) {
				hh_element *storage_ref = get_ref(h_bucket, el_key, buffer, end);
				//
				if ( storage_ref != nullptr ) {
					storage_ref->_kv.key = el_key;
					storage_ref->_kv.value = v_value;
					this->slice_unlock_counter(controller,selector,thread_id);
					//storage_ref->_V = loaded_value;
					uint64_t loaded_key = (((uint64_t)el_key) << HALF) | h_bucket; // LOADED
					loaded_key = stamp_key(loaded_key,selector);
					return(loaded_key);
				} else {
					this->slice_unlock_counter(controller,selector,thread_id);
					return(UINT64_MAX);
				}
			}
			return(UINT64_MAX);
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

		/**
		 * del
		*/
		uint32_t get(uint32_t h_bucket,uint32_t el_key,[[maybe_unused]] uint8_t thread_id = 1) {  // full_hash,hash_bucket
			//
			uint8_t selector = ((el_key & HH_SELECT_BIT) == 0) ? 0 : 1;
			el_key = clear_selector_bit(el_key);
			//
			hh_element *buffer = (selector ? _region_HV_0 : _region_HV_1);
			hh_element *end = (selector ? _region_HV_0_end : _region_HV_1_end);
			//
			//this->bucket_lock(h_bucket);
			hh_element *storage_ref = get_ref(h_bucket, el_key, buffer, end);
			//
			if ( storage_ref == nullptr ) {
				//this->store_unlock_controller(h_bucket);
				return UINT32_MAX;
			}
			uint32_t V = storage_ref->_kv.value;
			//this->store_unlock_controller(h_bucket);
			//
			return V;
		}


		/**
		 * del
		*/
		uint32_t del(uint64_t loaded_key,uint8_t thread_id = 1) {
			//
			uint32_t el_key = (uint32_t)((loaded_key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t h_bucket = (uint32_t)(loaded_key & HASH_MASK);
			uint8_t selector = ((loaded_key & HH_SELECT_BIT) == 0) ? 0 : 1;
			//
			hh_element *buffer = (selector ? _region_HV_0 : _region_HV_1);
			hh_element *end = (selector ? _region_HV_0_end : _region_HV_1_end);
			//
			atomic<uint32_t> *controller = this->slice_bucket_lock(h_bucket,selector,thread_id);
			if ( controller ) {
				uint32_t i = del_ref(h_bucket, el_key, buffer, end);
				if ( i == UINT32_MAX ) {
					this->slice_unlock_counter(controller,selector,thread_id);
				} else {
					this->slice_bucket_count_decr_unlock(controller,selector,thread_id);
				}
				return i;
			}
			return UINT32_MAX;
		}



		// bucket probing
		//
		//	return a whole bucket... (all values)
		// 
		/**
		 * get_bucket
		*/
		uint8_t get_bucket(uint32_t h_bucket, uint32_t xs[32]) {
			//
			uint8_t selector = ((h_bucket & HH_SELECT_BIT) == 0) ? 0 : 1;
			//
			hh_element *buffer = (selector ? _region_HV_0 : _region_HV_1);
			hh_element *end = (selector ? _region_HV_0_end : _region_HV_1_end);
			hh_element *next = buffer + h_bucket;
			next = el_check_end(next,buffer,end);
			auto c = next->c_bits;
			if ( ~(c & 0x1) ) {
				uint8_t offset = (c >> 0x1);
				next -= offset;
				next = el_check_beg_wrap(next,buffer,end);
				c =  next->c_bits;
			}
			//
			uint8_t count = 0;
			hh_element *base = next;
			while ( c ) {
				next = base;
				uint8_t offset = get_b_offset_update(c);				
				next = el_check_end(next + offset,buffer,end);
				xs[count++] = next->_kv.value;
			}
			//
			return count;	// no value  (values will always be positive, perhaps a hash or'ed onto a 0 value)
		}



	protected:			// these may be used in a test class...
 

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// ----  get_ref
		// ----
		// ----
		/**
		 * get_ref
		*/
		hh_element *get_ref(uint32_t h_bucket, uint32_t el_key, hh_element *buffer, hh_element *end) {
			hh_element *base = buffer + h_bucket;
			base = el_check_end_wrap(base,buffer,end);
			//
			hh_element *next = base;
			auto bucket_bits = (atomic<uint32_t>*)(&base->c_bits);
			//
			uint32_t H = bucket_bits->load(std::memory_order_acquire);
			do {
				uint32_t c = H;
				while ( c ) {
					next = base;
					uint8_t offset = get_b_offset_update(c);
					next += offset;
					next = el_check_end_wrap(next,buffer,end);
					if ( el_key == next->_kv.key ) {
						return next;
					}
				}
			} while ( bucket_bits->compare_exchange_weak(H,H,std::memory_order_acq_rel)  );  // if it changes try again 
			return nullptr;  // found nothing and the bit pattern did not change.
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// del_ref
		//  // caller will decrement count
		//
		/**
		 * del_ref
		*/
		uint32_t del_ref(uint32_t h_start, uint32_t el_key, hh_element *buffer, hh_element *end) {
			hh_element *base = buffer + h_start;
			hh_element *next = base;
			//
			next = el_check_end_wrap(next,buffer,end);
			auto c = next->c_bits;
			while ( c ) {			// search for the bucket member that matches the key
				next = base;
				uint8_t offset = get_b_offset_update(c);				
				next += offset;
				next = el_check_end_wrap(next,buffer,end);
				if ( el_key == next->_kv.key ) {
					removal_taken_spots(base,next,buffer,end);  // bucket header and the matched element
					return offset;
				}
			}
			return UINT32_MAX;
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
	public:


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		void place_in_bucket_at_base(hh_element *hash_ref,uint32_t value,uint32_t el_key) {
			hash_ref->_kv.value = value;
			hash_ref->_kv.key = el_key;
			SET( hash_ref->c_bits, 0);   // now set the usage bits
			hh_element *base_ref = hash_ref;
			uint32_t c = 1;
			for ( uint8_t i = 1; i < NEIGHBORHOOD; i++ ) {
				hash_ref++;
				if ( hash_ref->c_bits & 0x1 ) {
					c |= (hash_ref->taken_spots << i);
					break;
				} else if ( hash_ref->c_bits != 0 ) {
					SET(c,i);
				}
			}
			base_ref->taken_spots = c; 
		}

		/**
		 * place_taken_spots
		*/
		void place_taken_spots(hh_element *hash_ref, uint32_t hole, uint32_t c, hh_element *buffer, hh_element *end) {
			hh_element *vb_probe = hash_ref;
			//
			c = c & zero_above(hole);
			while ( c ) {
				vb_probe = hash_ref;
				uint8_t offset = get_b_offset_update(c);			
				vb_probe += offset;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				vb_probe->taken_spots |= (1 << (hole - offset));   // the bit is not as far out
			}
			//
			place_back_taken_spots(hash_ref, hole, buffer, end);
		}


		/**
		 * place_back_taken_spots
		*/

		void place_back_taken_spots(hh_element *hash_ref, uint32_t hole, hh_element *buffer, hh_element *end) {
			hh_element *vb_probe = hash_ref;
			uint8_t how_far_back = 1;
			while ( how_far_back < NEIGHBORHOOD ) {
				vb_probe = (hash_ref - how_far_back);
				vb_probe = el_check_beg_wrap(vb_probe,buffer,end);
				if ( vb_probe->c_bits & 0x1 ) {
					vb_probe->taken_spots |= (1 << (hole + how_far_back));   // the bit is not as far out
					how_far_back++;
				} else if ( vb_probe->c_bits != 0 ) {
					auto dist = (vb_probe->c_bits >> 1);
					how_far_back += dist;
				} else how_far_back++;
			}
		}

		/**
		 * remove_membership_spots
		*/
		hh_element * remove_membership_spots(hh_element *hash_base,hh_element *vb_nxt,uint32_t c, hh_element *buffer, hh_element *end) {
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
			c = c & zero_above(nxt_loc);
			while ( c ) {
				hh_element *vb_probe = hash_base;
				auto offset = get_b_offset_update(c);
				vb_probe += offset;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				vb_probe->taken_spots &= (~((uint32_t)0x1 << (nxt_loc - offset)));   // the bit is not as far out
			}
		}

		/**
		 * remove_back_taken_spots
		*/

		void remove_back_taken_spots(hh_element *hash_ref, uint32_t hole, hh_element *buffer, hh_element *end) {
			uint8_t how_far_back = 1;
			while ( how_far_back < NEIGHBORHOOD ) {
				hh_element *vb_probe = (hash_ref - how_far_back);
				vb_probe = el_check_beg_wrap(vb_probe,buffer,end);
				if ( vb_probe->c_bits & 0x1 ) {
					vb_probe->taken_spots &= (~((uint32_t)0x1 << (hole + how_far_back)));  // the bit is not as far out
					how_far_back++;
				} else if ( vb_probe->c_bits != 0 ) {
					auto dist = (vb_probe->c_bits >> 1);
					how_far_back += dist;
				} else how_far_back++;
			}
		}


		/**
		 * removal_taken_spots
		*/
		void removal_taken_spots(hh_element *hash_base,hh_element *hash_ref,hh_element *buffer, hh_element *end) {
			//
			uint32_t a = hash_base->c_bits; // membership mask
			uint32_t b = hash_base->taken_spots;
			//
			hh_element *vb_last = nullptr;

			auto c = a;   // use c as temporary
			//
			if ( hash_base != hash_ref ) {  // if so, the bucket base is being replaced.
				uint32_t offset = (hash_ref - hash_base);
				c = c & ones_above(offset);
			}
			//
			vb_last = remove_membership_spots(hash_base,hash_ref,c,buffer,end);
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
		 * pop_oldest_full_bucket
		*/

		void pop_oldest_full_bucket(hh_element *hash_base,uint32_t c,uint64_t &v_passed,uint32_t &time,uint32_t offset,hh_element *buffer, hh_element *end) {
			//
			uint32_t min_base_offset = 0;
			uint32_t offset_nxt_base = 0;
			auto min_probe = hash_base;
			auto min_base = hash_base;
			auto base_probe = hash_base;
			//
			while ( c ) {
				base_probe = hash_base;
				// look for a bucket within range that may give up a position
				while ( c ) {
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					if ( base_probe->c_bits & 0x1 ) {
						offset_nxt_base = offset_nxt;
						break;
					}
					base_probe = hash_base;
				}
				if ( c ) {		// landed on a bucket (now what about it)
					if ( popcount(base_probe->c_bits) > 1 ) {
						//
						auto mem_nxt = base_probe->c_bits;		// nxt is the membership of this bucket that has been found
						mem_nxt = mem_nxt & (~((uint32_t)0x1));   // the interleaved bucket does not give up its base...
						if ( offset_nxt_base < offset ) {
							mem_nxt = mem_nxt & ones_above(offset - offset_nxt_base);  // don't look beyond the window of our base hash bucket
						}
						mem_nxt = mem_nxt & zero_above((NEIGHBORHOOD-1) - offset_nxt_base); // don't look outside the window
						while ( mem_nxt ) {			// same as while c
							auto vb_probe = base_probe;
							auto offset_min = get_b_offset_update(mem_nxt);
							vb_probe += offset_min;
							vb_probe = el_check_end_wrap(vb_probe,buffer,end);
							if ( vb_probe->taken_spots < time ) {
								min_probe = vb_probe;
								min_base = base_probe;
								min_base_offset = offset_min;
							}
						}
					}
				}
			}
			if ( min_base != hash_base ) {  // found a place in the bucket that can be moved...
				min_probe->_V = v_passed;
				min_probe->c_bits = (min_base_offset + offset_nxt_base) << 1;
				time = stamp_offset(time,(min_base_offset + offset_nxt_base));  // offset is now
				min_probe->taken_spots = time;  // when the element is not a bucket head, this is time... 
				min_base->c_bits &= ~(0x1 << min_base_offset);  // turn off the bit
				hash_base->c_bits |= (0x1 << (min_base_offset + offset_nxt_base));
				// taken spots is black, so we don't reset values after this...
			}
		}


		uint32_t inner_bucket_time_swaps(hh_element *hash_ref,uint8_t hole,uint64_t &v_passed,uint32_t &time,hh_element *buffer, hh_element *end) {
			uint32_t a = hash_ref->c_bits;
			a = a & zero_above(hole);
			//
			// unset the first bit of the indexing bits (which indicates this position starts a bucket)
			uint32_t offset = 0;
			hh_element *vb_probe = nullptr;
			//
			a = a & (~((uint32_t)0x1));  // skipping the base (the new element is there)
			while ( a ) {
				vb_probe = hash_ref;
				offset = get_b_offset_update(a);
				vb_probe += offset;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				swap(v_passed,vb_probe->_V);
				time = stamp_offset(time,offset);
				swap(time,vb_probe->taken_spots);  // when the element is not a bucket head, this is time... 
			}
			return offset;
		}


		/**
		 * pop_until_oldest
		*/
		bool pop_until_oldest(hh_element *hash_base, uint64_t &v_passed, uint32_t &time, hh_element *buffer, hh_element *end_buffer,uint32_t &h_bucket) {
			if ( v_passed == 0 ) return false;  // zero indicates empty...
			//
			hh_element *vb_probe = hash_base;
			uint32_t a = hash_base->c_bits; // membership mask
			uint32_t b = hash_base->taken_spots;
			//
			uint8_t hole = countr_one(b);
			uint32_t hbit = (1 << hole);
			if ( hbit < 32 ) {   // if all the spots are already taken, no need to reserve it.
				a = a | hbit;
				b = b | hbit;
			}
			hash_base->c_bits = a;
			hash_base->taken_spots = b;
			//
			uint32_t offset = inner_bucket_time_swaps(hash_base,hole,v_passed,time,buffer,end_buffer);
			// offset will be > 0

			// now the oldest element in this bucket is in hand and may be stored if interleaved buckets don't
			// preempt it.
			a = hash_base->c_bits;
			uint32_t c = ( hbit < 32 ) ? (a ^ b) : ~a;

			// c contains bucket starts.. offset to those to update the occupancy vector if they come between the 
			// hash bucket and the hole.

			if ( b == UINT32_MAX ) {  // v_passed can't be zero. Instead it is the oldest of the (maybe) full bucket.
				// however, other buckets are stored interleaved. These buckets have not been examined for maybe 
				// swapping with the current bucket oldest value.
				pop_oldest_full_bucket(hash_base,c,v_passed,time,offset,buffer,end_buffer);
				//
				//h_bucket += offset;
				return false;  // this region has no free space...
			} else {
				vb_probe = hash_base + hole;
				vb_probe->c_bits = (hole << 1);
				vb_probe->taken_spots = time;
				place_taken_spots(hash_base, hole, c, buffer, end_buffer);
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

		// ---- ---- ---- ---- ---- ---- ----
		//
		bool							_status;
		const char 						*_reason;
		//
		bool							_initializer;
		uint32_t						_max_count;
		uint32_t						_max_n;
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

		bool 							_restore_operational;

		atomic_flag		 				*_random_gen_thread_lock;
		atomic_flag		 				*_random_share_lock;
		atomic<uint32_t>				*_random_gen_region;

		QueueEntryHolder<>				_process_queue;
		atomic_flag						_sleeping_reclaimer;

};


#endif // _H_HOPSCOTCH_HASH_SHM_