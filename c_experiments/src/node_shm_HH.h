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


// WORK_ON_THESE
// short_list_old_entry
// wait_until_immobile
// wait_if_base_update
// become_bucket_state_master
// release_bucket_state_master
// wait_become_bucket_delete_master
// release_delete_master



#define MAX_THREADS (64)
#define EXPECTED_THREAD_REENTRIES (8)


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
	HH_FROM_EMPTY,			// from zero 
	HH_FROM_BASE,			// overwritten and old value queued  (wait during overwrite)
	HH_FROM_BASE_AND_WAIT,	// queued no overwrite (no wait)
	HH_USURP,			// specifically a member is being overwritten
	HH_USURP_BASE,		// secondary op during usurp (may not need to wait)
	HH_DELETE, 			// more generally members
	HH_DELETE_BASE,		// specifically rewiting the base or completely erasing it
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
		atomic<uint32_t>	*control_bits;
		hh_element 			*hash_ref;
		//
		uint32_t 			cbits;
		uint32_t 			cbits_op;
		uint32_t 			cbits_op_base;
		uint32_t 			h_bucket;
		uint32_t			el_key;
		uint32_t			value;
		hh_element			*buffer;
		hh_element			*end;
		uint8_t				hole;
		uint8_t				which_table;
		hh_adder_states		update_type;
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





