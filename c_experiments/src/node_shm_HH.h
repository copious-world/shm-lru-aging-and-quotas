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


/**
 * API exposition
 * 
 * 1. `prepare_for_add_key_value_known_refs` 
 * 				--	start the intake of new elements. Assign a table slice and get memory references
 * 					for use in the call to `add_key_value_known_refs`
 * 
 * 2. `add_key_value_known_refs`
 * 				--	must be set up by the caller with a call to `prepare_for_add_key_value_known_refs`
 * 					Given the slice has been selected, calls `_first_level_bucket_ops`
 * 
 * 3. `update`	--	calls `_internal_update` with a new value to assign to a key... (In current use, this may be rare.)
 * 4. `del`		--	calls `_internal_update` with an out of bounds key and sets up the cropper threads to work on removing the key
 * 
 * 5. `get`		--	calls `_get_bucket_reference`. This is supposed to be the most free (least blocked) method giving  
 * 					much favor to the process of searching for an exact key and return the value associated with it.
 * 
*/

/**
 * cbits: membership bit map for buckets up to (32) members.
 * 
 * At any time a bucket, addressed by a hash index, may be in one of five states:
 * empty --		all information and value words are zero or UINT32_MAX, initially zero.
 * root --		called a base, contains the original cbit membership map, with lowest bit set to 0x1 and highest set to zero (0x0).
 * root in operation	-- a base that is not noop. Stores operational information. The orignial cbit map is stashed, the lowest bit is 0x0 and the highest bit is 0x0.
 * member				-- stores a back reference to a base. The lowes bit is 0x0 and the highest bit is 0x1.
 * member in transition	-- stores operational information, and may store a back reference temporarily. The lowes bit is 0x0 and the highest bit is 0x1.
 * 
*/


/**
 * tbits: a bit map of taken and free buckets starting and up to the window size (32) or sorting data for other buckets
 * 
 * At any time, a bucket may gain, lose or collect bits from other base bit maps. tbits in operation may be in four states:
 * empty		-- 0 for a bucket that has not been used
 * taken spots	-- the map of taken spots, 0x1 for taken 0x1 for free. The lowest bit is set to 0x1, since the base is stored there.
 * operative	-- the lowest bit is cleared 0x0. Others must wait for access or operate on stashed tbits
 * reader		-- the lowest bit is cleared 0x0 and an even (by 2) reader count is stored in the low word. 
 * member order	-- the lowest bit is 0x0 and a weight (often time) is stored for members.
 * 
 * The member order is detected by examining the cbits to see if the cbits highest bit is set to 0x1.
*/


/**
 * Sometime buckets are mobile and sometimes they are not. 
 * 
 * By mobility is meant that the values keys and tbits may be swapped in position relative to the base bucket.
 * 
 * The base (root) is always immobile when considering operations that rearrange elements within a membership.
 * The base is always usurped by a new element and later shifted into the membership. The later operation leaves 
 * the new element alone. 
 * 
 * If the base is being deleted and there are other members, the first member may be placed in the root provided the update
 * queue for the base is empty. The element moved in will be marked for deletion in its old position and will be reclaimed later.
 * 
 * Elements may be swapped in particular circumstances:
 * 1. An element is being pushed in from the base (preserving order, but forcing elements to move to a new hole or out like a queue)
 * 2. An element is being inserted after being usurped, again pushing elements in a queue like operation.
 * 3. An element is being moved to the end of the membership map since it is marked for deletion.
 * 4. Occasionally elements may be sorted.
 * 
 * 
 * It is possible that some applicaiton can sort tbits and keys together.
 * 
 * Memberships may be sometimes sorted. Sorting is determined by keys and tbits for members.
 * -- The basic rule is that if a membership is in tact, then the tbits of members are used for ordering. 
 * -- Otherwise, if the key of an element is UINT32_MAX, the element will treated as having the greatest (least) weight.
 * 
 * In deleting an element, the tbits may be set to zero at some point, immediately or eventually. If tbits represent time, 
 * then a zero tbits will indicate an oldest element (it may be possible to ignore the key).
 * 
 * Sorting memberships is not always necessary. Memberships are searched linearly by key. Sorting can result in compression.
 * The main reason for being nearly sorted is that if a bucket gets full and tries to reinsert an element
 * by swapping memberhip with another bucket, it will prove useful to inspect the older usurpable elements 
 * within the updating updating bucket's window. If the elements are sorted to some degree, the search for an element to usurp
 * in second base, would be from the end of the membership cbits of that base going towards that base. This condition 
 * may only be needed when buckets start to get full. So, sorting might be done occassionally out of band and only after
 * some load factor. A thread that crops can take care of sorting when deleting elements. Furthermore, it can work to 
 * compress buckets. 
 * 
 * 
*/



