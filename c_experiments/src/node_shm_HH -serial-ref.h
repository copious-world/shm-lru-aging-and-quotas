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




// HH control buckets
const uint32_t RESIDENT_BIT_SHIFT = 31;
const uint32_t PARTNERED_BIT_SHIFT = 31;
const uint32_t HOLD_BIT_SHIFT = 23;
const uint32_t DBL_COUNT_MASK_SHIFT = 16;

const uint32_t THREAD_ID_SECTION_CLEAR_MASK = (~THREAD_ID_SECTION & ~RESIDENT_BIT_SET);
const uint32_t THREAD_ID_SHIFT = 16;

const uint32_t DOUBLE_COUNT_MASK_BASE = 0x3F;  // up to (64-1)
const uint32_t DOUBLE_COUNT_MASK = (DOUBLE_COUNT_MASK_BASE << DBL_COUNT_MASK_SHIFT);
const uint32_t HOLD_BIT_SET = (0x1 << HOLD_BIT_SHIFT);
const uint32_t FREE_HOLD_BIT_MASK = ~HOLD_BIT_SET;

const uint32_t RESIDENT_BIT_SET = (0x1 << RESIDENT_BIT_SHIFT);
const uint32_t PARTNERED_BIT_SET = (0x1 << PARTNERED_BIT_SHIFT);
const uint32_t QUIT_SHARE_BIT_MASK = ~RESIDENT_BIT_SET;   // if 0 then thread_id == 0 means no thread else one thread. If 1, the XOR is two.
const uint32_t QUIT_PARTNERED_BIT_MASK = ~PARTNERED_BIT_SET;   // if 0 then thread_id == 0 means no thread else one thread. If 1, the XOR is two.


const uint32_t COUNT_MASK = 0x1F;  // up to (32-1)
const uint32_t HI_COUNT_MASK = (COUNT_MASK<<8);
//



static inline uint32_t add_thread_id(uint32_t target, uint32_t thread_id) {
	auto rethread_id = thread_id & THREAD_ID_BASE;
	if ( rethread_id != thread_id ) return 0;
	rethread_id = (rethread_id << CBIT_THREAD_SHIFT);
	if ( target & HOLD_BIT_SET ) {
		if ( target & RESIDENT_BIT_SET ) return 0;
		target = target | RESIDENT_BIT_SET;
		target = (target & THREAD_ID_SECTION_CLEAR_MASK) | ((target & THREAD_ID_SECTION) ^ rethread_id);
	} else {
		target = target | rethread_id;
	}
	return target;
}


static inline uint32_t remove_thread_id(uint32_t target, uint32_t thread_id) {
	auto rethread_id = thread_id & THREAD_ID_BASE;
	if ( rethread_id != thread_id ) return 0;
	if ( target & HOLD_BIT_SET ) {
		rethread_id = rethread_id << CBIT_THREAD_SHIFT;
		if ( target & RESIDENT_BIT_SET ) {
			target = target & QUIT_SHARE_BIT_MASK;
			target = (target & THREAD_ID_SECTION_CLEAR_MASK) | ((target & THREAD_ID_SECTION) ^ rethread_id);
		} else {
			target = target & QUIT_SHARE_BIT_MASK;
			target = (target & THREAD_ID_SECTION_CLEAR_MASK);
		}
	}
	return target;
}



static inline uint32_t get_partner_thread_id(uint32_t target, uint32_t thread_id) {
	auto rethread_id = thread_id & THREAD_ID_BASE;
	if ( rethread_id != thread_id ) return 0;
	if ( target & HOLD_BIT_SET ) {
		if ( target & RESIDENT_BIT_SET ) {
			uint32_t partner_id = target & THREAD_ID_SECTION;
			partner_id = (partner_id >> CBIT_THREAD_SHIFT) & THREAD_ID_BASE;
			partner_id = (partner_id ^ rethread_id);
			return partner_id;
		} else {
			return 0;
		}
	}
	return 0;
}



const uint32_t HOLD_BIT_ODD_SLICE = (0x1 << (7+8));
const uint32_t FREE_BIT_ODD_SLICE_MASK = ~HOLD_BIT_ODD_SLICE;