inline uint8_t get_b_reverse_offset_update(uint32_t &c) {
	uint8_t offset = countl_zero(c);
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






// THREAD CONTROL

inline void tick() {
	this_thread::sleep_for(chrono::nanoseconds(20));
}

inline void thread_sleep(uint8_t ticks) {
	microseconds us = microseconds(ticks);
	auto start = high_resolution_clock::now();
	auto end = start + us;
	do {
		std::this_thread::yield();
	} while ( high_resolution_clock::now() < end );
}



// ---- ---- ---- ---- ---- ----  HHash
// HHash <- HHASH

/**
 * CLASS: SliceSelector
 * 
 * Retains reference to the data structures and provides methods interfacing stashes.
 * Provides methods for managing the selection of a slice based upon bucket membership size.
 * Also, this class introduces methods that provide final implementations of the random bits for slice selections.
 * 
*/


class SliceSelector : public Random_bits_generator<> {


public:

		SliceSelector() {
		}

		virtual ~SliceSelector() {
		}

		void initialize_randomness(void) {
			_random_gen_region->store(0);
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


		// C BITS

		uint32_t fetch_real_cbits(uint32_t cbit_carrier) {
			if ( is_base_noop(cbit_carrier) ) {
				return cbit_carrier;
			} else {
				auto stash_index = cbits_stash_index_of(cbit_carrier);
				CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
				if ( csh != nullptr ) {
					auto cbits = csh->_real_bits;
					return cbits;
				}
			}
			return 0;
		}


		// T BITS

		uint32_t fetch_real_tbits(uint32_t tbit_carrier) {
			if ( is_base_tbits(tbit_carrier) ) {
				return tbit_carrier;
			} else {
				auto stash_index = tbits_stash_index_of(tbit_carrier);
				TBIT_stash_el *tse = _tbit_stash.stash_el_reference(stash_index);
				if ( tse != nullptr ) {
					auto tbits = tse->_real_bits;
					return tbits;
				}
			}
		}


		void store_real_tbits(uint32_t tbits,uint8_t thrd) {
			_tbits_temporary_store[thrd] = tbits;
		}



		/**
		 * get_real_base_cbits
		*/

		uint32_t get_real_base_cbits(hh_element *base_probe) {
			atomic<uint32_t> *a_cbits = (atomic<uint32_t> *)(base_probe->c.bits);
			auto cbits = a_cbits->load(std::memory_order_acquire);
			if ( is_base_in_ops(cbits) ) {
				cbits = fetch_real_cbits(cbits);
			}
			if ( is_base_noop(cbits) ) {
				return cbits;
			}
			return 0;
		}


		/**
		 * get_real_base_tbits
		*/

		uint32_t get_real_base_tbits(hh_element *base_probe) {
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(base_probe->tv.taken);
			auto tbits = a_tbits->load(std::memory_order_acquire);
			if ( !(is_base_tbits(tbits)) ) {
				tbits = fetch_real_cbits(tbits);
			}
			return tbits;
		}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void stash_key_value(uint32_t key,uint32_t value,uint16_t thread_id) {
			_key_value_temporary_store[thread_id].first = key;
			_key_value_temporary_store[thread_id].second = value;
		}
		

		void erase_stashed_key_value(uint16_t thread_id) {
			_key_value_temporary_store[thread_id].first = UINT32_MAX;
			_key_value_temporary_store[thread_id].second = 0;
		}

		uint32_t check_key_value(uint32_t key,uint16_t thread_id) {
			if ( _key_value_temporary_store[thread_id].first == key ) {
				return _key_value_temporary_store[thread_id].second;
			}
			return UINT32_MAX;
		}

		bool _check_key_value_stash(uint16_t thread_id,uint32_t key,uint32_t &value) {
			auto v1 = check_key_value(key,thread_id);
			if ( v1 == UINT32_MAX ) return false;
			value = v1;
			return true;
		}
		
		uint32_t search_key_value(uint32_t key) {
			for ( uint16_t thread_id = 0; thread_id < _num_threads; thread_id++ ) {
				if ( _key_value_temporary_store[thread_id].first == key ) {
					return _key_value_temporary_store[thread_id].second;
				}
			}
			return UINT32_MAX;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


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


	public: 


		/**
		 * load_real_cbits
		 * 
		*/

		uint32_t load_real_cbits(hh_element *base) {
			atomic<uint32_t> *a_cbits_base = (atomic<uint32_t> *)(&(base->c.bits));
			auto cbits = a_cbits_base->load(std::memory_order_acquire);
			if ( !(is_base_noop(cbits)) ) {
				cbits = fetch_real_cbits(cbits);
			}
			return cbits;
		}


		atomic<uint32_t> *load_cbits(hh_element *base,uint32_t &cbits,uint32_t &cbits_op) {
			atomic<uint32_t> *a_cbits_base = (atomic<uint32_t> *)(&(base->c.bits));
			auto cbits = a_cbits_base->load(std::memory_order_acquire);
			if ( !(is_base_noop(cbits)) &&  (cbits != 0) ) {
				cbits_op = cbits;
				cbits = fetch_real_cbits(cbits);
			}
			return a_cbits_base;
		}



		atomic<uint32_t> *load_cbits(atomic<uint32_t> *a_cbits_base,uint32_t &cbits,uint32_t &cbits_op) {
			auto cbits = a_cbits_base->load(std::memory_order_acquire);
			if ( !(is_base_noop(cbits)) &&  (cbits != 0) ) {
				cbits_op = cbits;
				cbits = fetch_real_cbits(cbits);
			}
			return a_cbits_base;
		}
		//stash_el_reference


		/**
		 * _get_member_bits_slice_info
		 * 
		 * 	<-- called by prepare_for_add_key_value_known_refs -- which is application facing.
		*/
		atomic<uint32_t> *_get_member_bits_slice_info(uint32_t h_bucket,uint8_t &which_table,uint32_t &c_bits,uint32_t &c_bits_op,uint32_t &c_bits_base_ops,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,CBIT_stash_holder *cbit_stashes[4]) {
			//
			if ( h_bucket >= _max_n ) {
				h_bucket -= _max_n;  // let it be circular...
			}
			//
			c_bits_op = 0;
			c_bits = 0;
			//
			uint8_t count0{0};
			uint8_t count1{0};
			//
			hh_element *buffer0		= _region_HV_0;
			hh_element *end_buffer0	= _region_HV_0_end;
			//
			hh_element *el_0 = buffer0 + h_bucket;
			atomic<uint32_t> *a_cbits0 = (atomic<uint32_t> *)(&(el_0->c.bits));
			auto c0 = a_cbits0->load(std::memory_order_acquire);
			uint32_t c0_ops = 0;
			uint8_t backref0 = 0;
			uint32_t c0_op_base = 0;
			hh_element *base0 = nullptr;
			//
			if ( c0 != 0 ) {  // no elements
				if ( is_base_noop(c0) ) {
					count0 = popcount(c0);
				} else {
					if ( is_base_in_ops(c0) ) {
						auto real_bits = fetch_real_cbits(c0);
						c0_ops = c0;
						c0 = real_bits;
						count0 = popcount(real_bits);
					} else if ( is_member_bucket(c0) ) {
						c0_ops = c0;
						base0 = cbits_base_from_backref(c0,backref0,el_0,buffer0,end_buffer0);
						load_cbits(base0,c0,c0_op_base);
						count0 = 1;   // indicates the member element ... some work will be done to reinsert the previously stored member
					}
				}
			}
			//
			hh_element *buffer1		= _region_HV_1;
			hh_element *end_buffer1	= _region_HV_1_end;
			//
			hh_element *el_1 = buffer1 + h_bucket;
			atomic<uint32_t> *a_cbits1 = (atomic<uint32_t> *)(&(el_1->c.bits));
			auto c1 = a_cbits1->load(std::memory_order_acquire);
			uint32_t c1_ops = 0;
			uint8_t backref1 = 0;
			uint32_t c1_op_base = 0;
			hh_element *base1 = nullptr;
			//
			if ( c1 != 0 ) {  // no elements
				if ( is_base_noop(c1) ) {
					count1 = popcount(c1);
				} else {
					if ( is_base_in_ops(c1) ) {
						auto real_bits = fetch_real_cbits(c1);
						c1_ops = c1;
						c1 = real_bits;
						count1 = popcount(real_bits);
					} else if ( is_member_bucket(c0) ) {
						c1_ops = c1;
						base1 = cbits_base_from_backref(c1,backref1,el_1,buffer1,end_buffer1);
						load_cbits(base1,c1,c1_op_base);
						count1 = 1;   // indicates the member element ... some work will be done to reinsert the previously stored member
					}
				}
			}
			//
			auto selector = _hlpr_select_insert_buffer(count0,count1);
			if ( selector == 0xFF ) return nullptr;
			//
			atomic<uint32_t> *a_cbits = nullptr;
			hh_element *base = nullptr;
			uint8_t backref = 0;
			//
			which_table = selector;
			//
			if ( selector == 0 ) {				// select data structures
				*bucket_ref = el_0;
				*buffer_ref = buffer0;
				*end_buffer_ref = end_buffer0;
				c_bits = c0;		// real cbits
				c_bits_op = c0_ops;
				backref = backref0;
				c_bits_base_ops = c0_op_base;
				//
				base = base0;
				a_cbits = a_cbits0;
			} else {
				*bucket_ref = el_1;
				*buffer_ref = buffer1;
				*end_buffer_ref = end_buffer1;
				c_bits = c1;		// real cbits
				c_bits_op = c1_ops;
				backref = backref1;
				c_bits_base_ops = c1_op_base;
				//
				base = base1;
				a_cbits = a_cbits1;
			}
			//

			stash_cbits(a_cbits,base,c_bits,c_bits_op,c_bits_base_ops,cbit_stashes,*buffer_ref,*end_buffer_ref);
			//
			return a_cbits;
			
		}



		/**
		 * stash_base_bits
		 * 
		 * 
		 * c_bits -- is always the real cbits of the base (where the bucket is base itself or the bucket is a member of the base)
		 * 
		*/

		CBIT_stash_holder 		*stash_base_bits(atomic<uint32_t> *a_base_bits,uint32_t &c_bits,uint32_t &c_bits_op) {
			uint8_t stash_index = _cbit_stash.pop_one_wait_free();
			auto c_bits_op_update = cbit_stash_index_stamp(c_bits_op,stash_index);

			while ( !a_base_bits->compare_exchange_weak(c_bits_op,c_bits_op_update,std::memory_order_acq_rel) ) {
				auto already_stashed = cbits_stash_index_of(c_bits_op);
				if ( already_stashed != stash_index ) {
					// maybe update a stash counter
					CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(already_stashed);
					csh->_updating++;
					_cbit_stash.return_one_to_free(stash_index);
					return csh;
				}
			}

			CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
			csh->_real_bits = c_bits;
			csh->_updating = 1; // this should be the first one.. 
			csh->_is_base_mem = CBIT_BASE;
			c_bits_op = c_bits_op_update;
			return csh;
		}



		CBIT_stash_holder 	*stash_member_bits(atomic<uint32_t> *a_mem_bits,uint32_t c_bits,uint32_t &c_bits_op) {
			uint8_t stash_index = _cbit_stash.pop_one_wait_free();
			auto c_bits_op_update = cbit_member_stash_index_stamp(c_bits_op,stash_index);

			while ( !a_mem_bits->compare_exchange_weak(c_bits_op,c_bits_op_update,std::memory_order_acq_rel) ) {
				auto already_stashed = cbits_stash_index_of(c_bits_op);
				if ( already_stashed != stash_index ) {
					// maybe update a stash counter
					CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(already_stashed);
					csh->_updating++;
					_cbit_stash.return_one_to_free(stash_index);
					return csh;
				}
			}

			CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
			csh->_real_bits = c_bits;
			csh->stored._csm._member_bits = c_bits_op;
			csh->_updating = 1; // this should be the first one.. 
			csh->_is_base_mem = CBIT_MEMBER;
			c_bits_op = c_bits_op_update;
			return csh;
		}


		/**
		 * stash_cbits
		 * 
		 * 
		 * c_bits -- is always the real cbits of the base (where the bucket is base itself or the bucket is a member of the base)
		 * 
		*/

		void stash_cbits(atomic<uint32_t> *a_cbits, hh_element *base, uint32_t &c_bits, uint32_t &c_bits_op, uint32_t &c_bits_base_ops, CBIT_stash_holder *cbit_stashes[4],hh_element *buffer,hh_element *end) {
			//
			if ( base == nullptr ) {  // not a member  i.e. this is a base

				if ( c_bits_op == 0 ) {  // means not yet stashed, zero or membership bits
					CBIT_stash_holder *csh = stash_base_bits(a_cbits, c_bits, c_bits_op);  // this puts bits (cbits) into the otherwise empty cell
					cbit_stashes[0] = csh;
				} else {
					auto already_stashed = cbits_stash_index_of(c_bits_op);
					if ( already_stashed != 0 ) {
						CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(already_stashed);
						csh->_updating++;
						cbit_stashes[0] = csh;
					}
				}

			} else { //  bucket is a member and base ops have value
				//
				uint16_t stash_index = cbits_member_stash_index_of(c_bits_op);  // make sure the member bucket is marked
				if ( stash_index != 0 ) {
					CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
					cbit_stashes[0] = csh;
					csh->_updating++;			// following this one; if not a base yet, it will be
					return;  // already stashed means subject to edit or usurpation
				}

				CBIT_stash_holder *csh_mem = stash_member_bits(a_cbits,c_bits,c_bits_op);
				cbit_stashes[0] = csh_mem;

				// base is not nullptr and is the backref from the bucket
				atomic<uint32_t> *base_cbits = (atomic<uint32_t> *)(&(base->c.bits));
				if  ( c_bits_base_ops == 0 ) {
					CBIT_stash_holder *csh = stash_base_bits(base_cbits, c_bits, c_bits_base_ops);  // this puts bits (cbits) into the otherwise empty cell
					cbit_stashes[1] = csh;
				} else {
					auto already_stashed = cbits_stash_index_of(c_bits_base_ops);
					CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(already_stashed);
					csh->_updating++;
					cbit_stashes[1] = csh;
				}
				//
			}

		}


		void _unstash_base_cbits(atomic<uint32_t> *a_cbits,CBIT_stash_holder *csh) {
			auto current_cbits = a_cbits->load(std::memory_order_acquire);
			auto cbits = csh->_real_bits;
			cbits &= ~(csh->stored._cse._remove_update);
			cbits |= csh->stored._cse._remove_update;
			while ( !(a_cbits->compare_exchange_weak(current_cbits,cbits,std::memory_order_release)) ) {
				if ( csh->_updating > 1 ) {
					csh->_updating--;
					if ( csh->_updating == 0 ) {
						_cbit_stash.return_one_to_free(csh->_index);
					}
					return;
				}
			}
			csh->_updating--;
			if ( csh->_updating == 0 ) {
				_cbit_stash.return_one_to_free(csh->_index);
			}
		}


		void _unstash_member_cbits(atomic<uint32_t> *a_cbits,uint32_t mem_cbits,CBIT_stash_holder *csh) {
			csh->_updating--;
			if ( csh->_updating == 0 ) {
				auto updating_cbits = 0;
				if ( mem_cbits & USURPED_CBIT_SET ) {  // restore to real roots
					updating_cbits = csh->_real_bits;
				} else {
					updating_cbits = csh->stored._csm._member_bits;
				}
				while ( !(a_cbits->compare_exchange_weak(mem_cbits,updating_cbits,std::memory_order_release)) && (mem_cbits != updating_cbits) );
			}

		}



		void unstash_base_cbits(hh_element *base,CBIT_stash_holder *csh_base,hh_element *buffer,hh_element *end) {
			atomic<uint32_t> *a_cbits = (atomic<uint32_t> *)(&(base->c.bits));
			auto removals = csh_base->stored._cse._remove_update.load(std::memory_order_acquire);
			if ( removals != 0 ) {
				auto c = removals;
				while ( c ) {
					hh_element *next = base;
					uint8_t offset = get_b_offset_update(c);
					next += offset;
					next = el_check_end_wrap(next,buffer,end);
					atomic<uint32_t> *a_nxt_cbits = (atomic<uint32_t> *)(&(next->c.bits));
					auto n_cbits = a_nxt_cbits->load(std::memory_order_acquire);
					auto mem_stash_index = cbits_member_stash_index_of(n_cbits);
					if ( mem_stash_index ) {
						CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(mem_stash_index);
						_unstash_member_cbits(a_nxt_cbits,n_cbits,csh);
					}
				}
			}
			_unstash_base_cbits(a_cbits,csh_base);
		}

		// ---- 



		TBIT_stash_el *stash_taken_spots(hh_element *nxt_base) {
			return nullptr;
		}



		TBIT_stash_el *stash_taken_spots(hh_element *nxt_base,uint32_t &real_tbits) {
			return nullptr;
		}

		void _unstash_base_tbits(atomic<uint32_t> *a_tbits,TBIT_stash_el *tse) {
		}



		atomic<uint32_t> *load_tbits(atomic<uint32_t> *a_tbits_base,uint32_t &tbits,uint32_t &tbits_op) {
			tbits_op = 0;
			auto tbits = a_tbits_base->load(std::memory_order_acquire);
			if ( !(is_base_noop(tbits)) &&  (tbits != 0) ) {
				tbits_op = tbits_op;
				tbits = fetch_real_tbits(tbits);
			}
			return a_tbits_base;
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 

		CBIT_stash_holder *from_member_to_base(uint32_t &cbits_op,CBIT_stash_holder *csm) {
			// Can change the nature of what it is, but leaves the stash key, value for readers until it is unstashed by the original base
			auto stashed_member_cbits = csm->stored._csm._member_bits.load(std::memory_order_acquire);
			csm->stored._cse._add_update = 0x1;
			csm->stored._cse._remove_update = 0;
			csm->_is_base_mem = CBIT_BASE;
			return csm;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		uint32_t stash_updating_count(uint32_t cbits_op) {
			auto stash_index = cbits_stash_index_of(cbits_op);
			CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
			return csh->_updating;
		}

		uint32_t stash_member_updating_count(uint32_t cbits_op) {
			auto stash_index = cbits_member_stash_index_of(cbits_op);
			CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
			return csh->_updating;
		}
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		bool stashed_empty_bucket(uint32_t cbits,uint32_t cbits_op) {
			bool clear_cbits = (cbits == 0);
			bool ops_clear = ((cbits_op & BITS_STASH_INDEX_CLEAR_MASK) == 0);
			return clear_cbits & ops_clear;
		}

		bool stashed_base_bucket(uint32_t cbits,uint32_t cbits_op) {
			bool is_real_cbits = ((cbits & 0x1) != 0);
			bool ops_clear = ((cbits_op & BITS_STASH_INDEX_MASK) != 0);  // there may be ops
			return ( is_real_cbits & ops_clear );
		}

		void stash_key_value(uint32_t key, uint32_t value, CBIT_stash_holder *csh) {
			csh->_key = key;
			csh->_value = value;
		}


		void stash_key_value(uint32_t key, uint32_t value, CBIT_stash_holder *csh) {
			csh->_key = key;
			csh->_value = value;
		}



		void stash_bits_clear(uint32_t clear_bits, CBIT_stash_holder *csh) {
			csh->stored._cse._remove_update |= clear_bits;
		}


		void stash_real_cbits(uint32_t add_bits, CBIT_stash_holder *csh) {
			csh->stored._cse._add_update |= add_bits;
		}

		

		void stash_tbits_clear(uint32_t clear_bits, TBIT_stash_el *tse) {
			tse->_remove_update |= clear_bits;
		}


		void stash_real_tbits(uint32_t add_bits, TBIT_stash_el *tse) {
			tse->_add_update |= add_bits;
		}

		



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


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
		uint8_t _hlpr_select_insert_buffer(uint8_t count_0,uint8_t count_1) {
			uint8_t which_table = 0;
			if ( (count_0 >= MAX_BUCKET_COUNT) && (count_1 >= MAX_BUCKET_COUNT) ) {
				return UINT8_MAX;
			}
			if ( count_0 >= MAX_BUCKET_COUNT ) {
				which_table = 1;
			}
			if ( count_1 >= MAX_BUCKET_COUNT ) {
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


		// cbits_stash_update(cbits_op,update_bits,ADD_BITS_IMMEDIATE)
		void cbits_stash_update(uint32_t cbits_op,uint32_t update_bits,op_type op) {
			uint8_t stash_index = cbits_stash_index_of(cbits_op);  // make sure the member bucket is marked
			CBIT_stash_holder *cse = _cbit_stash.stash_el_reference(stash_index);
			cbits_stash_update(cse,cbits_op,update_bits,op);
		}

		void cbits_stash_update(CBIT_stash_holder *cse,uint32_t cbits_op,uint32_t update_bits,op_type op) {
			uint8_t stash_index = cbits_stash_index_of(cbits_op);  // make sure the member bucket is marked
			CBIT_stash_holder *cse = _cbit_stash.stash_el_reference(stash_index);
			switch ( op ) {
				case ADD_BITS: {
					cse->stored._cse._add_update |= update_bits;
					break;
				}
				case ADD_BITS_IMMEDIATE: {
					cse->stored._cse._add_update |= update_bits;
					cse->_real_bits |= update_bits;
					break;
				}
				case REMOVE_BITS: {
					cse->stored._cse._remove_update |= update_bits;
					break;
				}
				case REMOVE_BITS_IMMEDIATE: {
					cse->stored._cse._remove_update |= update_bits;
					cse->_real_bits &= ~(update_bits);
					break;
				}
				case MOVE_BITS: {
					break;
				}
				default: {
					break;
				}
			}
		}


	public:


		/**
		 * DATA STRUCTURES:
		 */  

		uint32_t						_num_threads;
		uint32_t						_max_count;
		uint32_t						_max_n;   // max_count/2 ... the size of a slice.
		//
		hh_element		 				*_region_HV_0;
		hh_element		 				*_region_HV_1;
		hh_element		 				*_region_HV_0_end;
		hh_element		 				*_region_HV_1_end;

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

		atomic_flag		 				*_rand_gen_thread_waiting_spinner;
		atomic_flag		 				*_random_share_lock;

		atomic<uint32_t>				*_random_gen_region;


		FreeOperatorStashStack<CBIT_stash_holder,MAX_THREADS,EXPECTED_THREAD_REENTRIES>		_cbit_stash;
		FreeOperatorStashStack<TBIT_stash_el,MAX_THREADS,EXPECTED_THREAD_REENTRIES>			_tbit_stash;


};





/**
 * CLASS: HH_thread_manager
 * 
*/


class HH_thread_manager {

	public:

		HH_thread_manager(uint32_t num_threads){
			_num_threads = num_threads;
		}
		virtual ~HH_thread_manager() {}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


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
		 * 
		 * 
		*/
		bool is_restore_queue_empty(uint8_t thread_id, uint8_t which_table) {
			proc_descr *p = _process_table + thread_id;
			bool is_empty = p->_process_queue[which_table].empty();
#ifndef __APPLE__

#endif
			return is_empty;
		}




		/**
		 * enqueue_restore
		 * 
		 * hh_adder_states update_type
		 * atomic<uint32_t> *control_bits
		 * uint32_t cbits
		 * uint32_t cbits_op_base
		 * hh_element *bucket
		 * uint32_t h_start
		 * uint64_t loaded_value
		 * uint8_t which_table
		 * uint8_t thread_id
		 * hh_element *buffer
		 * hh_element *end
		 * uint8_t &require_queue
		*/
		void enqueue_restore(hh_adder_states update_type, atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t cbits_op, uint32_t cbits_op_base, hh_element *hash_ref, uint32_t h_bucket, uint32_t el_key, uint32_t value, uint8_t which_table, hh_element *buffer, hh_element *end,uint8_t require_queue) {
			q_entry get_entry;
			//
			get_entry.update_type = update_type;
			get_entry.control_bits = control_bits;
			get_entry.cbits = cbits;
			get_entry.cbits_op = cbits_op;
			get_entry.cbits_op_base = cbits_op_base;
			get_entry.hash_ref = hash_ref;
			get_entry.h_bucket = h_bucket;
			get_entry.el_key = el_key;
			get_entry.value = value;
			get_entry.which_table = which_table;
			get_entry.buffer = buffer;
			get_entry.end = end;
			//
			proc_descr *p = _process_table + require_queue;
			//
			p->_process_queue[which_table].push(get_entry); // by ref
		}



		/**
		 * dequeue_restore
		*/
		void dequeue_restore(hh_adder_states &update_type, atomic<uint32_t> **control_bits_ref, uint32_t &cbits, uint32_t &cbits_op, uint32_t &cbits_op_base, hh_element **hash_ref_ref, uint32_t &h_bucket, uint32_t &el_key, uint32_t &value, uint8_t &which_table, uint8_t assigned_thread_id, hh_element **buffer_ref, hh_element **end_ref) {
			//
			q_entry get_entry;
			//
			proc_descr *p = _process_table + assigned_thread_id;
			//
			p->_process_queue[which_table].pop(get_entry); // by ref
			//
			update_type = get_entry.update_type;
			*control_bits_ref = get_entry.control_bits;
			cbits = get_entry.cbits;
			cbits_op = get_entry.cbits_op;
			cbits_op_base = get_entry.cbits_op_base;
			//
			hh_element *hash_ref = get_entry.hash_ref;
			h_bucket = get_entry.h_bucket;
			el_key = get_entry.el_key;
			value = get_entry.value;
			which_table = get_entry.which_table;
			hh_element *buffer = get_entry.buffer;
			hh_element *end = get_entry.end;
			//
			*hash_ref_ref = hash_ref;
			*buffer_ref = buffer;
			*end_ref = end;
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
			if ( _round_robbin_proc_table_threads >= _num_threads ) _round_robbin_proc_table_threads = 1;
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
		bool is_cropping_queue_empty(uint8_t thread_id,uint8_t which_table) {
			proc_descr *p = _process_table + thread_id;
			bool is_empty = p->_to_cropping[which_table].empty();
#ifndef __APPLE__

#endif
			return is_empty;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * wakeup_value_restore
		*/

		void wakeup_value_restore(hh_adder_states update_type, atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t cbits_op, uint32_t cbits_op_base, hh_element *bucket, uint32_t h_start, uint32_t el_key, uint32_t value, uint8_t which_table, hh_element *buffer, hh_element *end, CBIT_stash_holder &csh) {
			// this queue is jus between the calling thread and the service thread belonging to just this process..
			// When the thread works it may content with other processes for the hash buckets on occassion.
			auto service_thread = csh._service_thread;
			if ( service_thread == 0 ) {
				_round_robbin_proc_table_threads++;
				if ( _round_robbin_proc_table_threads >= _num_threads ) _round_robbin_proc_table_threads = 1;
				service_thread = csh._service_thread = _round_robbin_proc_table_threads;
			}
 
			uint8_t require_queue = service_thread;
			enqueue_restore(update_type, control_bits, cbits, cbits_op, cbits_op_base, bucket, h_start, el_key, value, which_table, buffer, end, require_queue);

			wake_up_one_restore();
		}

		/**
		 * submit_for_cropping
		 * 
		 * 
		 * cbits - the membership map (not the operational version)
		*/

		void submit_for_cropping(hh_element *base,uint32_t cbits,hh_element *buffer,hh_element *end,uint8_t which_table) {
			enqueue_cropping(base,cbits,buffer,end,which_table);
			wake_up_one_cropping();
		}


	public: 

		// threads ...
		proc_descr						*_process_table;						
		proc_descr						*_end_procs;

		uint8_t							_round_robbin_proc_table_threads{1};
		uint32_t						_num_threads;
		//
		atomic_flag						_sleeping_reclaimer;
		atomic_flag						_sleeping_cropper;


};




/**
 * CLASS: HH_map_structure
 * 
 * Initialization of shared memory sections... 
 * 
*/


template<const uint32_t NEIGHBORHOOD = 32>
class HH_map_structure : public HMap_interface, public HH_thread_manager, public SliceSelector {

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
			initialize_randomness();
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

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



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
		 * clear
		*/
		void clear(void) {
			if ( _initializer ) {
				uint8_t sz = sizeof(HHash);
				uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
				this->setup_region(_initializer,header_size,_max_count,_num_threads);
			}
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



	public:


		/**
		 * DATA STRUCTURES:
		 */  

		// ---- ---- ---- ---- ---- ---- ----
		//
		bool							_status;
		const char 						*_reason;
		//
		bool							_initializer;
		//
		uint8_t		 					*_region;
		uint8_t		 					*_endof_region;
		//
		HHash							*_T0;
		HHash							*_T1;
		//

};







/**
 * CLASS: HH_map_atomic_no_wait
 * 
*/


template<const uint32_t NEIGHBORHOOD = 32>
class HH_map_atomic_no_wait : public HH_map_structure<NEIGHBORHOOD>, public HH_thread_manager {
	//
	public:

		// LRU_cache -- constructor
		HH_map_atomic_no_wait(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) :
		 								HH_map_structure<NEIGHBORHOOD>(region, seg_sz, max_element_count, num_threads, am_initializer), HH_thread_manager(num_threads) {
		}

		virtual ~HH_map_atomic_no_wait() {
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * empty_bucket
		 * 
		*/

		inline bool empty_bucket(atomic<uint64_t> *a_c,hh_element *base,uint32_t &cbits,uint32_t &op_cbits,uint32_t &root_ky) {
			auto  cbits_n_key = a_c->load(std::memory_order_acquire);
			cbits = (uint32_t)(cbits_n_key & UINT32_MAX);
			op_cbits = 0;
			root_ky = (uint32_t)((cbits_n_key >> HALF) & UINT32_MAX);
			if ( is_empty_bucket(cbits) ) return true;  // no members and no back ref either (while this test for backref may be uncommon)
			if ( !(is_base_noop(cbits)) && (is_member_in_mobile_predelete(cbits) || is_deleted(cbits)) ) return true;
			if ( is_base_in_ops(cbits) ) {
				op_cbits = cbits;
				cbits = fetch_real_cbits(cbits);
			}
			return false;
		}

		inline bool empty_bucket(atomic<uint64_t> *a_c,hh_element *base,uint32_t &cbits,uint32_t &op_cbits) {
			auto  cbits_n_key = a_c->load(std::memory_order_acquire);
			cbits = (uint32_t)(cbits_n_key & UINT32_MAX);
			op_cbits = 0;
			if ( is_empty_bucket(cbits) ) return true;  // no members and no back ref either (while this test for backref may be uncommon)
			if ( !(is_base_noop(cbits)) && (is_member_in_mobile_predelete(cbits) || is_deleted(cbits)) ) return true;
			if ( is_base_in_ops(cbits) ) {
				op_cbits = cbits;
				cbits = fetch_real_cbits(cbits);
			}
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


		bool is_base_bucket_cbits_ops(uint32_t cbits_ops) {
			return cbits_ops == 0;
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

		/**
		 * reader_sem_stash_tbits
		*/

		bool reader_sem_stash_tbits(atomic<uint32_t> *a_tbits, uint32_t tbits_stash, uint8_t thread_id) {
			uint32_t reader_bits = tbit_thread_stamp(0x2,thread_id);
			auto expected_tbits = tbits_stash;
			while ( !(a_tbits->compare_exchange_weak(expected_tbits,reader_bits,std::memory_order_acq_rel)) ) {
				if ( expected_tbits != tbits_stash ) {
					if ( expected_tbits == 0 ) return false;
					else {  // this is a reader semaphore setup by another thread
						if ( !(tbits_sem_at_max(reader_tbits)) ) {
							a_tbits->fetch_add(TBITS_READER_SEM_INCR,std::memory_order_acq_rel);
							return true;
						}
					}
				}
			}
			// 
			store_real_tbits(tbits_stash,thread_id);  // keep the real tbits in a safe place
			return true;
		}


		/**
		 * tbits_add_reader
		*/

		TBIT_stash_el *tbits_add_reader(atomic<uint32_t> *a_tbits, uint32_t &tbits, uint32_t &value) {
			//
/*
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
				a_tbits->fetch_add(TBITS_READER_SEM_INCR,std::memory_order_acq_rel);
			}
		}
	}
*/
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
				a_tbits->fetch_sub(TBITS_READER_SEM_INCR,std::memory_order_acq_rel);
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
		 * tbits_add_swappy_op
		*/

/*
	uint32_t b = a_tbits->load(std::memory_order_acquire);
	if ( b == 0 ) {  // tbits have been stashed
		b = wait_for_real_tbits(a_tbits,b);  // wait for this to be other than zero 
	}

	bool tbits_stashed = tbits_are_stashed(b);

	atomic<uint32_t> *a_real_tbits = a_tbits;

	if ( tbits_stashed ) {     // add swappy op
		uint32_t tbits_op = b;
		uint8_t alt_thread = tbits_thread_id_of(tbits_op); // contend to set tbis (or cooperatively) in the stash
		b = fetch_real_tbits(b);
		a_real_tbits = (atomic<uint32_t> *)(&(_tbits_temporary_store[alt_thread]));
	} else {
		a_real_tbits = stash_tbits(a_tbits,b,thread_id);   // add swappy op
	}
*/

		atomic<uint32_t> *tbits_add_swappy_op(hh_element *base,uint32_t cbits,uint32_t &tbits,uint8_t thread_id) {
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			return tbits_add_swappy_op(a_tbits,cbits,tbits,thread_id);
		}

		TBIT_stash_el *tbits_add_swappy_op(atomic<uint32_t> *a_tbits,uint32_t cbits,uint32_t &tbits,uint8_t thread_id) {


			uint8_t count_result = 0;
			auto tbits_update = swappy_count_incr(tbits, count_result);
			do {
				while ( tbits_update == tbits ) {  // this happens when the cbits are maxed out
					tick();
					tbits = a_tbits->load(std::memory_order_acquire);
					tbits_update = swappy_count_incr(tbits, count_result);
				}
				while ( !(a_tbits->compare_exchange_weak(tbits,tbits_update,std::memory_order_acq_rel)) ) {
					tbits_update = swappy_count_incr(tbits, count_result);
					if ( (tbits & TBIT_SHIFTED_SWPY_COUNT_MAX) == TBIT_SHIFTED_SWPY_COUNT_MAX  ) break;  // another thread maxed it out -- back to waiting.
				}
			} while ( count_result == TBIT_SWPY_COUNT_MAX );



			return a_real_tbits;
		}


		/**
		 * tbits_remove_swappy_op
		*/
		void tbits_remove_swappy_op(atomic<uint32_t> *a_tbits) {
			uint8_t count_result = 0;
			auto tbits = a_tbits->load(std::memory_order_acquire);
			auto tbits_update = swappy_count_decr(tbits, count_result);
			do {
				while ( !(control_bits->compare_exchange_weak(tbits,tbits_update,std::memory_order_acq_rel)) ) {
					tbits_update = swappy_count_decr(tbits, count_result);
				}
			} while ( count_result == Q_COUNTER_MAX );
		}


		/**
		 * wait_tbits_exclude_swappy_op
		*/

		void wait_tbits_exclude_swappy_op(atomic<uint32_t> *a_tbits) {
			uint8_t count_result = 0;
			auto tbits = a_tbits->load(std::memory_order_acquire);
			while ( !(tbits & 0x1) && !(tbits_sem_at_zero(tbits)) ) {
				tick();
				tbits = a_tbits->load(std::memory_order_acquire);
			}
		}




		/**
		 * a_clear_cbit
		*/

		void a_clear_cbit(hh_element *base,uint8_t offset) {
			// slice_unlock(min_probe)
			atomic<uint64_t> *c_aptr = (atomic<uint64_t> *)(base->c.bits);
			auto c_bits = c_aptr->load(std::memory_order_acquire);
			auto c_bits_prev = c_bits;
			auto update_c_bits = (c_bits & ~(0x1 << offset));
			while ( !(c_aptr->compare_exchange_weak(c_bits_prev,update_c_bits,std::memory_order_release)) ) { c_bits = c_bits_prev; update_c_bits = (c_bits & ~(0x1 << offset); }
		}


		/**
		 * a_add_cbit
		*/

		void a_add_cbit(hh_element *base,uint8_t offset) {
			atomic<uint64_t> *a_c_aptr = (atomic<uint64_t> *)(base->c.bits);
			auto c_bits = a_c_aptr->load(std::memory_order_acquire);
			auto c_bits_prev = c_bits;
			auto update_c_bits = (c_bits | (0x1 << (min_base_offset + offset_nxt_base)));
			while ( !(c_aptr->compare_exchange_weak(c_bits_prev,update_c_bits,std::memory_order_release)) ) { \
				c_bits = c_bits_prev; 
				update_c_bits = (c_bits | (0x1 << (min_base_offset + offset_nxt_base)))
			}
		}


		/**
		 * a_store_real_tbits_and
		 * 
		 * a_tbits -- the presupposition is that a_tbits is a reference to exactly the bucket tbits and is not a
		 * reference to the stash of the tbits. 
		*/

		void a_store_real_tbits_and(atomic<uint32_t> *a_tbits, uint32_t tbits_update) {
			auto current_tbits = a_tbits->load(std::memory_order_acquire);
			if ( current_tbits & 0x1 ) {
				a_tbits->fetch_and(tbits_update,std::memory_order_acq_rel);
			} else {
				uint8_t alt_thread = tbits_thread_id_of(current_tbits);
				atomic<uint32_t> *a_real_tbits = (atomic<uint32_t> *)(&(_tbits_temporary_store[alt_thread]));
				a_tbits->fetch_and(tbits_update,std::memory_order_acq_rel);
			}
		}


		/**
		 * a_tbit_store_reservation
		 * 
		 * The method first attempts to use the proposed single hole discovered in the original tbits 
		 * to fill a bit position. If there is contention, then the method tries to find another one.
		 * In the end, the method returns a tbit pattern in which a hole is eventually captured. 
		 * 
		 * On failing to capture a hole, the method returns all black. 
		 * 
		*/
		uint32_t a_tbit_store_reservation(atomic<uint32_t> *a_real_tbits, TBIT_stash_el *tse, uint32_t &original_tbits, uint8_t &hole, uint32_t &hbit) {
			//
			hbit = (1 << hole); 	// hbit is the claiming bit
			auto b = original_tbits | hbit;   // declare the position taken
			//
			while ( !(tse->_add_update.compare_exchange_weak(original_tbits,b,std::memory_order_acq_rel)) ) {
				// stored value is not expected ... another thread got the position first
				if ( (original_tbits & hbit) != 0 ) {
					hole = countr_one(b);   // try for another hole ... someone else got last attempt
					if ( hole >= sizeof(uint32_t) ) {   // the bucket is full already
						return UINT32_MAX;
					}
					hbit = (1 << hole);
				} // else this didn't affect the position of choice ... two threads two different positions
				b = original_tbits | hbit;  // either hbit changed or we are trying to set the same spot
			}
			//
			tse->_real_bits = b;
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
		void a_store_real_cbits(atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t op_cbits, uint32_t hbit, uint8_t thread_id) {
			while ( (op_cbits & DELETE_CBIT_SET) && !(op_cbits & 0x1) ) {  // just wait for the deleters to finish
				op_cbits = control_bits->load(std::memory_order_acquire); // clears a bit, might be next writing it
			}
			if ( op_cbits & 0x1 ) {  // somehow, some thread gave up owernship of the bucket, but the current thread is supposed to own the bucket
				control_bits->fetch_or(hbit,std::memory_order_release);
			} else {
				auto thrd = bits_thread_id_of(op_cbits);
				atomic<uint32_t> *a_stash_cbit = (atomic<uint32_t> *)(&(_cbits_temporary_store[thrd]));
				a_stash_cbit->fetch_or(hbit,std::memory_order_release);
			}
		}


		/**
		 * become_bucket_state_master
		 * 
		 * must preserve reader semaphore if it is in use
		 * 
		 * cbits and bits_op pararmeters reflect the last inspection of the bucket state.
		 * A small amount of time may intervene between the inspection and the attempt to change the state
		 * to be an ownership of control for the thread. 
		 * 
		 * cbits and bits_op are passed by reference so that if the state has been preempted, the caller
		 * may know and take another action after failing to become the master of the bucket.
		 * 
		 * 
		 * params: 
		 * 
		 * master_context -- informs the state transition allowing to lockout temporary reader states while the root is overwritten. 
		 * control_bits -- atomic access to the cbits... 
		 * 
		 * 
		 * 
		 * 	
	HH_FROM_EMPTY,			// from zero 
	HH_FROM_BASE,			// overwritten and old value queued  (wait during overwrite)
	HH_FROM_BASE_AND_WAIT,	// queued no overwrite (no wait)
	HH_USURP,			// specifically a member is being overwritten
	HH_USURP_BASE,		// secondary op during usurp (may not need to wait)
	HH_DELETE, 			// more generally members
	HH_DELETE_BASE,		// specifically rewiting the base or completely erasing it
	HH_ADDER_STATES

		*/

		void set_control_bit_state(atomic<uint32_t> *control_bits,uint32_t &cbits_op,uint32_t set_bit) {
			while ( !(cbits_op & set_bit) ) {
				auto cbits_op_expected = cbits_op;
				auto cbits_op_update = cbits_op | set_bit; // if winning this state, then tell things to be swappy aware
				while ( !(control_bits->compare_exchange_weak(cbits_op_expected,cbits_op_update,std::memory_order_acq_rel)) && !(cbits_op_expected & set_bit) ) {
					cbits_op_update = cbits_op_expected | set_bit;
				}
			}
		}

		bool become_bucket_state_master(hh_adder_states master_context,atomic<uint32_t> *control_bits,hh_element *bucket, uint32_t &cbits,  uint32_t &cbits_op, uint32_t &cbits_base_op) {
			switch ( master_context ) {
				case HH_FROM_EMPTY: {

					bool set_it = false;
					while ( !(cbits_op & ROOT_EDIT_CBIT_SET) ) {
						auto cbits_op_expected = cbits_op;
						auto cbits_op_update = cbits_op | ROOT_EDIT_CBIT_SET;
						//
						while ( !(set_it = control_bits->compare_exchange_weak(cbits_op_expected,cbits_op_update,std::memory_order_acq_rel)) && !(cbits_op_expected & ROOT_EDIT_CBIT_SET) ) {
							cbits_op_update = cbits_op_expected | ROOT_EDIT_CBIT_SET;
						}
					}

					return set_it;
				}

				case HH_FROM_BASE: {
					//
					bool set_it = false;

					while ( stash_updating_count(cbits_op) == 1 ) {  // first one in allowing zero, but unlikely case
						// try to get ahead of editors, which might delete as well 
						while ( !(cbits_op & ROOT_EDIT_CBIT_SET) ) {
							auto cbits_op_expected = cbits_op;
							auto cbits_op_update = cbits_op | ROOT_EDIT_CBIT_SET | EDITOR_CBIT_SET; // if winning this state, then tell things to be swappy aware
							// after the new value is inserted, the next phase will be a late arrival insertion of the old value..
							//
							while ( !(set_it = control_bits->compare_exchange_weak(cbits_op_expected,cbits_op_update,std::memory_order_acq_rel)) && !(cbits_op_expected & ROOT_EDIT_CBIT_SET) ) {
								auto ref_count = stash_updating_count(cbits_op);
								if ( (ref_count == 0) && (cbits_op_expected & 0x1) ) {  // an error condition, in which this is unstashed 
									CBIT_stash_holder *cbit_stashes[4];
									stash_cbits(control_bits,bucket,cbits,cbits_op,cbits_base_ops,cbit_stashes);
									break;
								} else {
									cbits_op_update = cbits_op_expected | ROOT_EDIT_CBIT_SET | EDITOR_CBIT_SET;
								}
							}
						}
						//
					}

					return set_it;
				}


				// The base of the cell holds the membership map in which this branch finds the cell.
				// The base will be altered... 
				// If successful in being the thread that will change the member, then take ownership of the base as 
				// a cell that will execute a partial deletion by removing the membership bit for the base bucket.
				// Taken bits will not change until the usurped member is reinserted at that base. 
				// Operations that hope to usurp but are too late to be the member cell editor, will have to try again.
				// Note that these late operations already passed the previous gaurd, but lost the race to the next line.
				// When those threads try again, they may encounter the usurped cell as a base. 

				case HH_USURP: {
					bool set_it = false;
					//
					while ( stash_member_updating_count(cbits_op) == 1 ) {  // first one in allowing zero, but unlikely case

						while ( !(cbits_op & IMMOBILE_CBIT_SET) ) {
							auto cbits_op_expected = cbits_op;
							auto cbits_op_update = cbits_op | IMMOBILE_CBIT_SET | USURPED_CBIT_SET; // if winning this state, then tell things to be swappy aware
							// after the new value is inserted, the next phase will be a late arrival insertion of the old value..
							//
							while ( !(set_it = control_bits->compare_exchange_weak(cbits_op_expected,cbits_op_update,std::memory_order_acq_rel)) && !(cbits_op_expected & IMMOBILE_CBIT_SET) && !(cbits_op_expected & USURPED_CBIT_SET) ) {
								auto ref_count = stash_member_updating_count(cbits_op);
								if ( (ref_count == 0) && (cbits_op_expected & 0x1) ) {  // an error condition, in which this is unstashed 
									CBIT_stash_holder *cbit_stashes[4];
									stash_cbits(control_bits,bucket,cbits,cbits_op,cbits_base_ops,cbit_stashes);
									break;
								} else {
									cbits_op_update = cbits_op_expected | IMMOBILE_CBIT_SET | USURPED_CBIT_SET;
								}
							}
						}
					}					
					//
					return set_it;
				}
			}

			//
			return false;
		}

		void clear_bucket_root_edit(uint64_t &ky_n_cbits) {
			ky_n_cbits &= ~((uint64_t)ROOT_EDIT_CBIT_SET);
		}

		void clear_bucket_root_edit(uint32_t cbits_op) {
			cbits_op &= ~((uint32_t)ROOT_EDIT_CBIT_SET);
		}



		// if become_bucket_state_master identifies this situation or wait_become_bucket_delete_master does so.
		uint32_t wait_if_base_update(atomic<uint64_t> *a_base_c_n_k,uint32_t &cbits,uint32_t &cbits_op) {
			auto cbits_n_ky = a_base_c_n_k->load();
			while ( cbits_n_ky & ROOT_EDIT_CBIT_SET ) {
				tick();
				cbits_n_ky = a_base_c_n_k->load();
			}
			auto base_ky = (uint32_t)((cbits_n_ky >> sizeof(uint32_t)) & UINT32_MAX);
			return base_ky;
		}



		/**
		 * release_bucket_state_master
		 * 
		 * 		should be called by the restore queue handler
		*/
		void release_bucket_state_master(atomic<uint32_t> *control_bits, uint32_t real_cbits) {
			control_bits->store(real_cbits); 
		}


		bool wait_become_bucket_delete_master(hh_adder_states master_context,atomic<uint64_t> *a_c_bits,uint8_t thread_id) {  // mark the whole bucket swappy mark for delete
			return true;
		}

/*
	HH_DELETE, 			// more generally members
	HH_DELETE_BASE,		// specifically rewiting the base or completely erasing it
*/

		void release_delete_master(hh_adder_states master_context,atomic<uint64_t> *a_c_bits,uint8_t thread_id) {

			if ( master_context == HH_DELETE_BASE ) {
				//
				// if the ROOT edit context has been set up by this thread then clear it
				auto ky_n_cbits = a_c_bits->load(std::memory_order_acquire);
				auto ky_n_cbits_update = ky_n_cbits;
				clear_bucket_root_edit(ky_n_cbits_update);
				while ( !(a_c_bits->compare_exchange_weak(ky_n_cbits,ky_n_cbits_update)) );
				//
			}

		}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * no_base_cbit_change -- used in checking to see if a second attempt at searching a bucket membership
		 * should be taken after failing to find a key.
		*/

		bool no_base_cbit_change(atomic<uint32_t> *base_bits,uint32_t original_bits) {
			auto current_bits = base_bits->load(std::memory_order_acquire);
			if ( current_bits & 0x1 ) {
				return ( current_bits == original_bits );
			} else {
				auto c = fetch_real_cbits(current_bits);
				return ( c == original_bits );
			}
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


		void wait_until_immobile(atomic<uint32_t> *a_cbits, uint32_t &cbits, uint32_t &ky) {
// IMMOBILE_CBIT_SET
		}

		uint64_t load_marked_as_immobile(atomic<uint64_t> *a_b_n_key) {   // a little more cautious
			return a_bits_n_ky->fetch_or((uint64_t)IMMOBILE_CBIT_SET,std::memory_order_acquire);
		}

		void remobilize(atomic<uint64_t> *a_bits_n_ky) {
			a_bits_n_ky->fetch_and(IMMOBILE_CBIT_RESET64,std::memory_order_acq_rel);
		}

					
		void remobilize(hh_element *storage_ref,uint32_t modifier = 0) {
			atomic<uint64_t> *a_bits_n_ky = (atomic<uint64_t> *)(&(storage_ref->c));
			remobilize(a_bits_n_ky);
		}



		/**
		 * attempt_preempt_delete
		 * 
		 * prior to a swap... cannot pass into a swappy bucket
		 * 
		*/

		bool attempt_preempt_delete(atomic<uint32_t> *control_bits, hh_element *bucket, uint32_t &cbits, uint32_t &cbits_op, uint32_t &cbits_op_base, CBIT_stash_holder cbit_stashes[4]) {
			//
			uint8_t backref = 0;
			uint32_t base_cbits;
			uint32_t base_cbits_op

			hh_element *base = cbits_base_from_backref(backref,bucket,buffer,end_buffer);
			atomic<uint32_t> *base_control_bits = load_cbits(base,base_cbits,base_cbits_op);
			//
			CBIT_stash_el *cse_base = cbit_stashes[0]._is_base ? cbit_stashes[0].rf._cse :  cbit_stashes[1].rf._cse;
			CBIT_stash_member *csm = !(cbit_stashes[0]._is_base) ? cbit_stashes[0].rf._csm :  cbit_stashes[1]._csm;

			// else 
			uint32_t tbits = 0;
			uint32_t value = 0;
			uint8_t stash_id = 0;
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			TBIT_stash_el *tse = tbits_add_reader(a_tbits, tbits, stash_id, value);
			if ( tse == nullptr ) { return false; }
			//
			// 
			auto state_capture = control_bits->fetch_and(~MOBILE_CBIT_SET,std::memory_order_acq_rel);
			if ( state_capture & DELETE_CBIT_SET ) {  // cropping will have to take a breath for the restore
				control_bits->store(state_capture,std::memory_order_acq_rel);
				tbits_remove_reader(a_tbits, tse);
				return false;
			}
			//
			cse_base->_remove_update.fetch_or(((uint32_t)0x1) << backref);
			unstash_base_cbits(base_control_bits,cse_base);
			//
			tbits_remove_reader(a_tbits, tse);
			return true;	// if a failed and the element targeted is now filled with a valid value, a preempt will take place.
		}


		/**
		 * _swappy_aware_search_ref
		 * 
		 * Assume that searches start after the base bucket. 
		 * The base bucket is a special case. Make this assumption for both versions of search.
		 * 
		 *	NOTES: By default the base bucket is immobile. It may be read immediately unless it is being deleted.
		 			The base may be owned by an editor. If it is, then `_swappy_aware_search_ref` will be invoked.
					The cbits of the base causing `_swappy_aware_search_ref` to be called will be indicating operation by
					and editor. And, the orginal cbits will be in the `_cbits_temporary_store` indexed by the thread
					carried in the current cbits.

				There are several strategies for delete at the base. 
				One: is to leave the root in editor operation. The reader may complete the swap ahead of the deleter.
				Two: is to leave the root cbits as the membership map. But, examine the key to see if it is max.
					If it is max, then perform the first swap, moving the next element into the root position.
		*/



		hh_element *_swappy_aware_search_ref(uint32_t el_key, hh_element *base, uint32_t c,uint32_t &value, uint32_t &time) {
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
				atomic<uint64_t> *a_tbits_n_val = (atomic<uint64_t> *)(&(next->tv));
				
				//
				// immobility is easiy marked on a member... 
				// on a base, the swappy search marks a thread marked controller while the original cbits are stashed.
				// so the immobility mark can still be used. But, the few editors operating in the bucket will not
				// be able to restore the original cbits until the immobility mark is erased. 

				auto c_n_ky = load_marked_as_immobile(a_bits_n_ky);
				uint32_t nxt_bits = (uint32_t)(c_n_ky & UINT32_MAX);
				auto chk_key = (c_n_ky >> sizeof(uint32_t)) & UINT32_MAX;
				if ( is_member_bucket(nxt_bits) ) {  // if it is not, then the bucket has been cleared and the membership map still wants an update
					//
					if ( !(is_cbits_deleted(nxt_bits) || is_cbits_in_mobile_predelete(nxt_bits)) ) {
						if ( el_key == chk_key ) {
							auto tbits_n_val = a_tbits_n_val->load(std::memory_order_acquire);
							value = (tbits_n_val >> HALF);
							time = (tbits_n_val & UINT32_MAX);
							return next;			// return marked as immobile.
						}
					}
					//
					if ( is_cbits_in_mobile_predelete(nxt_bits) && (chk_key == UINT32_MAX) ) {
						remobilize(a_bits_n_ky);
						// proceed if another operation is not at the moment doing a swap.
						// pass by reference -- values may be the bucket after swap by another thread
						//
						wait_until_immobile(a_bits_n_ky,nxt_bits,ky);
						c_n_ky = load_marked_as_immobile(a_bits_n_ky);
						//
						nxt_bits = (uint32_t)(c_n_ky & UINT32_MAX);
						chk_key = (c_n_ky >> sizeof(uint32_t)) & UINT32_MAX;
						//
						if ( !(is_cbits_deleted(nxt_bits)) ) {  // just in case this is not moving
							if ( el_key == chk_key ) {
								auto tbits_n_val = a_tbits_n_val->load(std::memory_order_acquire);
								value = (tbits_n_val >> HALF);
								time = (tbits_n_val & UINT32_MAX);
								return next;			// return marked as immobile.
							}
						}
					}
				}

				remobilize(a_bits_n_ky);
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
		 * original_bits -- are real cbits, i.e. non operational
		 * 
		*/

		hh_element *_editor_locked_search_ref(atomic<uint32_t> *a_base_bits, uint32_t original_bits, hh_element *base, uint32_t el_key, uint32_t &value) {
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
				if ( no_base_cbit_change(a_base_bits,original_bits) ) {
					break;
				} else {
					uint32_t cbits = 0;
					uint32_t op_cbits = 0;
					if ( empty_bucket(a_n_b_n_key,next,cbits,op_cbits) ) {
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


		/**
		 * _get_bucket_reference 
		 * 
		 * returns an immobile reference to the bucket, supplying value, key, and info cbits and tbits
		 * the base is always immobile
		*/

		hh_element *_get_bucket_reference(atomic<uint32_t> *bucket_bits, hh_element *base, uint32_t cbits, uint32_t cbits_op, uint32_t h_bucket, uint32_t el_key, hh_element *buffer, hh_element *end,uint8_t thread_id,uint32_t &value) {
			//
			// bucket_bits are the base of the bucket storing the state of cbits
			uint32_t tbits = 0;
			uint8_t stash_id = 0;
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			atomic<uint64_t> *a_base_c_n_k = (atomic<uint64_t> *)(&(base->c));
			//
			TBIT_stash_el *tse = tbits_add_reader(a_tbits,tbits,stash_id,value);  // lockout ops that swap elements
			// get the original bits even in a dynamic situation, and get the current representation
			// stash cbits allowing readers to pass through. Only swappy operations need to be locked out for non swappy reads
			uint32_t base_ky = wait_if_base_update(a_base_c_n_k,cbits,cbits_op); // a_base_c_n_k->load(std::memory_order_acquire);
			if ( is_empty_bucket(cbits) ) {  // base update finishes by deleting the only element in the bucket
				tbits_remove_reader(a_tbits,tse);					// last reader out restores tbits 
				return nullptr;
			}
			if ( base_ky == el_key ) {
				tbits_remove_reader(a_tbits,tse);					// last reader out restores tbits 
				return base;
			}
			//
			if ( !(is_swappy(cbits,tbits)) ) {  // last known cbits and tbits
											// swappy operation occurs in this bucket for now, make it exclusive to readers 
										  	// and don't make the readers do work on behalf of anyone
				hh_element *hhe = _editor_locked_search_ref(bucket_bits,cbits,base,el_key,value);
				tbits_remove_reader(a_tbits,tse);
				return hh;
			} else {
				tbits_add_swappy_op(base,cbits,tbits);
				tbits_remove_reader(a_tbits,tse);	// if here, then this is the only non swappy reader and blocks swappy readers
												// but, right now, thinsgs are wappy
				uint32_t time = 0;
				auto ref = _swappy_aware_search_ref(el_key, base, original_cbits,thread_id,value,time);
				//
				tbits_remove_swappy_op(a_tbits,tse);				// last reader out restores tbits 
				return ref;
			}
		}

		/**
		 * _first_level_bucket_ops
		 * 
		 * 
		 * cbits -- remain last known state of the bucket.
		 * 
		*/

		uint64_t _first_level_bucket_ops(atomic<uint32_t> *control_bits, uint32_t el_key, uint32_t h_bucket, uint32_t offset_value, uint8_t which_table, uint8_t thread_id, uint32_t cbits, uint32_t cbits_op,uint32_t cbits_base_op, hh_element *bucket, hh_element *buffer, hh_element *end_buffer,CBIT_stash_holder *cbit_stashes[4]) {
			//
			auto prev_bits = cbits;   // cbits are the current stored cbits at the bucket
			atomic<uint64_t> *a_ky_n_cbits = (uint64_t)(&(bucket->c));
			atomic<uint64_t> *a_val_n_tbits = (uint64_t)(&(bucket->tv));
			do {
				prev_bits = cbits;
				//
				if ( stashed_empty_bucket(cbits,cbits_op) ) {   // this happens if the bucket is completetly empty and not a member of any other bucket
					// if this fails to stay zero, then either another a hash collision has immediately taken place or another
					// bucket created a member.
					// Become master by being the first to declare operation
					if ( become_bucket_state_master(HH_FROM_EMPTY,control_bits,bucket,cbits,cbits_op,cbits_base_op) ) { // OWN THIS BUCKET
						//
						uint64_t ky_n_cbits = (((uint64_t)(el_key) << HALF) | cbits_op);  // cbits_op contains the stash id
						uint64_t val_n_tbits = (((uint64_t)(offset_value) << HALF) | 0x1 );
						//
						a_val_n_tbits->store(val_n_tbits,std::memory_order_release);	// indicate that bits have been stashed 
																						// since it is new, leave the map 0.
						a_ky_n_cbits->store(ky_n_cbits,std::memory_order_release);

						CBIT_stash_el *cse = cbit_stashes[0].rf._cse;
						cbits_stash_update(cse,cbits_op,0x1,ADD_BITS_IMMEDIATE);
						//
						// -- in this case, `wakeup_value_restore` fixes the taken bits map (value corresponds to the bucket master)
						wakeup_value_restore(HH_FROM_EMPTY, control_bits, 0x1, cbits_op, cbits_base_op, bucket, h_bucket, el_key, offset_value, which_table, buffer, end, cbit_stashes[0]);
						return;
					} else {
						control_bits = load_cbits(bucket,cbits,cbits_op);
					}
				} else if ( stashed_base_bucket(cbits,cbits_op) ) {  // NEW MEMBER: This is a base bucket (the new element should become a member)
					// become the master by verifying this is the first element in
					if ( become_bucket_state_master(HH_FROM_BASE,control_bits,bucket,cbits,cbits_op,cbits_base_op) ) { // OWN THIS BUCKET
						// results in reinsertion and search for a hole
						//
						uint64_t ky_n_cbits = a_ky_n_cbits->load(std::memory_order_acquire);
						// make it unreadable for a brief bit (although swappy has been set)  -- tbits are zero
						// this is at the bucket entry point (also precludes tbit updates) 
						// readers already going through can still read, but may not be able to reduce a semaphore... (may fore a wait)
						uint64_t val_n_tbits = a_val_n_tbits->exchange(((uint64_t)(UIN32_MAX) << HALF),std::memory_order_relaxed);
						//
						uint32_t tmp_value = val_n_tbits >> HALF;
						uint32_t tmp_key = ky_n_cbits >> HALF;
						//
						stash_key_value(tmp_key,tmp_value,cbit_stashes[0].rf._cse);  // stash the bits 
						//
						clear_bucket_root_edit(ky_n_cbits);  // when set, the root edit will be cleared 
						//
						// this is back in and the cbits still refer to the stash and mark the bucke swappy
						ky_n_cbits = (ky_n_cbits & UINT32_MAX) | ((uint64_t)el_key << HALF);
						val_n_tbits = (val_n_tbits & UINT32_MAX) | ((uint64_t)offset_value << HALF);
						//
						// not yet stored, so do that now
						a_val_n_tbits->store(val_n_tbits,std::memory_order_release);		// put the tbits back as they were
						a_ky_n_cbits->store(ky_n_cbits,std::memory_order_release);			// cbits back
						//
						wakeup_value_restore(HH_FROM_BASE_AND_WAIT, control_bits, cbits, cbits_op, cbits_base_op, bucket, h_bucket, tmp_key, tmp_value, which_table, buffer, end, cbit_stashes[0]);
						return;
					} else {  // THIS BUCKET IS ALREADY BEING EDITED -- let the restore thread shift the new value in
						// not being able to control the bucket this insertion is defered (new and old alike)
						// the control bits belong to the operation thread, but the queue increases
						set_control_bit_state(control_bits,cbits_op,EDITOR_CBIT_SET);
						if ( wait_on_max_queue_incr(control_bits,cbits,cbits_op,cbits_base_op) ) {
							wakeup_value_restore(HH_FROM_BASE_AND_WAIT, control_bits, cbits, cbits_op, cbits_op_base, bucket, h_bucket, el_key, offset_value, which_table, buffer, end, cbit_stashes[0]);
							return;
						}  // else, before being able to up the queue count, the bucket finished the base insertion (so, go again)
					}
				} else {	// MEMBER BUCKETS:
					// MEMBER BUCKETS: this bucket is a member and cbits carries a back_ref. (this should be usurped)
					// usurping gives priortiy to an entry whose hash target a bucket absolutely
					// For members, the cbits_op are the last known state of the member bucket, while the cbits
					// are the membership bits of the base bucket in which bucket is a member. 
					uint8_t op_thread_id = 0;
					if ( is_cbits_usurped(cbits_op) )  {  // attempting the same operation 
						// results in first time insertion and search for a hole (could alter time by a small amount to keep a sort)
						set_control_bit_state(control_bits,cbits_op,EDITOR_CBIT_SET);
						if ( wait_on_max_queue_incr(control_bits,cbits,cbits_op,cbits_base_op) ) {
							wakeup_value_restore(HH_FROM_BASE_AND_WAIT, control_bits, cbits, cbits_op, cbits_op_base, bucket, h_bucket, el_key, offset_value, which_table, buffer, end, cbit_stashes[0]);
							return;
						}
					}
					//
					if ( is_delete(cbits_op) ) {
						tick();
						continue;
					}

					//
					uint8_t backref = 0;			// get thise by ref
					uint32_t base_cbits = 0;
					uint32_t base_cbits_op = 0;  
					//
					//
					if ( is_cbits_in_mobile_predelete(cbits_op) && !(is_cbits_swappy(base_cbits_op)) ) {
						//
						if ( attempt_preempt_delete(control_bits,bucket,cbits,cbits_op,cbits_op_base,cbit_stashes) ) {   // delcare immobility to the cropper
							//
							cbit_stashes[0] = from_member_to_base(cbits_op,cbit_stashes[0]);
							//
							uint64_t ky_n_cbits = (((uint64_t)(el_key) << HALF) | cbits_op);
							uint64_t val_n_tbits = (((uint64_t)(offset_value) << HALF) | 0x1);
							//
							a_val_n_tbits->store(val_n_tbits,std::memory_order_release);	// indicate that bits have been stashed 
																							// since it is new, leave the map 0.
							a_ky_n_cbits->store(ky_n_cbits,std::memory_order_release);

							wakeup_value_restore(HH_FROM_EMPTY, control_bits, cbits, cbits_op, cbits_op_base, bucket, h_bucket, el_key, offset_value, which_table, buffer, end, cbit_stashes[0]);
							return;
						}
						//
					}


					// get sel base (header inline)
					hh_element *cells_base = cbits_base_from_backref(backref,bucket,buffer,end_buffer);
					atomic<uint32_t> *base_control_bits = load_cbits(cells_base,base_cbits,base_cbits_op);
					//

					// The base of the cell holds the membership map in which this branch finds the cell.
					// The base will be altered... 
					// If successful in being the thread that will change the member, then take ownership of the base as 
					// a cell that will execute a partial deletion by removing the membership bit for the base bucket.
					// Taken bits will not change until the usurped member is reinserted at that base. 
					// Operations that hope to usurp but are too late to be the member cell editor, will have to try again.
					// Note that these late operations already passed the previous gaurd, but lost the race to the next line.
					// When those threads try again, they may encounter the usurped cell as a base. 
					if ( become_bucket_state_master(HH_USURP,control_bits,bucket,cbits,cbits_op,cbits_op_base) ) { // OWN THIS BUCKET
						//
						// become master of the cell being usurped.
						// Does this have to lock out other base inserters? Not really, the stash holder will 
						// add a delete bit to the stashed `remove` field. The membership bit will be dropped when 
						// the cbits are unstashed. It may inform readers that they will have to look into stash 
						// of the membership stash holder. (if they check the `remove` bits of the stash for the base,
						// they can identify which buckets stash their values prior to insertion.)

						// At the moment it seems that stashing is the method by which this ownership takes affect within the usurp case
						// while ( !become_bucket_state_master(HH_USURP_BASE,base_control_bits,cells_base,base_cbits,base_cbits_op,cbits_base_op) ) { tick() };
						//
						auto val_n_tbits = a_val_n_tbits->exchange(((uint64_t)UINT32_MAX << HALF),std::memory_order_acquire);
						auto ky_n_cbits = a_ky_n_cbits->fetch_or((((uint64_t)UINT32_MAX << HALF) | IMMOBILE_CBIT_SET),std::memory_order_acquire);
						//
						uint32_t save_value = (val_n_tbits >> HALF);
						uint32_t save_key = (ky_n_cbits >> HALF);
						//
						CBIT_stash_holder *base_cse = (cbit_stashes[0]->_is_base_mem == CBIT_BASE) ? cbit_stashes[0] :  cbit_stashes[1];
						CBIT_stash_holder *cse = (cbit_stashes[0]->_is_base_mem == CBIT_MEMBER) ? cbit_stashes[0] :  cbit_stashes[1];
						//
						stash_key_value(save_key,save_value,cse);  // stash the key,value with the bucket the belong to the member
						//
						// stash the cbits update ... then last man out
						stash_bits_clear((1 << backref),base_cse);
						// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
						// membership stash
						cse = from_member_to_base(cbits_op,cse);
						cse->_updating++; // add an updater on behalf of the base, which will later release the member when it unstashes
						//
						a_val_n_tbits = (offset_value << HALF) | 0x1;
						//
						ky_n_cbits = (el_key << HALF) | (ky_n_cbits & UINT32_MAX);  // keep the reference to the stash for now
						a_ky_n_cbits->store(ky_n_cbits,std::memory_order_release);
						a_val_n_tbits->store(a_val_n_tbits,std::memory_order_release);    // next fix up the taken bits
						// don't unstash_base_cbits(control_bits,cse); yet. The base should complete its operations first.
						wakeup_value_restore(HH_FROM_EMPTY, control_bits, cbits, cbits_op, cbits_op_base, bucket, h_bucket, el_key, offset_value, which_table, buffer, end, cse);
						//
						// the usurped value can occupy another spot in its base
						// don't unstash_base_cbits(base_control_bits,base_cse), it will be done after the reinsertion is resolved
						auto base_h_bucket = (h_bucket-backref);		// next reinser the displaced value now currently stashed
						wakeup_value_restore(HH_FROM_BASE_AND_WAIT, base_control_bits, base_cbits, base_cbits_op, cbits_base_op, cells_base, base_h_bucket, save_key, save_value, which_table, buffer, end, base_cse);
						//
					} else {
						control_bits = load_cbits(bucket,cbits,cbits_op);
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

		uint64_t _internal_update(atomic<uint64_t> *a_c_bits, hh_element *base, uint32_t cbits, uint32_t cbits_op, uint32_t el_key, uint32_t h_bucket, uint32_t v_value, uint8_t thread_id, uint32_t modifier, uint32_t el_key_update = 0) {
			//
			uint32_t value = 0;
			hh_element *storage_ref = _get_bucket_reference(a_c_bits, base, cbits, cbits_op h_bucket, el_key, buffer, end, thread_id,value);  // search
			//
			if ( storage_ref != nullptr ) {
				// 
				//
				uint64_t loaded_key = (((uint64_t)el_key) << HALF) | v_value; // LOADED
				loaded_key = stamp_key(loaded_key,selector);
				//
				//
				atomic<uint64_t> *a_cell_cbits_ky = (atomic<uint64_t> *)(&(storage_ref->c));
				//
				if ( base != storage_ref ) {
					//
					auto cbits_ky = a_cell_cbits_ky->load(std::memory_order_acquire);

					if ( el_key_update != 0 ) {
						auto update_key_cbits = (el_key_update & ((uint64_t)UINT32_MAX << sizeof(uint32_t)));
						cbits_ky = update_key_cbits | (cbits_ky & (uint64_t)IMMOBILE_CBIT_RESET) | modifier;
					} else {
						cbits_ky &= IMMOBILE_CBIT_RESET64;
						cbits_ky |= modifier;
					}

					a_cell_cbits_ky->store(cbits_ky,std::memory_order_release);
					//
				}
				//
				storage_ref->tv.value = v_value;
				//
				return(loaded_key);
			}
			//
			return(UINT64_MAX); // never locked
		}

};






/**
 * CLASS: HH_map
 * 
 * Initialization of shared memory sections... 
 * 
*/


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
		bool prepare_for_add_key_value_known_refs(atomic<uint32_t> **control_bits_ref,uint32_t h_bucket,uint8_t &which_table,uint32_t &cbits,uint32_t &cbits_op,uint32_t &cbits_base_op,uint32_t &cbits_base_ops,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,CBIT_stash_holder *cbit_stashes[4]) {
			//
			//
			atomic<uint32_t> *a_c_bits = _get_member_bits_slice_info(h_bucket,which_table,cbits,cbits_op,cbits_base_ops,bucket_ref,buffer_ref,end_buffer_ref,cbit_stashes);
			//
			if ( a_c_bits == nullptr ) return false;
			*control_bits_ref = a_c_bits;

			return true;
		}


		/**
		 * add_key_value_known_refs
		 * 
		 * 
		 * cbits -- reflects the last known state of the bucket in op or a membership map
		*/
		uint64_t add_key_value_known_refs(atomic<uint32_t> *control_bits,uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table,uint32_t cbits,uint32_t cbits_op,uint32_t cbits_base_op,hh_element *bucket,hh_element *buffer,hh_element *end_buffer,CBIT_stash_holder *cbit_stashes[4]) {
			//
			uint8_t selector = 0x3;
			//
			if ( (offset_value != 0) &&  selector_bit_is_set(h_bucket,selector) ) {
				//
				return _first_level_bucket_ops(atomic<uint32_t> *control_bits,full_hash,h_bucket,offset,which_table,thread_id,cbits,cbits_op,cbits_base_op,bucket,buffer,end_buffer,cbit_stashes);
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
			if ( el_key == UINT32_MAX ) return UINT32_MAX;
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
			uint32_t op_cbits = 0;
			uint32_t value = 0;
			//
			uint8_t tries = 0;
			while ( tries++ < 3 ) {
				//
				if ( empty_bucket(a_c_bits,base,cbits,op_cbits) ) return UINT32_MAX;   // empty_bucket cbits by ref
				//
				hh_element *storage_ref = _get_bucket_reference(a_c_bits, base, cbits, op_cbits, h_bucket, el_key, buffer, end, thread_id, value);  // search
				//
				if ( storage_ref == nullptr ) {
					uint8_t op_thrd = cbits_thread_id_of(op_cbits);
					if ( _check_key_value_stash(op_thrd,el_key,value) ) {
						return value;
					}
					tick();
				} else {
					// clear mobility lock
					remobilize(storage_ref);
					return value;
				}
			}
			//
			return UINT32_MAX;
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
			if ( el_key == UINT32_MAX ) return UINT64_MAX;
			//
			uint8_t selector = 0;
			if ( selector_bit_is_set(h_bucket,selector) ) {
				h_bucket = clear_selector_bit(h_bucket);
			} else { return UINT64_MAX; }
			//
			hh_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			hh_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			hh_element *base = bucket_at(buffer, h_bucket, end);
			atomic<uint64_t> *a_c_bits = (atomic<uint64_t> *)(&(base->c));
			uint32_t cbits = 0;
			uint32_t op_cbits = 0;
			if ( empty_bucket(a_c_bits,base,cbits,op_cbits) ) return UINT64_MAX;   // empty_bucket cbits by ref
			// CALL UPDATE (VALUE UPDATE NO KEY UPDATE)
			return _internal_update(a_c_bits, base, cbits, op_cbits, el_key, h_bucket, v_value, thread_id, 0);
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
			if ( el_key == UINT32_MAX ) return UINT32_MAX;
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
			uint32_t op_cbits = 0;
			uint32_t root_ky = 0;
			if ( empty_bucket(a_c_bits,base,cbits,op_cbits,root_ky) ) return UINT32_MAX;   // empty_bucket cbits by ref
			//
			// cbits are real cbits
			if ( root_ky == el_key ) {  // deleting the base -- make it a special case
				if ( wait_become_bucket_delete_master(HH_DELETE_BASE,a_c_bits,thread_id) ) {  // failure may mean that an adder has a value to put in 
					if ( popcount(cbits) == 1 ) {  // Hence the whole bucket will be empty
						atomic<uint64_t> *a_t_bits = (atomic<uint64_t> *)(&(base->tv));
						a_remove_back_taken_spots(base,0,1,buffer,end);
						a_t_bits->store(0);
						a_c_bits->store(0);   // throws out bucket master identity as well
					} else {
						// swap in the next element and mark its old position as deleted
						a_swap_next(a_c_bits,base,cbits,UINT32_MAX,0,buffer,end,MOBILE_CBIT_SET);
						submit_for_cropping(base,cbits,buffer,end,selector);  // after a total swappy read, all BLACK keys will be at the end of members
						release_delete_master(HH_DELETE_BASE,a_c_bits,thread_id)
					}
				}
				return;
			}
			// set that edit is taking place
			// CALL UPDATE (VALUE UPDATE PLUS KEY UPDATE) -- key update is special value
			uint64_t loaded = _internal_update(a_c_bits, base, cbits, op_cbits, el_key, h_bucket, 0, thread_id, MOBILE_CBIT_SET, UINT32_MAX);
			if ( loaded == UINT64_MAX ) return UINT32_MAX;
			//
			submit_for_cropping(base,cbits,buffer,end,selector);  // after a total swappy read, all BLACK keys will be at the end of members
			return;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * complete_bucket_creation
		*/

		void complete_bucket_creation(atomic<uint32_t> *control_bits, uint32_t &cbits, uint32_t &cbits_ops, uint32_t &cbits_op_base, hh_element *bucket, hh_element *buffer, hh_element *end_buffer) {
			//
			a_place_back_taken_spots(bucket, 0, buffer, end);
			create_base_taken_spots(bucket, buffer, end);   // this bucket is locked 
			//
		}



		/**
		 * complete_add_bucket_member
		 * 
		 * Use a reserved position to store a new element (actually restore one moved out of the base bucket)
		 * 
		 * Sets up ownership of the bit in tbits and stores the membership in cbits at the same offset via `reserve_membership`.
		 * The membership should (except under race coniditions) allow for a simple insertion of the key and value in the
		 * new hole. If `reserve_membership` fails, then it should be the case that the tbits are full and this method 
		 * will return false. In other words, the method `reserve_membership` is expected to set up some empty cell preemptively
		 * even if that means that one thread or other will lose the race for it first try, and this expectation remains 
		 * true until there are no more accessible positions left.
		 * 
		*/

		bool complete_add_bucket_member(atomic<uint32_t> *control_bits, uint32_t &cbits, uint32_t &cbits_ops, uint32_t &cbits_op_base, uint32_t el_key, uint32_t offset_value, uint32_t store_time, hh_element *base, hh_element *buffer, hh_element *end_buffer) {
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->->tv.taken));
			uint8_t hole;
			// mark position as immobile
			if ( reserve_membership(control_bits,base,a_tbits,cbits,cbits_ops,cbits_op_base,hole,thread_id,buffer,end_buffer) ) {
				//
				// should be in the clear to write whatever at this point.
				// At this point, the stash records for the hole should be returned 
				// It it is possible that an immobility bit could be set to block immediate usurpation until
				// this operation completes. Deletion should also be delayed. 
				// This state of affairs can be regulated by the base. ... A delete that comes in at this point 
				// might wait until the base membership is completely unstashed. If not, the base will overwrite the
				// future delete. (So, the stash removal method can state that it failed to the calling remover given that the adder bit
				// will be set for the same position.)
				auto bucket = base + hole;
				bucket = el_check_end_wrap(bucket,buffer,end);
				//
				atomic<uint64_t> *a_ky_n_cbits = (uint64_t)(&(bucket->c));
				atomic<uint64_t> *a_val_n_tbits = (uint64_t)(&(bucket->tv));
				//
				uint64_t offset_to_base = (hole << 1); // always 0 in the lsb
				uint64_t ky_n_cbits = (((uint64_t)(el_key) << HALF) | offset_to_base);
				uint64_t val_n_tbits = (((uint64_t)(offset_value) << HALF) | store_time);  // value and timing (when timing is helpful)
				//
				a_val_n_tbits->store(val_n_tbits,std::memory_order_release);
				a_ky_n_cbits->store(ky_n_cbits,std::memory_order_release);  // similar to publication
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
		bool short_list_old_entry([[maybe_unused]] uint32_t key, [[maybe_unused]] uint32_t value,[[maybe_unused]] uint32_t store_time) {
			return true;
		}


		/**
		 * value_restore_runner   --- a thread method...
		 * 
		 * One loop of the main thread for restoring values that have been replaced by a new value.
		 * The element being restored may still be in play, but 
		*/
		void value_restore_runner(uint8_t slice_for_thread, uint8_t assigned_thread_id) {
			atomic<uint32_t> *control_bits = nullptr;
			uint32_t real_cbits = 0;
			uint32_t cbits_op = 0;
			uint32_t cbits_op_base = 0;
			//
			hh_element *hash_ref = nullptr;
			uint32_t h_bucket = 0;
			uint32_t el_key = 0;
			uint32_t offset_value = 0;
			uint64_t loaded_value = 0;
			uint8_t which_table = slice_for_thread;
			hh_element *buffer = nullptr;
			hh_element *end = nullptr;
			hh_adder_states update_type;
			//
			while ( is_restore_queue_empty(assigned_thread_id,slice_for_thread) ) wait_notification_restore();
			//
			dequeue_restore(update_type, &control_bits, real_cbits, cbits_op, cbits_op_base, &hash_ref, h_bucket, el_key, offset_value, which_table, assigned_thread_id, &buffer, &end);
			// store... if here, it should be locked
			//
			uint32_t store_time = now_time(); // now
			hh_element *yielding_base = nullptre;

			switch ( update_type ) {
				case HH_FROM_EMPTY: {
					// at this point the empty bucket should be totally claimed, however, 
					// the memory map (tbits) are not set up causing hole discovery and search 
					// to have points of failure... This should be easily rectified
					///
					complete_bucket_creation(control_bits, real_cbits, cbits_op, cbits_op_base, hash_ref, buffer, end_buffer);
					//
					uint8_t count_result = 0;
					q_count_decr(cbits_op,count_result);  // a q count for ops sequenced by queuing to the restore queue.
					if ( count_result == 0 ) {
						release_bucket_state_master(control_bits, real_cbits);
					}
					break;
				}
				case HH_FROM_BASE_AND_WAIT: {
					// at this point, the value in the base is the new value.
					// The value taken from the restore queue has to be shifted into the membership map. cbits.
					// Or, just a new position will be staked out and the element placed there without regard to order.
					// The cbits must be updated by acquiring a new hole identified by the taken bits.
					// it is possible that by the time the value to be restored arrives here that
					// the taken positions available to the base are gone. In that case, a version of usurpation
					// may occur, or the oldest elements (or deleted and not yet serviced elements) may be shifted out.
					auto prev_bits = control_bits->load(std::memory_order_acquire);
					// store ....  pop until oldest
					if ( !(complete_add_bucket_member(control_bits, real_cbits, cbits_op_base, el_key, offset_value, store_time, hash_ref, buffer, end_buffer, thread_id)) ) {
						yielding_base = handle_full_bucket_by_usurp(hash_base, real_cbits, el_key, offset_value, time, offset, buffer, end_buffer, thread_id);
					}
					//
					uint8_t count_result = 0;
					q_count_decr(prev_bits,count_result);
					if ( count_result == 0 ) {
						release_bucket_state_master(control_bits, fetch_real_cbits(prev_bits));
					}
					break;
				}
				default: {
					// an impossible state of affairs unless someone has altered the insertion methods. 
					break;
				}
			}

			if ( yielding_base !== nullptr ) {
				// if the entry can be entered onto the short list (not too old) then allow it to be added back onto the restoration queue...
				if ( short_list_old_entry(el_key, offset_value,, store_time) ) {
					control_bits = (atomic<uint32_t> *)(&(yielding_base->c.bits));
					load_cbits(control_bits,cbits,cbits_op);
					set_control_bit_state(control_bits,cbits_op,EDITOR_CBIT_SET);
					while ( !wait_on_max_queue_incr(control_bits,cbits,cbits_op,cbits_base_op) ) { tick(); }

					h_bucket = (yielding_base - buffer);
					// el_key, offset_value have been updated to contain the yield member value 

					CBIT_stash_el *cse = nullptr;
					if ( cbits_op != 0 ) {
						cse = cbits_stash_index_of(cbits_op);
					} else {
						CBIT_stash_holder *cbit_stashes[4];
						stash_cbits(control_bits, yielding_base, cbits, cbits_op, cbits_op_base, cbit_stashes, buffer, end);
						cse = cbit_stashes[0];
					}

					wakeup_value_restore(HH_FROM_BASE_AND_WAIT, control_bits, cbits, cbits_op, cbits_op_base, yielding_base, h_bucket, el_key, offset_value, which_table, buffer, end, cse);
				}
			}

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



#ifdef _DEBUG_TESTING_
	public:			// these may be used in a test class...
#else
	protected:
#endif


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * ensure_is_member
		*/

		bool ensure_is_member(hh_element *bucket) {
			atomic<uint64_t> *a_cbits = (atomic<uint64_t> *)(&(bucket->c));
			auto cbits_n_ky = a_cbits->load(std::memory_order_acquire);
			//
			auto cbits = (uint32_t)(cbits_n_ky & UINT32_MAX);
			auto ky = (uint32_t)((cbits_n_ky >> sizeof(uint32_t)) & UINT32_MAX);
			//
			if ( ky == UINT32_MAX ) return false;
			//
			if ( is_member_bucket(cbits) ) {
				if ( is_in_any_state_of_delete(cbits) ) return false;
				return true;
			}
			return false;
		}


		/**
		 * seek_next_base
		*/
		hh_element *seek_next_base(hh_element *base_probe, uint32_t &c, uint32_t &base_cbits, uint32_t &offset_nxt_base, hh_element *buffer, hh_element *end) {
			hh_element *hash_base = base_probe;
			while ( c ) {   // c changes and `seek_next_base` may reenter with c in its last state
				auto offset_nxt = get_b_offset_update(c);
				base_probe += offset_nxt;
				base_probe = el_check_end_wrap(base_probe,buffer,end);
				auto cbits = get_real_base_cbits(base_probe);
				//
				if ( popcount(cbits) > 1 ) {
					base_cbits = cbits;
					offset_nxt_base = offset_nxt;
					return base_probe;
				}
				//
				base_probe = hash_base;
			}
			return nullptr;
		}

		/**
		 * seek_min_member
		 * 
		 * The min member could change if there is a usurpation.
		*/

		void seek_min_member(hh_element **min_probe_ref, uint32_t &min_base_offset, hh_element *base_probe, uint32_t &time, uint32_t lowest_free_member,  uint32_t offset_nxt_base, hh_element *buffer, hh_element *end) {
			atomic<uint32_t> *a_cbits = (atomic<uint32_t> *)(&(base_probe->c.bits));
			auto c = a_cbits->load(std::memory_order_acquire);		// nxt is the membership of this bucket that has been found
			c = c & (~((uint32_t)0x1));   // the interleaved bucket does not give up its base... (should already be zero)
			if ( offset_nxt_base < lowest_free_member ) {
				c = c & ones_above(lowest_free_member - offset_nxt_base);  // don't look beyond the window of our base hash bucket
			}
			c = c & zero_above((NEIGHBORHOOD-1) - offset_nxt_base); // don't look outside the window
			//
			while ( c ) {			// 
				auto vb_probe = base_probe;
				auto offset_min = get_b_offset_update(c);
				vb_probe += offset_min;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				if ( ensure_is_member(vb_probe) ) {
					if ( vb_probe->->tv.taken < time ) {
						time = vb_probe->->tv.taken;
						*min_probe_ref = vb_probe;
						min_base_offset = offset_min;
					}
				}
				// free lock this slice vb_probe,  when the above call returned true
			}
			//
		}



		/**
		 * handle_full_bucket_by_usurp
		 * 
		 * This is essentially hopscotch when the all the spots in a window are taken. 
		 * But, try to find something old. When something is too old and things are too full,
		 * it may be evicted, gracefully aged out by sending the entry to the next tier or of to FSM storage.
		 * (At the point of departure the application supplies a method to be called here)
		 * 
		 * The basic plan is to find a bucket that has a member it can give up and is within the window of the base.
		 * If it can be found, then usurp the element, but instead of making a new base 
		 * don't change the taken map, which is all spots taken, but clear the bit of the donating bucket.
		 * Then, add the bit at the offset to the cbit membership map of the base. Then, add the old member back into 
		 * the base if possible. Add it back in doing the equivalent of HH_FROM_BASE_AND_WAIT
		 * 
		 * 
		*/
		bool handle_full_bucket_by_usurp(hh_element *hash_base, uint32_t cbits, uint32_t &el_key, uint32_t &value, uint32_t &time, hh_element *buffer, hh_element *end, uint8_t thread_id) {
			//
			uint32_t min_base_offset = 0;
			uint32_t offset_nxt_base = 0;
			//
			auto min_probe = hash_base;
			auto min_base = hash_base;
			auto base_probe = hash_base;

			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(hash_base->tv.taken));
			auto b = a_tbits->load(std::memory_order_acquire);
			//
			a = cbits;
			uint32_t c = (a ^ b);  // these are base positions.
			//
			auto lowest_free_member = countr_zero(cbits);   // a zero will be here ... 
			//
			while ( c ) {
				uint32_t base_cbits = 0;
				base_probe = seek_next_base(hash_base, c, base_cbits, offset_nxt_base, buffer, end);
				// look for a bucket within range that may give up a position
				if ( base_probe != nullptr ) {		// landed on a bucket (now what about it)
					seek_min_member(&min_probe, min_base_offset, base_probe, time, offset_nxt_base, buffer, end);
				}
				// unlock slice counter base_probe
			}
			if ( (min_probe != hash_base) && (base_probe != hash_base) ) {  // found a place in the bucket that can be moved... (steal this spot)
				//
				uint32_t pky = el_key;  // just unloads it (was index)
				uint32_t pval = value;

				atomic<uint64_t> *a_tbits_n_val = (atomic<uint32_t> *)(&(min_probe->tv));
				atomic<uint64_t> *a_cbits_n_ky = (atomic<uint32_t> *)(&(min_probe->c));

				auto min_probe_tbits_n_val = a_tbits_n_val->load(std::memory_order_acquire);
				auto min_brobe_cbits_n_ky = a_cbits_n_ky->load(std::memory_order_acquire);

				uint32_t usurp_pos_offset = ((min_base_offset + offset_nxt_base) << 1);
				uint64_t cbits_n_ky = ( ((uint64_t)pk << HALF) | ((uint64_t)(usurp_pos_offset) & UINT32_MAX) );
				uint64_t tbits_n_val = ( ((uint64_t)pval << HALF) | ((uint64_t)(time) & UINT32_MAX) );
				//
				a_cbits_n_ky->store(cbits_n_ky);
				a_tbits_n_val->store(tbits_n_val);
				//
				a_clear_cbit(min_base,min_base_offset);
				a_add_cbit(hash_base,(min_base_offset + offset_nxt_base));

				el_key = ((min_brobe_cbits_n_ky >> HALF) & UINT32_MAX);
				value = ((min_probe_tbits_n_val >> HALF) & UINT32_MAX);
				//
				stash_key_value(el_key,value,thread_id);
				//
				return true
			}
			return false;
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
		 * create_base_taken_spots
		*/

		void create_base_taken_spots(hh_element *hash_ref, uint32_t value, uint32_t el_key, hh_element *buffer, hh_element *end_buffer) {
			hh_element *base_ref = hash_ref;
			uint32_t t = 1;				// c will be the new base element map of taken positions
			TBIT_stash_el *tse_base = stash_taken_spots(base_ref,t); // taken by ref
			for ( uint8_t i = 1; i < NEIGHBORHOOD; i++ ) {
				hash_ref = el_check_end_wrap( ++hash_ref, buffer, end ); // keep walking forward to the end of the window
				if ( bucket_is_base(hash_ref) ) {			// this is a base bucket with information beyond the window
					uint32_t taken = 0;
					TBIT_stash_el *tse = stash_taken_spots(hash_ref,taken); // taken by ref
					stash_real_tbits((taken << i),tse);
					break;								// don't need to get more information at this point
				} else if ( hash_ref->c.bits != 0 ) {
					SET(t,i);							// this element belongs to a base below the new base
				}
			}

			stash_real_tbits(t,tse);
			atomic<uint32_t> *b_t_bits = (atomic<uint32_t> *)(&base_ref->->tv.taken);
			_unstash_base_tbits(a_tbits,tse_base);    // unstash if last out
		}


		/**
		 * reserve_membership - is called at a point after cbits have been altered in the shared position 
		 * and original bits is the membership mask that must change
		*/

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
		 * reserve_membership
		 * 
		 * searches for an empty spot in the allocation map stored at the bucket. It takes into consideration that
		 * another thread may be examining the bucket, or even adding an element. In such cases, the tbits will be stashed
		 * and the operation tbits from the base will be a double semaphore for readers and swappy ops.
		 * 
		 * This method utimately operates on the tbits in stash using the operational thread. If this method has authority,
		 * it will restore the tbits to their location in the bucket upon completion of its operation.
		 * 
		*/
		bool reserve_membership(atomic<uint32_t> *control_bits, hh_element *base, atomic<uint32_t> *a_tbits, uint32_t &cbits, uint32_t &cbits_ops, uint32_t &cbits_op_base, uint8_t &hole, uint8_t thread_id, hh_element *buffer, hh_element *end_buffer ) {
			// first unload the taken spots... 
			uint32_t tbits = 0;												// tbits passed by reference
			TBIT_stash_el *tse = nullptr;
			while ( true ) {
				//
				TBIT_stash_el *tse = tbits_add_swappy_op(base, cbits, tbits);
				if ( tse == nullptr ) break;
				//
				auto a = cbits;    // these should be stored in the thread table at the time of call ... cbits indicate the ownership of the cell
				auto b = tbits;
				//
				hole = countr_one(b);  // first zero anywhere inside the word starting from lsb. Hence closest to the base
				if ( hole < sizeof(uint32_t) ) {
					//										// tbits by reference, tbits and b should be the same after op
																	  // hole by reference -- should settle ownership
					b = a_tbit_store_reservation(a_tbits, tse, tbits, hole, hbit);  // store the taken spot 
					// in storing the spot, if another thread took it, then the value of the 
					// cbits filed in the reserved bucket will not be zero.
					//
					if ( hole < sizeof(uint32_t) ) {  // or, the bucket might already be full
						// now put a stamp on the cell that should be reserved (if not the cell will be at least partially populated)
						hh_element *reserved = base + hole;		// nothing about reserved should be stashed at this point
						atomic<uint32_t> *a_reserved = (atomic<uint32_t> *)(reserved->c.bits);
						//
						// the member has zero cbits (if not touched by another thread)
						uint32_t cc = 0;	// cbits of reserved match zero if not preempted by other writer
											// put in a back reference member style cbit with immobility set
						auto new_member = (hole << 1) | CBIT_BASE_MEMBER_BIT | IMMOBILE_CBIT_SET;
						if ( a_reserved->compare_exchange_weak(cc,new_member,std::memory_order_acq_rel) ) {
							// the s
							auto c = (a ^ b);    // c contains bucket starts..
							a_place_taken_spots(hash_base, hole, c, buffer, end_buffer);  // add the position to the base memory record
							a_store_real_cbits(control_bits,cbits,cbits_ops,hbit,thread_id);  // since we are here, there bucket is owned by the thread
							tbits_remove_swappy_op(a_tbits,tse);  // expecting `tbits_remove_swappy_op` to unstash tbits
							return true
						} // else continuing
					} else {
						break;
					}
				} else {
					break;
				}
				//
				tbits_remove_swappy_op(a_tbits,tse);
			}	// end of while
			//
			if ( tse != nullptr ) {
				tbits_remove_swappy_op(a_tbits,tse);
			}
			return false;
		}


		/**
		 * a_place_taken_spots
		 * 
		 * c contains bucket starts..  Each one of these may be busy....
		 * but, this just sets the taken spots; so, the taken spots may be treated as atomic...
		 * 
		*/
		void a_place_taken_spots(hh_element *hash_ref, uint32_t hole, uint32_t c, hh_element *buffer, hh_element *end) {
			hh_element *nxt_base = nullptr;
			//
			c = c & zero_above(hole);  // moving from the base towards the hole and take the position at eash base in between
			while ( c ) {
				nxt_base = hash_ref;
				uint8_t offset = get_b_offset_update(c);			
				nxt_base += offset;
				nxt_base = el_check_end_wrap(nxt_base,buffer,end);
				//
				uint32_t taken_bit = (1 << (hole - offset));  // this spot has been taken
				//
				atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(nxt_base->->tv.taken));
				//
				TBIT_stash_el *tse = stash_taken_spots(nxt_base);
				stash_real_tbits(taken_bit,tse);
				_unstash_base_tbits(a_tbits,tse);    // unstash if last out
			}
			// now look below the base for bases that are within a window of the hole
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
			hh_element  *all_taken_spots[32];
			TBIT_stash_el *all_taken_spot_refs[32];
			uint8_t count_bases = 0;
			//
			if ( last_view > 0 ) { // can't influence any change below the bucket
				hh_element *vb_probe = hash_base - last_view;
				vb_probe = el_check_beg_wrap(vb_probe,buffer,end);
				uint32_t c = 0;
				uint8_t k = g;     ///  (dist_base + last_view) :: dist_base + (g - dist_base) ::= dist_base + (NEIGHBORHOOD - 1) - dist_base ::= (NEIGHBORHOOD - 1) ::= g
				// go until vb_probe hits on a base
				uint32_t taken;
				while ( vb_probe != hash_base ) {
					if ( bucket_is_base(vb_probe,c) ) {
						TBIT_stash_el *tse = stash_taken_spots(vb_probe);
						all_taken_spots[count_bases] = vb_probe;
						all_taken_spot_refs[count_bases++] = tse;
						stash_real_tbits(((uint32_t)0x1 << k),tse);
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				// get a map of all the base elements above the lowest bucket found
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & taken;  // bases 
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					uint32_t c2 = 0;
					if ( bucket_is_base(base_probe,c2) ) {  // double check
						auto j = k;
						j -= offset_nxt;
						//
						all_taken_spots[count_bases] = base_probe;
						TBIT_stash_el *tse = stash_taken_spots(base_probe);
						all_taken_spot_refs[count_bases++] = tse;
						stash_real_tbits(((uint32_t)0x1 << k),tse);
						//
						c = c & ~(c2);  // no need to look at base probe members anymore ... remaining bits are other buckets
					}
				}

				for ( uint8_t i = 0; i < count_bases; i++ ) {
					TBIT_stash_el *tse = all_taken_spot_refs[i];
					hh_element *base = all_taken_spots[count_bases];
					atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
					_unstash_base_tbits(a_tbits,tse);
				}

			}
		}




		//  REMOVE TAKEN SPOTS

		/**
		 * a_remove_back_taken_spots
		*/

		void a_remove_back_taken_spots(hh_element *hash_base, uint8_t dist_base, uint32_t pattern, hh_element *buffer, hh_element *end) {
			//
			uint8_t g = NEIGHBORHOOD - 1;
			auto last_view = (g - dist_base);
			hh_element  *all_taken_spots[32];
			TBIT_stash_el *all_taken_spot_refs[32];
			atomic<uint32_t> *all_taken_spot_refs[32];
			uint8_t count_bases = 0;
			//
			if ( last_view > 0 ) { // can't influence any change below the bucket
				hh_element *vb_probe = hash_base - last_view;
				vb_probe = el_check_beg_wrap(vb_probe,buffer,end);
				uint32_t c = 0;
				uint8_t k = g;     ///  (dist_base + last_view) :: dist_base + (g - dist_base) ::= dist_base + (NEIGHBORHOOD - 1) - dist_base ::= (NEIGHBORHOOD - 1) ::= g
				// go until vb_probe hits on a base
				uint32_t taken;
				while ( vb_probe != hash_base ) {
					if ( bucket_is_base(vb_probe,c) ) {
						TBIT_stash_el *tse = stash_taken_spots(vb_probe);
						all_taken_spots[count_bases] = vb_probe;
						all_taken_spot_refs[count_bases++] = tse;
						stash_tbits_clear((pattern << k),tse);
						break;
					}
					vb_probe++; k--;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				}
				//
				// get a map of all the base elements above the lowest bucket found
				c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
				c = c & taken;  // bases 
				//
				while ( c ) {
					auto base_probe = vb_probe;
					auto offset_nxt = get_b_offset_update(c);
					base_probe += offset_nxt;
					base_probe = el_check_end_wrap(base_probe,buffer,end);
					uint32_t c2 = 0;
					if ( bucket_is_base(base_probe,c2) ) {  // double check
						auto j = k;
						j -= offset_nxt;
						TBIT_stash_el *tse = stash_taken_spots(vb_probe);
						all_taken_spots[count_bases] = vb_probe;
						all_taken_spot_refs[count_bases++] = tse;
						stash_tbits_clear((pattern << j),tse);
						c = c & ~(c2);  // no need to look at base probe members anymore ... remaining bits are other buckets
					}
				}

				for ( uint8_t i = 0; i < count_bases; i++ ) {
					TBIT_stash_el *tse = all_taken_spot_refs[i];
					hh_element *base = all_taken_spots[count_bases];
					atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
					_unstash_base_tbits(a_tbits,tse);
				}

			}
		}



		/**
		 * remove_bucket_taken_spots
		*/
		void a_remove_bucket_taken_spots(hh_element *hash_base, uint32_t bases, uint8_t nxt_loc, uint32_t c_pattern, hh_element *buffer, hh_element *end) {
			auto c = bases;
			c = c & zero_above(nxt_loc);	// buckets after the removed location do not see the location; so, don't process them
			while ( c ) {					// c is a pattern indicating bases
				hh_element *nxt_base = hash_base;
				auto offset = get_b_offset_update(c);
				nxt_base += offset;
				nxt_base = el_check_end_wrap(nxt_base,buffer,end);
				//
				auto removal_mask = (c_pattern << (nxt_loc - offset));

				TBIT_stash_el *tse = stash_taken_spots(nxt_base);
				stash_tbits_clear(removal_mask,tse);
			}
		}


		/**
		 * a_removal_taken_spots
		 * 
		 * when called any shifing will already be done.
		 * 
		 * The base will not be the subject of this operation.
		 * 
		*/
		void a_removal_taken_spots(hh_element *hash_base,uint32_t cbits, uint32_t c_pattern, hh_element *buffer, hh_element *end, uint8_t thread_id) {
			//
			uint32_t a = c_pattern; // membership mask
			uint32_t b = get_real_base_tbits(hash_base);   // load this or rely on lock
			//
			auto tbits_update = a ^ b;
			store_real_tbits(tbits_update,thread_id);

			auto bases = cbits ^ tbits_update;

			// OTHER buckets
			// now look at the bits within the range of the current bucket indicating holdings of other buckets.
			// these buckets are ahead of the base, but behind the cleared position...
			uint8_t nxt_loc = (NEIGBORHOOD - countl_zero(a)) - popcount(a); // they are now all consecutive
			a_remove_bucket_taken_spots(hash_base,bases,nxt_loc,c_pattern,buffer,end);
			// look for bits preceding the current bucket
			a_remove_back_taken_spots(hash_base, nxt_loc, c_pattern, buffer, end);
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
		 * a_swap_values -- swap only in a bucket that allows one swappy op at a time.
		*/
		void a_swap_values(h_element *b1,h_element *b2) {
			atomic<uint64_t> *a_b1_cbits = (atomic<uint64_t> *)(&(b1->c));
			atomic<uint64_t> *a_b2_cbits = (atomic<uint64_t> *)(&(b2->c));
			//
			atomic<uint64_t> *a_b1_tbits = (atomic<uint64_t> *)(&(b1->tv));
			atomic<uint64_t> *a_b2_tbits = (atomic<uint64_t> *)(&(b2->tv));
			
			auto cbits_n_ky_1 = a_b1_cbits->load(std::memory_order_acquire);
			auto cbits_n_ky_2 = a_b2_cbits->load(std::memory_order_acquire);
			//
			auto tbits_n_val_1 = a_b1_tbits->load(std::memory_order_acquire);
			auto tbits_n_val_2 = a_b2_tbits->exchange(tbits_n_val_1,std::memory_order_acq_rel);
			a_b2_tbits->store(tbits_n_val_1,std::memory_order_release);
			//
			auto k1 = (uint32_t)((cbits_n_ky_1 >> HALF) & UINT32_MAX);
			auto k2 = (uint32_t)((cbits_n_ky_2 >> HALF) & UINT32_MAX);
			//
			cbits_n_ky_1 = (cbits_n_ky_1 & UINT32_MAX) | ((uint64_t)k2 << HALF);
			cbits_n_ky_2 = (cbits_n_ky_2 & UINT32_MAX) | ((uint64_t)k1 << HALF);
			//
			cbits_n_ky_1 = clear_bitsdeleted(cbits_n_ky_1);
			cbits_n_ky_2 = set_bitsdeleted(cbits_n_ky_2);
			//
			a_b1_cbits->store(cbits_n_ky_1);
			a_b2_cbits->store(cbits_n_ky_2);
		}



		/**
		 * a_swap_next
		*/

		void a_swap_next(atomic<uint64_t> *base_bits_n_ky, h_element *base, uint32_t cbits, uint32_t key, uint32_t value, hh_element *buffer, hh_element *end, uint32_t modifier = 0;) {
			//
			atomic<uint32_t> *a_base_val = (atomic<uint32_t> *)(&(base->tv.value));
			//
			auto c = cbits & (UINT32_MAX - 1);
			while ( c ) {
				//
				auto vb_probe = base;
				auto offset = get_b_offset_update(c);
				vb_probe += offset;
				vb_probe = el_check_end_wrap(vb_probe,buffer,end);
				//
				if ( !is_deletion_marked(vb_probe)  ) {
					atomic<uint64_t> *a_probe_key = (atomic<uint64_t> *)(&(vb_probe->c));
					auto save_key_n_cbits = a_probe_key->fetch_or(SWAPPY_CBIT_SET,std::memory_order_acq_rel);

					atomic<uint32_t> *a_probe_val = (atomic<uint32_t> *)(&(vb_probe->tv.value));
					auto save_value = a_probe_val->exchange(value,std::memory_order_acq_rel);
					a_base_val->store(save_value);
					//
					auto update_key_n_cbits = (save_key_n_cbits & ((uint64_t)(UINT32_MAX) << HALF) | (cbits & UINT32_MAX));
					base_bits_n_ky->store(update_key_n_cbits,std::memory_order_release);

					update_key_n_cbits = ((save_key_n_cbits & (uint64_t)(SWAPPY_CBIT_RESET64)) | modifier) | ((uint64_t)key << HALF);
					a_probe_key->store(update_key_n_cbits);

					break;
				}
				//
			}

		}



		/**
		 * _cropper
		 * 
		 * 
		 * del_cbits -- last known state of the bucket when the del submitted the base for cropping...
		 * 
		*/

		void _cropper(hh_element *base, uint32_t del_cbits, hh_element *buffer, hh_element *end,uint8_t thread_id) {
			//
			atomic<uint64_t> *base_bits_n_ky = (atomic<uint64_t> *)(&(base->c));
			atomic<uint32_t> *base_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			uint32_t h_bucket = (uint32_t)(base - buffer);
			//
			auto cbits_op_base = gen_bits_editor_active(thread_id);   // editor active  ... 
			//
			atomic<uint32_t> *base_bits = (atomic<uint32_t> *)(&(base->c.bits));
			auto real_cbits = 0;		// cbits by reference...
			while( !(wait_become_bucket_delete_master(HH_DELETE,base_bits,thread_id)) ) {
				tick();
			}
			//

			uint32_t value = 0;
			//
			hh_element *href = _get_bucket_reference(base_bits_n_ky, base, cbits, cbits_op, h_bucket, UINT32_MAX, buffer, end, thread_id, value);
			if ( href != nullptr ) {
				//
				// clear mobility lock
				// mark as deleted 
				remobilize(href,DELETE_CBIT_SET);  // not yet moved to the back of the cbits, but will want to own the swap

				tbits_wait_for_readers(base_tbits);	// allow no-swappy readers to finish, and the bucket becomes swappy
				uint8_t offset = href - base;		// how far from the base?

				hh_element *deletions[NEIGHBORHOOD];
				uint8_t del_count = 0;

				auto c = ones_above(offset-1) & cbits;		// 
				auto c_end = c;
				auto cbits_update = cbits;

				//
				auto tbits = base_tbits->load(std::memory_order_acquire);
				tbits_add_swappy_op(base_tbits,cbits_op,tbits,thread_id);

				while ( c & c_end ) {				// pick up the cells that will be moved to the end of the buffer
					auto vb_probe = base;
					auto offset = get_b_offset_update(c);
					vb_probe += offset;
					vb_probe = el_check_end_wrap(vb_probe,buffer,end);
					//
					uint32_t p_cbits = 0;
					if ( ready_for_delete(vb_probe,p_cbits) ) {
						remobilize(href,DELETE_CBIT_SET);
						while ( c_end ) {
							//
							auto roffset = get_b_reverse_offset_update(c_end);
							//
							if ( roffset > offset ) {
								//
								auto vb_replace = base;
								vb_replace += roffset;
								vb_replace = el_check_end_wrap(vb_replace,buffer,end);
								//
								if ( !is_deletion_marked(vb_replace)  ) {
									a_swap_values(vb_probe,vb_replace);  // swap everything but offsets...
									deletions[del_count++] = vb_replace;
									get_b_reverse_offset_update(cbits_update);
									break; // go get the next thing to delete
								}
								//
								deletions[del_count++] = vb_replace;
								get_b_reverse_offset_update(cbits_update);
							} else {
								deletions[del_count++] = vb_probe;
								get_b_reverse_offset_update(cbits_update);
								break;
							}
							//
						}
						del_count++;
					}
				}
				//
				update_cbits_release(base_bits,cbits_op_base,cbits_update,thread_id);			// update cbits
				//
				auto deleted_cbits = cbits ^ cbits_update;  // for changing the taken bits
				a_removal_taken_spots(base, cbits_update, deleted_cbits, buffer, end, thread_id);
				//
				tbits_remove_swappy_op(base_tbits);
				//
				for ( uint8_t i = 0; i < del_count; i++ ) {  // gone for good
					auto vb_probe = deletions[i];
					atomic<uint64_t> *bnky = (atomic<uint64_t> *)(&(vb_probe->c));
					bnky->store(0);
					atomic<uint64_t> *tv = (atomic<uint64_t> *)(&(vb_probe->tv));
					tv->store(0);
				}
			}

			release_delete_master(HH_DELETE,base_bits,thread_id)
		}

};


#endif // _H_HOPSCOTCH_HASH_SHM_