/**
 * When a bucket becomes full it may steal a spot from another bucket to accomodate a new bucket. 
 * The element stolen, must be the oldest element that can be stolen from a numnber of buckets.
 * Furthermore, these buckets must be within the window of the updating base bucket. And, 
 * the the position yieled should be as close to the best position of the reinserted element.
 * 
 * If the members cbits can indicate the oldest bucket, it may update when the oldest element is removed. 
 * Often, this element will be removed fore memberships swapping if storage is becoming full and a number of 
 * simultaneous strategies are being employed to age out elements in order to reduce density. Updating 
 * references to the oldest element will be minor. 
 * 
 * If oldest element may be reinserted, but it might regularly swap out to secondary storage.
 * Some attempt to keep storage fairly up to date by scheduling aging out operations should aid in 
 * keeping the need for position stealing as a rare case scenario.
 * 
*/


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
			cbits_temporary_store_2 = (uint32_t *)(_end_cbits_temporary_store);
			_end_cbits_temporary_store_2 = _end_cbits_temporary_store_2 + _num_threads;  // *sizeof(uint32_t)
			//
			_tbits_temporary_store = (uint32_t *)(_end_cbits_temporary_store_2);  // these start at the same place offset from start of second table
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
		uint32_t 						*_cbits_temporary_store_2;    // a shared structure
		uint32_t		 				*_end_cbits_temporary_store_2;
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

		uint32_t fetch_real_cbits(uint32_t cbit_carrier,bool second_lock = false) {
			auto thrd = cbits_thread_id_of(cbit_carrier);
			if ( second_lock ) {
				return _cbits_temporary_store_2[thrd];
			}
			return _cbits_temporary_store[thrd];
		}


		void store_real_cbits(uint32_t cbits,uint8_t thrd,bool second_lock = false) {
			if ( second_lock ) {
				_cbits_temporary_store_2[thrd] = cbits;
			}
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

		inline bool empty_bucket(atomic<uint64_t> *a_c,hh_element *base,uint32_t &cbits) {
			auto  cbits_n_key = a_c->load(std::memory_order_acquire);
			cbits = (uint32_t)(cbits_n_key & UINT32_MAX);
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

	// ----

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * tbits_wait_on_editor
		 * 	the aim is to have a 0 in the tbits by the time this returns.
		 * 	The reader wants to turn the tbits into a semaphore, but it wants to stash the real tbits.
		*/
		uint32_t tbits_wait_on_editor(atomic<uint32_t> *a_tbits) {
			auto which_tbits = a_tbits->load(std::memory_order_acquire);
			while ( (which_tbits == EDITOR_CBIT_SET) || (which_tbits == READER_CBIT_SET) ) {
				while ( (which_tbits == READER_CBIT_SET) && a_tbits->load(std::memory_order_acquire) == READER_CBIT_SET ) {  // another reader is about to set the tbits as a semaphore
					tick();
				}
				if ( which_tbits == READER_CBIT_SET ) {
					return a_tbits->load(std::memory_order_relaxed); // this will be the semaphores
				}
				//
				while ( which_tbits == EDITOR_CBIT_SET  ) {  // an editor is working on changing the mem allocation map
					which_tbits = a_tbits->load(std::memory_order_acquire);
					tick();
				}
				//
				auto real_bits = which_tbits; // the real bits should have been put in by the last editor on completion.
				//
				if ( a_tbits->compare_exchange_weak(which_tbits,READER_CBIT_SET,std::memory_order_acq_rel) ) {
					return real_bits;
				}
			}
			return a_tbits->load(std::memory_order_relaxed);
		}



		/**
		 * tbits_prepare_update
		*/

		uint32_t tbits_prepare_update(atomic<uint32_t> *a_tbits,bool &captured) {   // the job of an editor
			uint32_t real_bits = 0;
			//
			do {
				auto check_bits = a_tbits->load(std::memory_order_acquire);
				if ( is_base_tbit(check_bits) ) {
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

		bool reader_sem_stash_tbits(atomic<uint32_t> *a_tbits, uint32_t tbits_stash, uint8_t thread_id) {
			uint32_t reader_bits = tbit_thread_stamp(0x2,thread_id);
			auto expected_tbits = tbits_stash;
			while ( !(a_tbits->compare_exchange_weak(expected_tbits,reader_bits,std::memory_order_acq_rel)) ) {
				if ( expected_tbits != tbits_stash ) {
					if ( expected_tbits == 0 ) return false;
					else {  // this is a reader semaphore setup by another thread
						if ( !(tbits_sem_at_max(reader_tbits)) ) {
							a_tbits->fetch_add(2,std::memory_order_acq_rel);
							return true;
						}
					}
				}
			}
			// 
			store_real_tbits(tbits_stash,thread_id);  // keep the real tbits in a safe place
			return true;
		}


		atomic<uint32_t> *tbits_add_reader(hh_element *base, uint32_t &tbits, uint8_t thread_id, uint32_t &value) {
			//
			//
			atomic<uint64_t> *a_tbits_n_v = (atomic<uint64_t> *)(&(base->tv));
			auto t_n_v = a_tbits_n_v->load(std::memory_order_acquire);
			//
			if ( t_n_v == 0 ) return nullptr;	// a base is truely empty if both the tbits and the value are zero.. 
												// tbits can be 0 if a thread is editing the tbits map.
			//
			uint32_t tbits = (uint32_t)(t_n_v & UINT32_MAX);
			value = ((uint32_t)(t_n_v >> sizeof(uint32_t)) & UINT32_MAX);   // base bucket value VALUE NEVER CHANGES here
			//
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			while ( true ) {
				if ( tbits == 0 ) {
					while ( tbits == 0 ) {
						tick();
						tbits = a_tbits->load(std::memory_order_acquire);
					}
				}  // the thread that updated the tbits may put back a reader semaphore or the updated tbits, depending
				//
				if ( tbits & 0x1 ) {  // the updated or original tbits may be here.
					if ( reader_sem_stash_tbits(a_tbits,tbits,thread_id) ) {  // stash the tbis and make this a reader semaphore (count = 2)
						return a_tbits;
					}
				} else {
					if ( !(tbits_sem_at_max(reader_tbits)) ) {
						a_tbits->fetch_add(2,std::memory_order_acq_rel);
					}
				}
			}
			//
			return nullptr;
		}



		/**
		 * tbits_remove_reader
		*/

		void tbits_remove_reader(atomic<uint32_t> *a_tbits, uint8_t thread_id = 0) {
			//
			auto tbits = a_tbits->load(std::memory_order_acquire);
			//
			auto reader_tbits = tbits;
			if ( tbits & 0x1 ) {
				return;  // the tbits are where they are supposed to be
			}
			//
			if ( tbits_sem_at_zero(tbits) ) {  // it should have been that the last condition returned (maybe in transition)
				return;
			}
			//
			if ( tbits_sem_at_zero(tbits - 2) ) {   // last reader out
				tbits = fetch_real_tbits(reader_cbits);
				auto expect_bits = tbits - 2;
				while ( !(a_tbits->compare_exchange_weak(expect_bits,tbits,2,std::memory_order_acq_rel)) ) {
					if ( expect_bits & 0x1 ) return; // if there was a change, the pattern was updated.
				}
			} else {
				a_tbits->fetch_sub(2,std::memory_order_acq_rel);
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

		bool become_bucket_state_master(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t locking_bits, uint8_t thread_id,bool second_lock) {
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
		 * wait_until_immobile
		 * 
		 * Any thread/process attempting to swap buckets can tell other threads to stop participating in the swap
		 * until the swap completes.
		 * 
		 * If two threads address a bucket at the same time and one swaps, the other thread will read the bucket again
		 * if the swap has occurred.
		*/


		void wait_until_immobile(atomic<uint32_t> *a_cbits, uint32_t &cbits, uint32_t &ky, uint8_t &thread_id,bool second_lock) {
// IMMOBILE_CBIT_SET
		}

		uint64_t load_marked_as_immobile(atomic<uint64_t> *a_b_n_key) {   // a little more cautious
			(*a_b_n_key )|= (uint64_t)IMMOBILE_CBIT_SET
			return a_bits_n_ky->load(std::memory_order_acquire);
		}

		void remobalize(hh_element *bucket,bool second_lock) {

		}


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
		 * _editor_locked_search_ref
		 * 
		 * If in this method, then the original bits are a membership map.
		 * 
		 * Note: Assume that searches start after the base bucket. 
		 * The base bucket is a special case. Make this assumption for both versions of search.
		 * 
		 * 
		*/

		hh_element *_editor_locked_search_ref(atomic<uint32_t> *base_bits, uint32_t original_bits, hh_element *base, uint32_t el_key, uint8_t thread_id, uint32_t &value) {
			//
			uint32_t H = original_bits;   // cbits are from a call to empty bucket always at this point
			//
			hh_element *next = base;
			do {
				uint32_t c = H & (UINT32_MAX-1);  // skip checking the first postion ... this method assumes the base already checked
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
							load_marked_as_immobile(a_n_b_n_key);
							value = next->tv.value;   // maybe load
							return next;
						}
					}
				}
				if ( no_cbit_change(base_bits,original_bits) ) {
					break;
				} else {
					if ( empty_bucket(a_n_b_n_key,next,cbits) ) {
						break;
					}
					H = is_base_noop(cbits) ? cbits : fetch_real_cbits(cbits);
				}
			} while ( true );  // if it changes try again 
			return nullptr;
		}



		/**
		 * _get_bucket_reference
		 * 
		 * Note: pointers returned remain locked until the operation using them is complete.
		 * Note: the base bucket is a special case and it can be examined for a value provided that 
		 * an actual value swap is not in process. 
		*/

		uint32_t wait_if_base_update(atomic<uint64_t> *a_base_c_n_k,uint32_t cbits) {
			cbits_n_ky = a_base_c_n_k->load();
			auto base_ky = (uint32_t)((cbits_n_ky >> sizeof(uint32_t)) & UINT32_MAX);
			return base_ky;
		}


		bool is_swappy(uint32_t cbits,uint32_t tbits) {

			return false;
		}


		uint32_t tbits_add_swappy_op(base,cbits,tbits,value,thread_id) {

		}

		void tbits_remove_swappy_op(atomic<uint32_t> *a_tbits) {

		}



/*
auto original_cbits = tbits_add_swappy_reader(base,cbits,tbits,value,thread_id);
fetch_real_cbits(cbits);   // Since the base is in operation, the orginal bits will be in the `_cbits_temporary_store`.
*/

		hh_element *_get_bucket_reference(atomic<uint32_t> *bucket_bits, hh_element *base, uint32_t cbits, uint32_t h_bucket, uint32_t el_key, hh_element *buffer, hh_element *end,uint8_t thread_id,uint32_t &value) {
			//
			if ( el_key == UINT32_MAX ) return nullptr; // this should be worked out earlier but for completeness allow this check
			//
			// bucket_bits are the base of the bucket storing the state of cbits
			uint32_t tbits = 0;
			atomic<uint32_t> *a_tbits = tbits_add_reader(base,tbits,value,thread_id);  // lockout ops that swap elements
			atomic<uint64_t> *a_base_c_n_k = (atomic<uint64_t> *)(&(base->c));
			//
			// get the original bits even in a dynamic situation, and get the current representation
			// stash cbits allowing readers to pass through. Only swappy operations need to be locked out for non swappy reads
			auto base_ky = wait_if_base_update(a_base_c_n_k,cbits); // a_base_c_n_k->load(std::memory_order_acquire);
			if ( base_ky == el_key ) {
				release_when_base_update(a_base_c_n_k,a_tbits);	// last reader out restores cbits.
				tbits_remove_reader(a_tbits);					// last reader out restores tbits 
				return base;
			}
			//
			if ( !(is_swappy(cbits,tbits)) ) {  // 
											// swappy operation occurs in this bucket for now, make it exclusive to readers 
										  	// and don't make the readers do work on behalf of anyone
				hh_element *hhe = _editor_locked_search_ref(bucket_bits,cbits,base,el_key,thread_id,value);
				tbits_remove_reader(a_tbits);
				return hh;
			} else {
				auto original_cbits = tbits_add_swappy_op(base,cbits,tbits,value,thread_id);
				auto ref = _swappy_search_ref(el_key, base, original_cbits,thread_id,value);
				tbits_remove_swappy_op(a_tbits);
				tbits_remove_reader(a_tbits);
				return ref;
			}
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
						}  			// else --- on failing, try again ... some operation got in ahead of this. 
					} else {  // THIS BUCKET IS ALREADY BEING EDITED -- let the restore thread shift the new value in
						auto current_thread = cbits_thread_id_of(cbits);
						auto current_membership = fetch_real_cbits(cbits);
							// results in first time insertion and search for a hole (could alter time by a small amount to keep a sort)
						wait_on_max_queue_incr(control_bits,locking_bits);
						uint8_t require_queue = cbits_thread_id_of(cbits);
						uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
						wakeup_value_restore(HH_FROM_BASE_AND_WAIT,control_bits, current_membership, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, current_thread, buffer, end, require_queue);
					}
				} else {  // this bucket is a member and cbits carries a back_ref. (this should be usurped)
					// usurping gives priortiy to an entry whose hash target a bucket absolutely
					locking_bits = gen_bitsmember_usurped(thread_id,locking_bits);
					if ( become_bucket_state_master(control_bits,cbits,locking_bits,thread_id) ) { // OWN THIS BUCKET
						//
						uint8_t op_thread_id = 0;
						if ( is_member_usurped(cbits,op_thread_id) )  {  // attempting the same operation 
							// the element is enqueued so as to allow delay to be worked out by 
							// serialization on the queue.			
							uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
							auto current_membership = fetch_real_cbits(cbits);
							uint8_t require_queue = cbits_thread_id_of(cbits);
							// wait for the bucket to become a base and then shift into the bucket 
							wait_on_max_queue_incr(control_bits,locking_bits);
							wakeup_value_restore(HH_FROM_USURP_AND_WAIT,control_bits, current_membership, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, op_thread_id, buffer, end, require_queue);
						} else if ( is_member_in_mobile_predelete(cbits) || is_deleted(cbits) ) {
							// bucket is being abandoned -- store the value and lock out other editors -- 
							// no need to usurp... 
							// but, be careful to keep the taken maps and membership maps consistent
							bucket->c.key = el_key;   // operate cooperatively -- fix this as immobile (if yet to be deleted, the cbits of the base have yet to change)
							bucket->->tv.value = offset_value;
							//
							uint8_t backref = 0;			// get thise by ref
							uint32_t base_cbits = 0;
							uint32_t base_taken_bits = 0;
							// get sel base (header inline)
							hh_element *cells_base = cbits_base_from_backref(backref,bucket,buffer,end_buffer,base_cbits,base_taken_bits);
							//
							auto update_base_cbits = clear_cbit_member(base_cbits,backref);  // perform delete op if not done already
							if ( update_base_cbits != base_cbits ) {  // only if the delete got ahead at this point.
								base_taken_bits = base_taken_bits | (0x1 << backref);  // put the bit back...
								install_base_cbit_update(cells_base,update_base_cbits,base_taken_bits);  // rewrite base bits and taken bits
							}
							// complete the taken bit map for this new base
							uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
							// -- in this case, `wakeup_value_restore` fixes the taken bits map
							wakeup_value_restore(HH_FROM_EMPTY,control_bits, cbits, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, thread_id, buffer, end);
						} else {  // question about cell dynamics have been asked...   USURP
							uint8_t backref = 0;			// get thise by ref
							uint32_t base_cbits = 0;
							uint32_t base_taken_bits = 0;
							// get sel base (header inline)
							hh_element *cells_base = cbits_base_from_backref(backref,bucket,buffer,end_buffer,base_cbits,base_taken_bits);
							//
							// The base of the cell holds the membership map in which this branch finds the cell.
							// The base will be altered... And, the value of the cell should be put back into the cell.
							// BECOME the editor of the base bucket... (any other ops should not be able to access the cell bucket 
							// 	but, the cells base might be busy in conflicting ways. This results in a delay for this condition.)
							atomic<uint32_t> *base_control_bits = (atomic<uint32_t> *)(&(cells_base->c.bits));
							auto cell_base_locking_bits = gen_bits_editor_active(thread_id);   // editor active
							while ( !become_bucket_state_master(base_control_bits,base_cbits,cell_base_locking_bits,thread_id,true) ) { tick() };
							//
							uint32_t save_key = 0;
							atomic<uint32_t>  *stable_key = load_stable_key(bucket,save_key);
							uint32_t save_value = bucket->tv.value;
							stash_key_value(save_key,save_value,thread_id);  // stored momentarily in two places. One is shared. The queue is a single process object.
							// OWN THIS BUCKET
							store_real_cbits(0x1,thread_id);  // store for the bucket, the first ownership
							bucket->->tv.value = offset_value;
							stable_key->store(el_key);
							// drop ownership of the usurped bucket
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
		 * Note: callers do not lock the base bucket before any attempt at `_get_bucket_reference` which may or may not be
		 * conentiouss.
		 * 
		 * Note: callers are update and delete, which change the value of the bucket and maybe the key. Delete changes the 
		 * key of the bucket, making the bucket into a cell that will be cooperatively swapped to the end of the membership map
		 * so that later it may be removed by cropping.
		*/

		uint64_t _internal_update(atomic<uint64_t> *a_c_bits, hh_element *base, uint32_t cbits, uint32_t el_key, uint32_t h_bucket, uint32_t v_value, uint8_t thread_id, uint32_t el_key_update = 0) {
			//
			uint32_t value = 0;
			hh_element *storage_ref = _get_bucket_reference(a_c_bits, base, cbits, h_bucket, el_key, buffer, end, thread_id,value);  // search
			//
			if ( storage_ref != nullptr ) {
				// 
				atomic<uint64_t> *a_cell_cbits_ky = (atomic<uint64_t> *)(&(storage_ref->c));
				//
				auto cbits_ky = a_cell_cbits_ky->load(std::memory_order_acquire);

				if ( el_key_update != 0 ) {
					auto update_key_cbits = (el_key_update & ((uint64_t)UINT32_MAX << sizeof(uint32_t)));
					cbits_ky = update_key_cbits | (cbits_ky & (uint64_t)IMMOBILE_CBIT_RESET);
				} else {
					cbits_ky &= IMMOBILE_CBIT_RESET64;
				}
				//
				storage_ref->->tv.value = v_value;
				//
				uint64_t loaded_key = (((uint64_t)el_key) << HALF) | v_value; // LOADED
				loaded_key = stamp_key(loaded_key,selector);
				//
				// clear mobility lock
				a_cell_cbits_ky->store(cbits_ky,std::memory_order_release);
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
		 * Determines the slice an entry will occupy. 
		 * 
		 * Returns: the pointer to hh_element.c.bits as an atomic reference.
		 * 
		 * Returns: which_table, cbits by reference. 
		 * which_table -- the selection of the slice
		 * cbits -- the membership map for the bucket (which may be empty, a root, a root in operation, a member, a member in transition)
		 * 
		 * Returns:  bucket, buffer, end_buffer by address `**`
		 * 
		 * bucket_ref -- contains the address of the element at the offset `h_bucket`
		 * buffer_ref -- contains the address of the storage region for the selected slice
		 * end_buffer_ref -- contains the address of the end of the storage region
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
			atomic<uint64_t> *a_c_bits = (atomic<uint64_t> *)(&(base->c));
			uint32_t cbits = 0;
			if ( empty_bucket(a_c_bits,base,cbits) ) return UINT32_MAX;   // empty_bucket cbits by ref
			//
			uint32_t value = 0;
			hh_element *storage_ref = _get_bucket_reference(a_c_bits, base, cbits, h_bucket, el_key, buffer, end, thread_id, value);  // search
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
			atomic<uint64_t> *a_c_bits = (atomic<uint64_t> *)(&(base->c));
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
			atomic<uint64_t> *a_c_bits = (atomic<uint64_t> *)(&(base->c));
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
		 * complete_bucket_creation
		*/


		void complete_bucket_creation(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t locking_bits, hh_element *bucket, hh_element *buffer, hh_element *end_buffer) {
			//
			a_place_back_taken_spots(bucket, 0, buffer, end);
			create_base_taken_spots(bucket, buffer, end);   // this bucket is locked 
			//
		}


		bool complete_add_bucket_member(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t locking_bits, hh_element *bucket, hh_element *buffer, hh_element *end_buffer) {
			atomic<uint32_t> *tbits = (atomic<uint32_t> *)(&bucket->->tv.taken);
			uint8_t hole;
			if ( reserve_membership(control_bits,bucket,tbits,cbits,prev_bits,hole,thread_id,buffer,end_buffer) ) {
				//
				store_real_cbits(cbits,current_thread);   // reserve a spot using prev bits and take spots
				inner_bucket_time_swaps(hash_base,a,hole,v_passed,time,which_table,buffer,end_buffer,thread_id);  // thread id
				//
				return true;
			}
			return false;
		}


		bool complete_add_bucket_member_late(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t locking_bits, hh_element *bucket, hh_element *buffer, hh_element *end_buffer) {
			atomic<uint32_t> *tbits = (atomic<uint32_t> *)(&bucket->->tv.taken);
			uint8_t hole;
			if ( reserve_membership(control_bits,bucket,tbits,cbits,prev_bits,hole,thread_id,buffer,end_buffer) ) {
				//
				store_real_cbits(cbits,current_thread);   // reserve a spot using prev bits and take spots
				inner_bucket_time_swaps(hash_base,a,hole,v_passed,time,which_table,buffer,end_buffer,thread_id);  // thread id
				//
				return true;
			}
			return false;
		}



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
			bool did_produce_stashed_kv = false;

			switch ( update_type ) {
				case HH_FROM_EMPTY: {
					// at this point the empty bucket should be totally claimed, however, 
					// the memory map (tbits) are not set up causing hole discovery and search 
					// to have points of failure... This should be easily rectified
					atomic<uint32_t> *control_bits = (atomic<uint32_t> *)(&(hash_ref->c.bits));
					auto prev_bits = control_bits->load(std::memory_order_acquire);
					///
					auto cbits = fetch_real_cbits(prev_bits);
					complete_bucket_creation(control_bits, cbits, prev_bits, hash_ref, buffer, end_buffer);
					//
					// update tbits in window
					uint8_t count_result = 0;
					q_count_decr(prev_bits,count_result);
					if ( count_result == 0 ) {
						control_bits->store(cbits);  // the bucket is now a new base				
					}
					break;
				}
				case HH_FROM_BASE: {
					// at this point, the value in the base is the new value.
					// The value taken from the restore queue has to be shifted into the membership map. cbits.
					// the cbits must be updated by acquiring a new hole identified by the taken bits.
					// it is possible that by the time the value to be restored arrives here that
					// the taken positions available to the base are gone. In that case, a version of usurpation
					// may occur, or the oldest elements (or deleted and not yet serviced elements) may be shifted out.
					atomic<uint32_t> *control_bits = (atomic<uint32_t> *)(&(hash_ref->c.bits));
					auto prev_bits = control_bits->load(std::memory_order_acquire);
					///
					// store ....  pop until oldest
					auto cbits = fetch_real_cbits(prev_bits);
					if ( !(complete_add_bucket_member(control_bits, cbits, locking_bits, loaded_value, store_time, hash_ref, buffer, end_buffer, thread_id)) ) {
						did_produce_stashed_kv = handle_full_bucket_by_usurp(hash_base,c,v_passed,time,offset,buffer,end_buffer);
					}
					//
					uint8_t count_result = 0;
					q_count_decr(prev_bits,count_result);
					if ( count_result == 0 ) {
						control_bits->store(fetch_real_cbits(prev_bits));  // the bucket is now a new base				
					}
					// note: there may be a sequence of new value in the queue slightly older than the occupying value
					break;
				}
				case HH_FROM_BASE_AND_WAIT: {
					// this is the same as `HH_FROM_BASE` but the new value is being shifted into the base
					// when a blocking operation was in process when the value arrived.
					atomic<uint32_t> *control_bits = (atomic<uint32_t> *)(&(hash_ref->c.bits));
					auto prev_bits = control_bits->load(std::memory_order_acquire);
					// store ....  pop until oldest
					if ( !(complete_add_bucket_member_late(control_bits, cbits, locking_bits, loaded_value, store_time, hash_ref, buffer, end_buffer, thread_id)) ) {
						did_produce_stashed_kv = handle_full_bucket_by_usurp(hash_base,c,v_passed,time,offset,buffer,end_buffer);
					}
					//
					uint8_t count_result = 0;
					q_count_decr(prev_bits,count_result);
					if ( count_result == 0 ) {
						control_bits->store(fetch_real_cbits(prev_bits));  // the bucket is now a new base				
					}
					break;
				}
				case HH_FROM_BASE_USURP: {
					// Similar to `HH_FROM_BASE_AND_WAIT` but that the value of the base just lost a position.
					// The cbits of the base must reflect the removal of position, while the tbits will not change.
					// Then the element should be shifted into the base in a time ordered way. If the element is the 
					// oldest element from the base, then it may be aged out of the store. 
					atomic<uint32_t> *control_bits = (atomic<uint32_t> *)(&(hash_ref->c.bits));
					auto prev_bits = control_bits->load(std::memory_order_acquire);
					// store ....  pop until oldest
// 	Like this but the taken bits might not change except a new hole.
					auto cbits = fetch_real_cbits(prev_bits);
					precbitsv_bits = cbit_clear_bit(cbits,hole);
					control_bits->store(cbits);
					cbits = pop_until_oldest(hash_ref, loaded_value, store_time, which_table, buffer, end, thread_id);
					//
					// store ....  pop until oldest
					if ( !(complete_bucket_creation(control_bits, cbits, prev_bits, hash_ref, buffer, end_buffer)) ) {
						did_produce_stashed_kv = handle_full_bucket_by_usurp(hash_base,c,v_passed,time,offset,buffer,end_buffer);
					}
					//
					uint8_t count_result = 0;
					q_count_decr(prev_bits,count_result);
					if ( count_result == 0 ) {
						control_bits->store(cbits);  // the bucket is now a new base				
					}
					break;
				}
				case HH_FROM_USURP_AND_WAIT: {
					// The rare case that two insertion operation find the same bucket to be usurped. But, 
					// one gets there first. The following element will shift the new element in using `HH_FROM_BASE`
					// operations. The time ordering of the first element is lost to memory management of the tbits
					// while the second element will be the new newest element. If other similar elements (key,value)
					// are in the queue, then they will shift in making the cbit sequence longer.
					atomic<uint32_t> *control_bits = (atomic<uint32_t> *)(&(hash_ref->c.bits));
					auto prev_bits = control_bits->load(std::memory_order_acquire);
					//
					cbits = pop_until_oldest(hash_ref, loaded_value, store_time, which_table, buffer, end, thread_id);
					//
					// store ....  pop until oldest
					if ( !(complete_add_bucket_member_late(control_bits, cbits, locking_bits, loaded_value, store_time, hash_ref, buffer, end_buffer, thread_id)) ) {
						did_produce_stashed_kv = handle_full_bucket_by_usurp(hash_base,c,v_passed,time,offset,buffer,end_buffer);
					}
					//
					uint8_t count_result = 0;
					q_count_decr(prev_bits,count_result);
					if ( count_result == 0 ) {
						control_bits->store(fetch_real_cbits(prev_bits));  // the bucket is now a new base				
					}
					break;
				}
				default: {
					// an impossible state of affairs unless someone has altered the insertion methods. 
					break;
				}
			}

			if ( did_produce_stashed_kv ) {
				// if the entry can be entered onto the short list (not too old) then allow it to be added back onto the restoration queue...
				if ( short_list_old_entry(loaded_value, store_time) ) {
					// the bucket of membership is known and returned by `handle_full_bucket_by_usurp`
					auto current_thread = cbits_thread_id_of(cbits);
					auto current_membership = fetch_real_cbits(cbits);
						// results in first time insertion and search for a hole (could alter time by a small amount to keep a sort)
					wait_on_max_queue_incr(control_bits,locking_bits);
					uint64_t current_loaded_value = (((uint64_t)el_key) << HALF) | offset_value;
					wakeup_value_restore(HH_FROM_BASE_AND_WAIT,control_bits, current_membership, locking_bits, bucket, h_bucket, 0, current_loaded_value, which_table, current_thread, buffer, end);
				}
			}

		}


		/**
		 * enqueue_restore
		*/
		void enqueue_restore(hh_adder_states update_type, hh_element *hash_ref, uint32_t h_bucket, uint64_t loaded_value, uint8_t hole, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end,uint8_t &require_queue = 0) {
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
			if ( require_queue ) {
				p = _process_table + require_queue;
			}
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

		void wakeup_value_restore(hh_adder_states update_type,hh_element *hash_ref, uint32_t h_start, uint64_t loaded_value, uint8_t hole, uint8_t which_table, uint8_t thread_id, hh_element *buffer, hh_element *end,uint8_t &require_queue) {
			// this queue is jus between the calling thread and the service thread belonging to just this process..
			// When the thread works it may content with other processes for the hash buckets on occassion.
			if ( require_queue ) {
				enqueue_restore(update_type,hash_ref,h_start,loaded_value,hole,which_table,thread_id,buffer,end,require_queue);
			} else {
				enqueue_restore(update_type,hash_ref,h_start,loaded_value,hole,which_table,thread_id,buffer,end);
			}
			wake_up_one_restore();
		}



		/**
		 * wait_on_max_queue_incr
		*/

		void wait_on_max_queue_incr(atomic<uint32_t> *control_bits,uint32_t cbits) {
			uint8_t count_result = 0;
			auto cbits_update = q_count_incr(cbits, count_result);
			do {
				while ( cbits_update == cbits ) {  // this happens when the cbits are maxed out
					tick();
					cbits = control_bits->load(std::memory_order_acquire);
					cbits_update = q_count_incr(cbits, count_result);
				}
				while ( !(control_bits->compare_exchange_weak(cbits,cbits_update,std::memory_order_acq_rel)) ) {
					cbits_update = q_count_incr(cbits, count_result);
					if ( cbits_update == cbits ) break;  // another thread maxed it out -- back to waiting.
				}
			} while ( count_result == Q_COUNTER_MAX );
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
		 * handle_full_bucket_by_usurp
		*/
		void handle_full_bucket_by_usurp(hh_element *hash_base,uint32_t c,uint64_t &v_passed,uint32_t &time,uint32_t offset,hh_element *buffer, hh_element *end) {
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
				while ( !(hb_t_bits->compare_exchange_weak(b_prev,b,std::memory_order_release)) ) { b = b_prev | hbit; }
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
				handle_full_bucket_by_usurp(hash_base,c,v_passed,time,offset,buffer,end_buffer);
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

		uint32_t a_tbits_store(atomic<uint32_t> *a_real_tbits, uint32_t operative_tbits, uint32_t original_tbits, uint8_t &hole) {
			//
			uint32_t hbit = (1 << hole);   // hbit is the claiming bit
			//
			auto b = original_tbits | hbit;   // declare the position taken
			//
			while ( !(a_real_tbits->compare_exchange_weak(original_tbits,b,std::memory_order_acq_rel)) ) {
				// stored value is not expected ... another thread got the position first
				if ( (original_tbits & hbit) != 0 ) {
					hole = countr_one(b);   // try for another hole ... someone else got last attempt
					if ( hole >= sizeof(uint32_t) ) {   // the bucket is full already
						break;
					}
					hbit = (1 << hole);
				} // else this didn't affect the position of choice ... two threads two different positions
				b = original_tbits | hbit;  // either hbit changed or we are trying to set the same spot
			}

			return b;
		}

		/**
		 * a_store_real_cbits
		 * 
		 * store in the stash contentiously... the bit must be set in the stash (the bucket is currently owned)
		 * another thread may clear, but not set any bit in the membership map. It may clear the same bit,
		 * but, this method must then set the bit finally.
		 * 
		 * op_cbits -- the operation cbits
		*/
		void a_store_real_cbits(atomic<uint32_t> *control_bits, uint32_t op_cbits, uint32_t cbits, uint32_t hbit, uint8_t thread_id) {
			while ( (op_cbits & DELETE_CBIT_SET) && !(op_cbits & 0x1) ) {  // just wait for the deleters to finish
				op_cbits = control_bits->load(std::memory_order_acquire);
			}
			if ( op_cbits & 0x1 ) {  // somehow, some thread gave up owernship of the bucket, but the current thread is supposed to own the bucket
				control_bits->fetch_or(hbit,std::memory_order_release);
			} else {
				auto thrd = bits_thread_id_of(op_cbits);
				atomic<uint32_t> *a_stash_cbit = (atomic<uint32_t> *)(&(_cbits_temporary_store[thrd]));
				control_bits->fetch_or(hbit,std::memory_order_release);
			}
		}


		bool reserve_membership(atomic<uint32_t> *control_bits, hh_element *bucket, atomic<uint32_t> *tbits, uint32_t cbits, uint32_t original_cbits, uint8_t &hole, uint8_t thread_id, hh_element *buffer, hh_element *end_buffer ) {
			// first unload the taken spots... 
			while ( true ) {
				auto a = original_cbits;    // these should be stored in the thread table at the time of call ... cbits indicate the ownership of the cell
				uint32_t b = tbits->load(std::memory_order_acquire);
				auto b_prime = b;
				if ( !(b & 0x1) ) {  // tbits have been stashed
					b = fetch_real_tbits(b);
				}
				//
				hole = countr_one(b);  // first zero anywhere inside the word starting from lsb. Hence closest to the base
				if ( hole < sizeof(uint32_t) ) {
					//
					uint32_t hbit = (1 << hole);   // hbit is the claiming bit
					b = b | hbit;
					if ( !(b_prime & 0x1) ) {
						auto thrd = tbits_thread_id_of(operative_tbits); // contend to set tbis (or cooperatively) in the stash
						atomic<uint32_t> *a_real_tbits = (atomic<uint32_t> *)(&(_tbits_temporary_store[thrd]));
						a_tbits_store(a_real_tbits,b_prime, b, hole);
					} else {
						a_tbits_store(tbits,b_prime, b, hole);
					}
					if ( hole < sizeof(uint32_t) ) {  // a free position in the bucket range has been found
						hh_element *reserved = bucket + hole;
						atomic<uint32_t> *a_reserved = (atomic<uint32_t> *)(reserved->c.bits);
						//
						uint32_t cc = 0;
						auto new_member = (hole << 1) | CBIT_BASE_MEMBER_BIT;
						if ( a_reserved->compare_exchange_weak(cc,new_member,std::memory_order_acq_rel) ) {
							auto c = (a ^ b);    // c contains bucket starts..
							a_place_taken_spots(hash_base, hole, c, buffer, end_buffer);  // add he position to the base memory record
							a_store_real_cbits(control_bits,cbits,original_cbits,hbit,thread_id);  // since we are here, there bucket is owned by the thread
							return true
						}
					} else {
						break;
					}
				} else {
					break;
				}
			}
			//
			return false;
		}






		/**
		 * place_back_taken_spots
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



		/**
		 * a_place_taken_spots
		 * 
		 * c contains bucket starts..  Each one of these may be busy....
		 * but, this just sets the taken spots; so, the taken spots may be treated as atomic...
		 * 
		*/
		void a_place_taken_spots(hh_element *hash_ref, uint32_t hole, uint32_t c, hh_element *buffer, hh_element *end) {
			hh_element *vb_probe = nullptr;
			//
			c = c & zero_above(hole);
			while ( c ) {
				vb_probe = hash_ref;
				uint8_t offset = get_b_offset_update(c);			
				vb_probe += offset;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				//
				uint32_t taken_bit = (1 << (hole - offset));  // this spot has been taken
				//
				atomic<uint32_t> *a_real_tbits = (atomic<uint32_t> *)(&(vb_probe->->tv.taken));
				auto tbits = a_real_tbits->load(std::memory_order_acquire);
				while ( !(tbits & 0x1) ) {  // wait for editors and readers
					tick();
					tbits = a_real_tbits->load(std::memory_order_acquire);
				}
				a_real_tbits->fetch_or(taken_bit);
			}
			//
			a_place_back_taken_spots(hash_ref, hole, buffer, end);
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
					unlock_taken_spots(all_taken_spot_refs[i],all_taken_spots[i]);
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
			atomic<uint64_t> *base_bits_n_ky = (atomic<uint64_t> *)(&(base->c));
			atomic<uint32_t> *base_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			uint32_t h_bucket = (uint32_t)(base - buffer);

			auto locking_bits = gen_bits_editor_active(thread_id);   // editor active
			//
			uint32_t value = 0;
			hh_element *href = _get_bucket_reference(base_bits_n_ky, base, cbits, h_bucket, UINT32_MAX, buffer, end, thread_id, value);
			if ( href ) {
				//
				atomic<uint32_t> *base_bits = (atomic<uint32_t> *)(&(base->c.bits));
				// clear mobility lock
				// mark as deleted 
				while( !become_bucket_state_master(base_bits, cbits, locking_bits, thread_id) ) {
					tick();
				}

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