const uint32_t HOLD_BIT_EVEN_SLICE = (0x1 << (7));
const uint32_t FREE_BIT_EVEN_SLICE_MASK = ~HOLD_BIT_EVEN_SLICE;


//
// The control bit and the shared bit are mutually exclusive.
// They can both be off at the same time, but not both on at the same time.
//

const uint32_t THREAD_ID_BASE = (uint32_t)0x03F;
const uint32_t THREAD_ID_SECTION = (THREAD_ID_BASE << CBIT_THREAD_SHIFT);
//


// thread id's must be one based... (from 1)

/**
 * thread_id_of
*/
inline uint32_t thread_id_of(uint32_t controls) {
	uint32_t thread_id =  ((controls & THREAD_ID_SECTION) >> THREAD_ID_SHIFT);
	return thread_id & 0xFF;
}

/**
 * bitp_stamp_thread_id - bit partner stamp thread id
*/

inline uint32_t bitp_stamp_thread_id(uint32_t controls,uint32_t thread_id) {
	controls = controls & THREAD_ID_SECTION_CLEAR_MASK; // CLEAR THE SECTION
	controls = controls | ((thread_id & THREAD_ID_BASE) << THREAD_ID_SHIFT) | RESIDENT_BIT_SET;  // this thread is now resident
	return controls;
}

/**
 * check_thread_id
*/
inline bool check_thread_id(uint32_t controls,uint32_t lock_on_controls) {
	uint32_t vCHECK_MASK = THREAD_ID_SECTION | HOLD_BIT_SET;
	bool check = ((controls & vCHECK_MASK) == (lock_on_controls & vCHECK_MASK));
	return check;
}




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


		// interested parties might check if the tow following methods are the same.



		atomic<uint32_t> * bucket_lock2(atomic<uint32_t> *controller,uint32_t &ret_control,uint8_t thread_id) {   // where are the bucket counts??
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

		*/





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


		/**
		 * place_in_bucket_at_base
		*/

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
//cout << bitset<32>(c) << "  ---- " << (int)(vb_nxt - hash_base) << endl;
			while ( c ) {
				hh_element *vb_probe = hash_base;
				auto offset = get_b_offset_update(c);

				cout << bitset<32>(c) << " " << (int)offset << endl;

				if ( c ) {
					vb_probe += offset;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					//
//cout << "VALUES vb_nxt: " << vb_nxt->_V << " vb_probe: " << vb_probe->_V << endl;
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
			vb_last = shift_membership_spots(hash_base,hash_ref,c,buffer,end);
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


		/**
		 * inner_bucket_time_swaps
		*/

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
		 * This method should be caled after `prepare_for_add_key_value_known_refs` which locks the bucket found from `h_bucket`.
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



		// del_ref
		//  // caller will decrement count  -- if this is still happening, it will be during cropping...
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



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



		uint32_t obtain_cell_key(atomic<uint32_t> *a_ky) {
			uint32_t real_ky = (UINT32_MAX-1);
			while ( a_ky->compare_exchange_weak(real_ky,(UINT32_MAX-1)) && (real_ky == (UINT32_MAX-1)));
			return real_ky;
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
					atomic<uint32_t> *a_ky = (atomic<uint32_t> *)(&next->c.key);
					auto ky = obtain_cell_key(a_ky);
					//
					if ( (ky == UINT32_MAX) && c ) {
						auto use_next_c = c;
						uint8_t offset_follow = get_b_offset_update(use_next_c);  // chops off the next position
						hh_element *f_next += offset_follow;
						f_next = el_check_end_wrap(f_next,buffer,end);
						//
						auto value = f_next->->tv.value;
						auto taken = f_next->->tv.taken;
						//
						atomic<uint32_t> *a_f_ky = (atomic<uint32_t> *)(&f_next->c.key);
						uint32_t f_real_ky = obtain_cell_key(f_real_ky);
						//
						f_next->c.key = next->c.key;
						f_next->->tv.value = next->->tv.value;
						if ( f_real_ky != UINT32_MAX ) {
							next->->tv.value = value;
							next->->tv.taken = taken;
							a_ky->store(f_real_ky);  // next->c.key = f_real_ky;
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
				atomic<uint32_t> *a_ky = (atomic<uint32_t> *)(&next->c.key);
				
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
					atomic<uint32_t> *a_f_ky = (atomic<uint32_t> *)(&f_next->c.key);
					auto fky = a_f_ky->load(std::memory_order_acquire);
					auto value = f_next->->tv.value;
					auto taken = f_next->->tv.taken;
					//
					uint32_t f_real_ky = (UINT32_MAX-1);
					while ( a_f_ky->compare_exchange_weak(f_real_ky,(UINT32_MAX-1)) && (f_real_ky == (UINT32_MAX-1)));
					f_next->->tv.value = next->->tv.value;
					f_next->c.key = next->c.key;

					if ( f_real_ky != UINT32_MAX ) {
						next->->tv.value = value;
						next->c.key = f_real_ky;
						next->->tv.taken = taken;
						a_ky->store(f_real_ky);
						a_f_ky->store(UINT32_MAX);
						if ( f_real_ky == el_key  ) {  // opportunistic reader helps delete with on swap and returns on finding its value
							return value;
						}
					}
					//
					// else resume with the nex position being the hole blocker... 
				} else if ( el_key == ky ) {
					auto value = next->->tv.value;
					a_ky->store(ky);
					return value;
				} else {
					a_ky->store(ky);
				}
			}
			return UINT32_MAX;	
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

	

		/**
		 * shift_membership_spots  -- do the swappy read
		*/


		hh_element *a_shift_membership_spots(hh_element *hash_base,hh_element *vb_nxt,uint32_t c, hh_element *buffer, hh_element *end) {
/*
				auto original_cbits = tbits_add_swappy_op(base,cbits,tbits,thread_id);
*/
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
/*
				tbits_remove_swappy_op(a_tbits);
*/

			return nullptr;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		atomic<uint32_t>  *load_stable_key(hh_element *bucket,uint32_t &save_key) {
			atomic<uint32_t>  *stable_key = (atomic<uint32_t>  *)bucket->c.key);
			save_key = stable_key->load(std::memory_order_acquire);
			return stable_key;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	
		/**
		 * _swappy_search_ref
		 * 
		 * Assume that searches start after the base bucket. 
		 * The base bucket is a special case. Make this assumption for both versions of search.
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



		hh_element *_swappy_search_ref(uint32_t el_key, hh_element *base, uint32_t c,uint8_t thread_id, uint32_t &value) {
			//
			hh_element *next = base;
			auto base_bits = c;
			
			c = c & (UINT32_MAX-1);  // skip checking the first postion ... this method assumes the base already checked
			while ( c ) {
				next = base;
				uint8_t offset = get_b_offset_update(c);
				next += offset;
				next = el_check_end_wrap(next,buffer,end);
				// is this being usurped or swapped or deleted at this moment? Search on this range is locked
				//
				atomic<uint64_t> *a_bits_n_ky = (atomic<uint64_t> *)(&(next->c));
				
				//
				// immobility is easiy marked on a member... 
				// on a base, the swappy search marks a thread marked controller while the original cbits are stashed.
				// so the immobility mark can still be used. But, the few editors operating in the bucket will not
				// be able to restore the original cbits until the immobility mark is erased. 

				auto c_n_ky = load_marked_as_immobile(a_bits_n_ky);
				uint32_t nxt_bits = (uint32_t)(c_n_ky & UINT32_MAX);
				auto ky = (c_n_ky >> sizeof(uint32_t)) & UINT32_MAX;
				if ( is_member_bucket(n_cbits) && !(is_cbits_deleted(n_cbits) || (is_cbits_in_mobile_predelete(uint32_t cbits))) ) {
					if ( el_key == chk_key ) {
						value = next->tv.value; // maybe load  (note if this were the root it would be already loaded)
						return next;
					}
				}

				if ( ky == el_key  ) {  // opportunistic reader helps delete with on swap and returns on finding its value
					return next;
				}

				// proceed if another operation is not at the moment doing a swap.
				// pass by reference -- values may be the bucket after swap by another thread
				//
				wait_until_immobile(a_bits_n_ky,nxt_bits,ky,thread_id);
				
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

					value = f_next->->tv.value;
					auto taken = f_next->->tv.taken;
					//
					wait_until_immobile(a_bits_fn_ky,f_nxt_bits,fky,thread_id,true);  // pass by reference

					c_n_ky = (c_n_ky & ((uint64_t)UINT32_MAX << sizeof(uint32_t) )) | f_nxt_bits;
					c_fn_ky = (c_fn_ky & ((uint64_t)UINT32_MAX << sizeof(uint32_t) )) | nxt_bits;
					a_bits_n_ky->store(c_fn_ky);
					a_bits_fn_ky->store(c_n_ky);

					f_next->->tv.value = 0;
					remobalize(f_nexts,true);

					if ( fky != UINT32_MAX ) {  // move a real value closer
						next->->tv.value = value;
						next->->tv.taken = taken;
						if ( fky == el_key  ) {  // opportunistic reader helps delete with on swap and returns on finding its value
							return next;
						}
					}
					// else resume with the nex position being the hole blocker... 
				} else if ( el_key == ky ) {
					return next;
				}
				remobalize(next);
			}
			return UINT32_MAX;	
		}







		/**
		 * tbits_prepare_update
		*/

		uint32_t tbits_prepare_update(atomic<uint32_t> *a_tbits,bool &captured) {   // the job of an editor
			uint32_t real_bits = 0;
			//
			do {
				auto check_bits = a_tbits->load(std::memory_order_acquire);
				if ( is_base_tbits(check_bits) ) {
					real_bits = check_bits;
				} else if ( (check_bits != EDITOR_CBIT_SET) && (check_bits != READER_CBIT_SET) ) {
					real_bits = fetch_real_tbits(check_bits); // the aim of the caller is to get the real bits and maybe wait to update later.
				} else {  // either 0 or EDITOR_CBIT_SET
					// wait to get the bits... another editor owns the spot
					check_bits = tbits_wait_on_editor(a_tbits); // should be zero
					if ( check_bits & 0x1 ) {
						real_bits = check_bits;
					} else {
						real_bits = fetch_real_tbits(check_bits); // the aim of the caller is to get the real bits and maybe wait to update later.
					}
				}
			} while ( !(real_bits & 0x1) );
			// 
			// try to claim the tbis position, but if failing just move on.
			auto maybe_capture = real_bits;
			captured = a_tbits->compare_exchange_weak(maybe_capture,EDITOR_CBIT_SET,std::memory_order_release);
			if ( captured ) {
				if ( a_tbits->load(std::memory_order_relaxed) != EDITOR_CBIT_SET ) {
					captured = false;  // check on suprious conditions
				}
			}
			return real_bits;
		}




		/**
		 * update_cbits
		 * 
		*/
	

		void update_cbits_release(atomic<uint32_t> *base_bits, uint32_t cbits_op_base, uint32_t del_cbits, uint8_t thread_id) {
			auto op_bits = clear_bitsdeleted(cbits_op_base);
			auto del_bits_mask = ~del_cbits;
			a_remove_real_cbits(base_bits,op_bits,del_bits_mask,thread_id);
			auto cbits = fetch_real_cbits(cbits_op_base);
			base_bits->store(cbits,std::memory_order_release);
			// while ( !(base_bits->compare_exchange_weak(cbits_op_base,cbits,std::memory_order_acq_rel)) )
		}



		/**
		 * a_remove_real_cbits
		*/


		void a_remove_real_cbits(atomic<uint32_t> *control_bits, uint32_t op_cbits, uint32_t cbits_mask, uint8_t thread_id) {
			while ( (op_cbits & DELETE_CBIT_SET) && !(op_cbits & 0x1) ) {  // just wait for the deleters to finish
				op_cbits = control_bits->load(std::memory_order_acquire);
			}
			if ( op_cbits & 0x1 ) {  // somehow, some thread gave up owernship of the bucket, but the current thread is supposed to own the bucket
				control_bits->fetch_and(cbits_mask,std::memory_order_release);
			} else {
				atomic<uint32_t> *a_stash_cbit = (atomic<uint32_t> *)(&(_cbits_temporary_store[thread_id]));
				a_stash_cbit->fetch_and(cbits_mask,std::memory_order_release);
			}
		}



		/**
		 * a_remove_single_real_cbit
		*/

		void a_remove_single_real_cbit(atomic<uint32_t> *control_bits, uint32_t op_cbits, uint32_t cbits, uint32_t hbit, uint8_t thread_id) {
			while ( (op_cbits & DELETE_CBIT_SET) && !(op_cbits & 0x1) ) {  // just wait for the deleters to finish
				op_cbits = control_bits->load(std::memory_order_acquire);
			}
			auto bit_clear = ~hbit;
			if ( op_cbits & 0x1 ) {  // somehow, some thread gave up owernship of the bucket, but the current thread is supposed to own the bucket
				control_bits->fetch_and(bit_clear,std::memory_order_release);
			} else {
				auto thrd = bits_thread_id_of(op_cbits);
				atomic<uint32_t> *a_stash_cbit = (atomic<uint32_t> *)(&(_cbits_temporary_store[thrd]));
				a_stash_cbit->fetch_and(bit_clear,std::memory_order_release);
			}
		}




		/**
		 * lock_taken_spots
		*/

		atomic<uint32_t> *lock_taken_spots(hh_element *vb_probe,uint32_t &taken) {
			atomic<uint32_t> *a_taken = (atomic<uint32_t> *)(&(vb_probe->->tv.taken));
			taken = a_taken->load(std::memory_order_acquire);
			while ( !(taken & 0x1) ) {
				taken = a_taken->load(std::memory_order_acquire);
				if ( taken != 0 ) {  // this is a read sempahore
					taken = fetch_real_tbits(taken);
					return a_taken;
				}
			}
			auto prep_taken = 0;  // put in zero for update ops preventing a read semaphore.
			while ( !a_taken->compare_exchange_weak(taken,prep_taken,std::memory_order_acq_rel) );
			return a_taken;
		}


		/**
		 * unlock_taken_spots
		*/

		void unlock_taken_spots(atomic<uint32_t> *a_taken,uint32_t update) {
			auto taken = a_taken->load(std::memory_order_acquire);
			if ( (taken != 0)  && !(taken & 0x1) ) {  // this is a read sempahore
				auto thrd = tbits_thread_id_of(taken);
				atomic<uint32_t> *a_real_tbits = (atomic<uint32_t> *)(&(_tbits_temporary_store[thrd]));
				a_real_tbits->store(update);
			} else if ( taken == 0 ) {
				a_taken->store(std::memory_order_release);
			}
		}




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





/*

				stash_index = _cbit_stash.pop_one_wait_free();		// get a stash records
				auto c_bits_op_update = cbit_member_stash_index_stamp(c_bits_op,stash_index);

				while ( !a_cbits->compare_exchange_weak(c_bits_op,c_bits_op_update,std::memory_order_acq_rel) ) {
					auto already_stashed_mem = cbits_member_stash_index_of(c_bits_op);
					if ( (already_stashed_mem != 0) && (already_stashed_mem != stash_index) ) {		// doing the same thing for member cbits
						_cbit_stash.return_one_to_free(stash_index);  // claim no knowledge of the situation and leave
						// encountered this a few lines back, but it slipped in just now
						CBIT_stash_el *cse = _cbit_stash.stash_el_reference(already_stashed_mem);
						cbit_stashes[0]._cse = cse;			// put into the carrier
						cbit_stashes[0]._is_base = false;
/ *
						CBIT_stash_el *cse = _cbit_stash.stash_el_reference(already_stashed_mem);
						cse->_updating++;
						cbit_stashes[0]._cse = cse;			// put into the carrier .. there is already a base as well
						cbit_stashes[0]._is_base = false;
						auto member_cbits = cse->_real_bits;
						uint8_t backref = 0;
						hh_element *base = cbits_base_from_backref(member_cbits,backref,base,buffer,end);
						uint32_t cb;
						uint32_t cb_op_base;
						load_cbits(base,cb,cb_op_base);
						auto already_stashed = cbits_stash_index_of(cb_op_base);
						if ( already_stashed != 0 ) {
						CBIT_stash_el *cse = _cbit_stash.stash_el_reference(already_stashed);
							cse->_updating++;
							cbit_stashes[0]._cse = cse;
							cbit_stashes[0]._is_base = true;
						}
* /
						return;
					}
				}

				c_bits_op = c_bits_op_update;
*/