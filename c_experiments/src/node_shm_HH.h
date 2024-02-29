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
	uint32_t			c_bits;   // control bit mask
	union {
		key_value		_kv;
		uint64_t		_V;
	};
} hh_element;


// ---- ---- ---- ---- ---- ----  HHash
// HHash <- HHASH

template<const uint32_t NEIGHBORHOOD = 32>
class HH_map : public HMap_interface, public Random_bits_generator<> {
	//
	public:

		// LRU_cache -- constructor
		HH_map(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, bool am_initializer = false) {
			_reason = "OK";
			//
			_region = region;
			_endof_region = _region + seg_sz;
			//
			_status = true;
			_initializer = am_initializer;
			_max_count = max_element_count;
			//
			uint8_t sz = sizeof(HHash);
			uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
			//
			// initialize from constructor
			this->setup_region(am_initializer,header_size,(max_element_count/2));
		}


		// REGIONS...

		// setup_region -- part of initialization if the process is the intiator..
		// -- header_size --> HHash
		// the regions are setup as, [values 1][buckets 1][values 2][buckets 2][controls 1 and 2]
		void setup_region(bool am_initializer,uint8_t header_size,uint32_t max_count) {
			// ----
			uint8_t *start = _region;
			HHash *T = (HHash *)start;

			//
			this->_max_n = max_count;
			//
			_T1 = T;   // keep the reference handy
			//
			uint32_t vh_region_size = (sizeof(hh_element)*max_count);
			uint32_t c_regions_size = (sizeof(uint32_t)*max_count);

			auto hv_offset_1 = header_size;
			auto c_offset = (hv_offset_1 + vh_region_size);
			//
			auto next_hh_offset = (hv_offset_1 + vh_region_size);  // now, the next array of buckets and values (controls are the very end for both)
			//

			// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

			T->_HV_Offset = hv_offset_1;
			T->_C_Offset = (hv_offset_1 + vh_region_size)*2;

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
			_region_HV_1 = (hh_element *)(start + hv_offset_1);  // start on word boundary
			_region_HV_1_end = (hh_element *)(start + hv_offset_1 + vh_region_size);

			//
			if ( am_initializer ) {
				if ( check_end((uint8_t *)_region_HV_1) && check_end((uint8_t *)_region_HV_1_end) ) {
					memset((void *)(_region_HV_1),0,vh_region_size);
				} else {
					throw "hh_map (1) sizes overrun allocated region determined by region_sz";
				}
			}

			// # 2
			// ----
			T = (HHash *)(start + next_hh_offset);
			_T2 = T;   // keep the reference handy
			
			start = (uint8_t *)(T);

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
			_region_HV_2 = (hh_element *)(start + hv_offset_1);  // start on word boundary
			_region_HV_2_end = (hh_element *)(start + hv_offset_1 + vh_region_size);

			auto hv_offset_2 = next_hh_offset + header_size;

			T->_HV_Offset = hv_offset_2;
			T->_C_Offset = (hv_offset_2 + vh_region_size)*2;
			//
			//
			_T1->_C_Offset = c_offset*2;			// Both regions use the same control regions (interleaved)
			_T2->_C_Offset = c_offset;				// the caller will access the counts of the two buckets at the same offset often

			//
			//
			_region_C = (uint32_t *)(start + c_offset);  // these start at the same place offset from start of second table

			//
			if ( am_initializer ) {
				//
				if ( check_end((uint8_t *)_region_HV_2) && check_end((uint8_t *)_region_HV_2_end) ) {
					memset((void *)(_region_HV_2),0,vh_region_size);
				} else {
					throw "hh_map (2) sizes overrun allocated region determined by region_sz";
				}
				// one 16 bit word for two counters
				if ( check_end((uint8_t *)_region_C) && check_end((uint8_t *)_region_HV_2_end,true) ) {
					memset((void *)(_region_C), 0, c_regions_size);
				} else {
					throw "hh_map (3) sizes overrun allocated region determined by region_sz";
				}
			}
			//
		}


		// clear
		void clear(void) {
			if ( _initializer ) {
				uint8_t sz = sizeof(HHash);
				uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
				this->setup_region(_initializer,header_size,_max_count);
			}
		}



		bool check_end(uint8_t *ref,bool expect_end = false) {
			if ( ref == _endof_region ) {
				if ( expect_end ) return true;
			}
			if ( (ref < _endof_region)  && (ref > _region) ) return true;
			return false;
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


		static uint32_t check_expected_hh_region_size(uint32_t els_per_tier) {
			//
			uint8_t sz = sizeof(HHash);
			uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
			auto max_count = els_per_tier/2;
			//
			uint32_t vh_region_size = (sizeof(hh_element)*max_count);
			uint32_t c_regions_size = (sizeof(uint32_t)*max_count);
			//
			auto hv_offset_1 = header_size;
			auto next_hh_offset = (hv_offset_1 + vh_region_size);  // now, the next array of buckets and values (controls are the very end for both)
			//
			uint32_t predict = next_hh_offset*2 + c_regions_size;
			return predict;
		} 


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		// THREAD CONTROL

		void tick() {
			nanosleep(&request, &remaining);
		}

		void wakeup_random_genator(uint8_t which_region) {   // 
			// regenerate_shared(which_region);
			_random_gen_value->store(which_region);
		}

		void thread_sleep([[maybe_unused]] uint8_t ticks) {

		}

		void random_generator_thread_runner() {
			while ( true ) {
				uint8_t which_region = _random_gen_value->load();
				if ( which_region != UINT8_MAX ) {
					this->regenerate_shared(which_region);
					_random_gen_value->store(UINT8_MAX);
				}
				thread_sleep(10);
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
		 * bucket_counts
		*/
		pair<uint8_t,uint8_t> bucket_counts(uint32_t h_bucket) {   // where are the bucket counts??
			//
			pair<uint8_t,uint8_t> counts;
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			uint32_t controls = controller->load(std::memory_order_consume);
			//
			do {
				while ( controls & HOLD_BIT_SET ) {  // while some other process is using this count bucket
					controls = controller->load(std::memory_order_consume);
				}
				//
				while ( !controller->compare_exchange_weak(controls,(controls | HOLD_BIT_SET)) && !(controls & HOLD_BIT_SET) );

			 } while ( controls & HOLD_BIT_SET );
			//
			counts.first = controls & COUNT_MASK;
			counts.second = (controls>>EIGHTH) & COUNT_MASK;
			//
			return (counts);
		}

		/**
		 * bucket_counts
		*/
		void bucket_lock(uint32_t h_bucket) {   // where are the bucket counts??
			//
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			uint32_t controls = controller->load(std::memory_order_consume);
			//
			do {
				while ( controls & HOLD_BIT_SET ) {  // while some other process is using this count bucket
					controls = controller->load(std::memory_order_consume);
				}
				//
				while ( !controller->compare_exchange_weak(controls,(controls | HOLD_BIT_SET)) && !(controls & HOLD_BIT_SET) );

			 } while ( controls & HOLD_BIT_SET );
			//
		}

		/**
		 * bucket_count_incr
		*/
		void bucket_count_incr(uint32_t h_bucket,uint8_t which_table) {
			//
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			//
			uint32_t controls = controller->load(std::memory_order_consume);
			if ( !(controls & HOLD_BIT_SET) ) {	// should be the only one able to get here on this bucket.
				this->_status = -1;
				return;
			}
			//
			uint8_t counter = 0;
			if ( which_table == 0 ) {
				counter = controls & COUNT_MASK;
				counter++;
				counter = min(counter,(uint8_t)COUNT_MASK);
				controls = (controls & ~COUNT_MASK) | (COUNT_MASK & counter);
			} else {
				counter = (controls>>EIGHTH) & COUNT_MASK;
				counter++;
				counter = min(counter,(uint8_t)COUNT_MASK);
				uint32_t update = (counter << EIGHTH) & HI_COUNT_MASK;
				controls = (controls & ~HI_COUNT_MASK) | update;
			}
			//
			controller->store((controls & FREE_BIT_MASK),std::memory_order_release);
		}



		/**
		 * bucket_count_decr
		*/
		void bucket_count_decr(uint32_t h_bucket,uint8_t which_table) {
			//
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			//
			uint32_t controls = controller->load(std::memory_order_consume);
			if ( !(controls & HOLD_BIT_SET) ) {	// should be the only one able to get here on this bucket.
				this->_status = -1;
				return;
			}
			//
			uint8_t counter = 0;
			if ( which_table == 0 ) {
				counter = controls & COUNT_MASK;
				counter--;
				counter = min(counter,0);
				controls = (controls & ~COUNT_MASK) | (COUNT_MASK & counter);
			} else {
				counter = (controls>>EIGHTH) & COUNT_MASK;
				counter--;
				counter = min(counter,0);
				uint32_t update = (counter << EIGHTH) & HI_COUNT_MASK;
				controls = (controls & ~HI_COUNT_MASK) | update;
			}
			//
			controller->store((controls & FREE_BIT_MASK),std::memory_order_release);
		}


		/**
		 * unlock_counter
		*/

		void unlock_counter(uint32_t h_bucket) {
			//
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			//
			uint32_t controls = controller->load(std::memory_order_consume);
			//
			controller->store((controls & FREE_BIT_MASK),std::memory_order_release);
		}


		/**
		 * wait_if_unlock_bucket_counts
		*/

		bool wait_if_unlock_bucket_counts(uint32_t h_bucket,HHash **T_ref,uint32_t **buffer_ref,uint64_t **end_buffer_ref,uint8_t &which_table) {
			// ----
			HHash *T = _T1;
			hh_element *buffer		= _region_HV_1;
			hh_element *end_buffer	= _region_HV_1_end;
			which_table = 0;

			// ----
			pair<uint8_t,uint8_t> counts = this->bucket_counts(h_bucket);
			uint8_t count_1 = counts.first;		// favor the least full bucket ... but in case something doesn't move try both
			uint8_t count_2 = counts.second;

			if ( (count_2 >= COUNT_MASK) && (count_1 >= COUNT_MASK) ) {
				this->unlock_counter(h_bucket);
				return false;
			}

			//
			if ( count_2 < count_1 ) {
				T = _T2;
				which_table = 1;
				buffer = _region_HV_2;
				end_buffer = _region_HV_2_end;
			} else if ( count_2 == count_1 ) {
				uint8_t bit = pop_shared_bit();
				if ( bit ) {
					T = _T2;
					which_table = 1;
					buffer = _region_HV_2;
					end_buffer = _region_HV_2_end;
				}
			}

			*T_ref = T;
			*buffer_ref = buffer;
			*end_buffer_ref = end_buffer;
			return true;
			//
		}



		uint64_t add_key_value(uint32_t el_key,uint32_t h_bucket,uint32_t offset_value) {
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
			HHash *T = _T1;
			hh_element *buffer	= _region_HV_1;
			hh_element *ends	= _region_HV_1_end;
			uint8_t which_table = 0;

			//
			uint64_t loaded_key = UINT64_MAX;
			if ( wait_if_unlock_bucket_counts(h_bucket,&T,&buffer,&ends,which_table) ) {
				//
				uint64_t loaded_value = (((uint64_t)offset_value) << HALF) | el_key;
				bool put_ok = store_in_hash_bucket(T, h_bucket, loaded_value, buffer, ends);

				if ( put_ok ) {
					loaded_key = (((uint64_t)el_key) << HALF) | h_bucket; // LOADED
				} else {
					return(UINT64_MAX);
				}
				//
				this->bucket_count_incr(h_bucket,which_table);
				loaded_key = stamp_key(loaded_key,which_table);
			}

			return loaded_key;
		}



		// update
		// note that for this method the element key contains the selector bit for the even/odd buffers.
		// the hash value and location must alread exist in the neighborhood of the hash bucket.
		// The value passed is being stored in the location for the key...

		uint64_t update(uint32_t h_bucket, uint32_t el_key, uint32_t v_value) {
			if ( v_value == 0 ) return false;
			//
			uint8_t selector = ((el_key & HH_SELECT_BIT) == 0) ? 0 : 1;
			el_key = clear_selector_bit(el_key);
			//
			hh_element *buffer = (selector ? _region_HV_1 : _region_HV_2);
			hh_element *end = (selector ? _region_HV_1_end : _region_HV_2_end);
			//
			//uint64_t loaded_value = (((uint64_t)v_value) << HALF) | el_key;
			//
			this->bucket_lock(h_bucket);
			hh_element *storage_ref = get_ref(h_bucket, el_key, buffer, end);
			//
			if ( storage_ref != nullptr ) {
				storage_ref->_kv.key = el_key;
				storage_ref->_kv.value = v_value;
				this->unlock_counter(h_bucket);
				//storage_ref->_V = loaded_value;
				uint64_t loaded_key = (((uint64_t)el_key) << HALF) | h_bucket; // LOADED
				loaded_key = stamp_key(loaded_key,selector);
				return(loaded_key);
			} else {
				this->unlock_counter(h_bucket);
				return(UINT64_MAX);
			}
		}



		// get
		uint32_t get(uint64_t key) {
			uint32_t el_key = (uint32_t)((key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t hash = (uint32_t)(key & HASH_MASK);
			//
			return get(uint32_t bucket,uint32_t el_key);
		}


		// get
		uint32_t get(uint32_t h_bucket,uint32_t el_key) {  // full_hash,hash_bucket

			uint8_t selector = ((el_key & HH_SELECT_BIT) == 0) ? 0 : 1;
			el_key = clear_selector_bit(el_key);
			//
			hh_element *buffer = (selector ? _region_HV_1 : _region_HV_2);
			hh_element *end = (selector ? _region_HV_1_end : _region_HV_2_end);
			//
			this->bucket_lock(h_bucket);
			hh_element *storage_ref = get_ref(h_bucket, el_key, buffer, end);
			//
			if ( storage_ref == nullptr ) {
				this->unlock_counter(h_bucket);
				return UINT32_MAX;
			}
			uint32_t V = storage_ref->_kv.value;
			this->unlock_counter(h_bucket);
			//
			return V;
		}


		// del
		uint32_t del(uint64_t key) {
			//
			uint32_t el_key = (uint32_t)((key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t h_bucket = (uint32_t)(key & HASH_MASK);
			uint8_t selector = ((key & HH_SELECT_BIT) == 0) ? 0 : 1;
			//
			hh_element *buffer = (selector ? _region_HV_1 : _region_HV_2);
			hh_element *end = (selector ? _region_HV_1_end : _region_HV_2_end);
			//
			this->bucket_lock(h_bucket);
			uint32_t i = del_ref(h_bucket, el_key, buffer, ends);
			if ( i == UINT32_MAX ) {
				this->unlock_counter(h_bucket);
			} else {
				this->bucket_count_decr(h_bucket,selector);
			}
			return i;
		}



		// bucket probing
		//
		//	return a whole bucket... (all values)
		// 
		uint8_t get_bucket(uint32_t h_bucket, uint32_t xs[32]) {
			//
			uint8_t selector = ((h_bucket & HH_SELECT_BIT) == 0) ? 0 : 1;
			//
			hh_element *buffer = (selector ? _region_HV_1 : _region_HV_2);
			hh_element *end = (selector ? _region_HV_1_end : _region_HV_2_end);
			//
			hh_element *next = buffer + h_bucket;
			uint8_t count = 0;
			uint32_t i = 0;
			next = _succ_H_ref(next,i);
			while ( next != nullptr ) {
				xs[count++] = next->_kv.value;
				next = _succ_H_ref(next,(i + 1));
			}
			return count;	// no value  (values will always be positive, perhaps a hash or'ed onto a 0 value)
		}



	protected:			// these may be used in a test class...
 

		// operate on the hash bucket bits
		uint32_t _next(uint32_t H_, uint32_t i) {
  			uint32_t H = H_ & (~0 << i);
  			if ( H == 0 ) return UINT32_MAX;  // like -1
  			return countr_zero(H);	// return the count of trailing zeros
		}


		inline hh_element *check_end(hh_element *ptr,hh_element *buffer,hh_element *end) {
			if ( ptr >= end ) return buffer;
			return ptr;
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		// ----  _succ_H_ref
		// ----
		// ----

		hh_element *_succ_H_ref(hh_element *v_swap,uint32_t &i) {
			uint32_t H = v_swap->c_bits;
			if ( GET(H, i) ) return v_swap;
			i = _next(H, i);
			if  ( UINT32_MAX == i ) return nullptr;
			return (v_swap + i);
		}

		// ----  get_ref
		// ----
		// ----

		hh_element *get_ref(uint32_t h_bucket, uint32_t el_key, hh_element *buffer, hh_element *end) {
			//
			hh_element *next = buffer + h_bucket;
			next = check_end(next,buffer,end);
			uint32_t i = 0;
			next = _succ_H_ref(next,i);  // i by ref
			while ( next != nullptr ) {
				next = check_end(next,buffer,end);
				if ( el_key == next->_kv.key ) {
					return next;
				}
				next = _succ_H_ref(next,i);  // i by ref
			}
			return nullptr;
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// del_ref
		//  // caller will decrement count
		//
		uint32_t del_ref(uint32_t h_bucket, uint32_t el_key, hh_element *buffer, hh_element *end) {
			hh_element *next = buffer + h_bucket; next = check_end(next,buffer,end);
			uint32_t i = 0;
			next = _succ_H_ref(next,i);  // i by ref
			while ( next != nullptr ) {
				next = check_end(next,buffer,end);
				if ( el_key == next->_kv.key ) {
					auto H = next->c_bits;
					if ( (next->_kv.value == 0) || !GET(H, i) ) return;
					next->_V = 0;
					UNSET(H,i);
					next->c_bits = H;
					return i;
				}
				next = _succ_H_ref(next,i);  // i by ref
			}
			return UINT32_MAX;
		}


		// store_in_hash_bucket
		// Given a hash and a value, find a place for storing this pair
		// (If the buffer is nearly full, this can take considerable time)
		// Attempt to keep things organized in buckets, indexed by the hash module the number of elements
		//
		bool store_in_hash_bucket(HHash *T, uint32_t h_start, uint64_t v_passed,const hh_element *buffer, const hh_element *end_buffer) {
			//
			uint32_t N = this->_max_n;
			if ( v_passed == 0 ) return(false);  // zero indicates empty...
			//
			h_start = h_start % N;  // scale the hash .. make sure it indexes the array...
			hh_element *hash_ref = (hh_element *)(buffer) + h_start;
			hh_element *v_ref = hash_ref;
			hh_element *v_swap = nullptr;
			hh_element *v_swap_base = nullptr;

			uint32_t D = _circular_first_empty_from_ref(buffer, end_buffer, &v_ref);  // a distance starting from h (if wrapped, then past N)
			if ( D == UINT32_MAX ) return(false); // the positions in the entire buffer are full.

			// If we got to here, v_ref is now pointing at an empty spot (increasing mod N from h_start), first one found

			//
			// if the hole is father way from h_start than the neighborhood... then hopscotch
			//
			auto h_d = 0;
			v_swap = v_ref;
			while ( D >= NEIGHBORHOOD ) { // D is how far. If (D + h_start) > N, v_ref will be < hash_ref ... (v_ref < hash_ref)
				//
				// find something closer to hash_ref and can be traded with the hole
				// (since the hole will be in the neighborhood of something closer.)
				// We imagine that the hole is in the tail end of a neighborhood, and swapping will
				// keep it in the neighborhood but closer to the hash_ref.
				uint32_t j = _hop_scotch_refs(&v_swap, buffer, end_buffer);   // j is the hole location in the neighborhood (for bits)
				if ( j == 0 ) return(false); // could not find anything that could move. (Frozen at this point..)

				// At this point, v_swap points to a hash bucket/value that provides a useful bitmask (nice neighborhood)
				// But, v_swap itself may not point to the actual position to swap...
				// So, advance the pointer to the first spot that the hash v_swap location owns (in the neighborly sense).
				// Then swap with that position.

				// found a position that can be moved... (offset from h <= d closer to the neighborhood)
				v_swap_base = v_swap;
				uint32_t i = 0;
				v_swap = _succ_H_ref(v_swap,i);				// i < j is the swap position in the neighborhood (for bits)
				v_swap = check_end(v_swap,buffer,end);
				if ( v_swap == nullptr ) return false;
				//
				_swapper(v_swap_base,v_swap,v_ref,i,j); // take care of the bits as well...
				//
				if ( v_swap > hash_ref ) {
					D = (v_swap - hash_ref);
				} else {
					D = (end_buffer - hash_ref) + (v_swap - buffer);
				}
			}
			//
			//
			v_swap->_V = v_passed;
			SET(hash_ref->c_bits,D);

			return(true);
		}



		void _reset_bits(uint32_t &bits,uint32_t i, uint32_t j) {
			UNSET(bits, i);
			SET(bits, j);
		}



		void _swapper(hh_element *v_swap_base,hh_element *v_swap,hh_element *v_ref,uint32_t i, uint32_t j) {
			v_ref->_V = v_swap->_V;
			v_swap->_V = 0;
			_reset_bits(v_swap_base->c_bits,i,j);
		}


		/**
		 *  _probe -- search for a free space within a bucket
		 * 		h : the bucket starts at h (an offset in _region_V)
		 * 
		 *  zero in the value buffer means no entry, because values do not start at zero for the offsets 
		 *  (free list header is at zero if not allocated list)
		 * 
		 * `_probe` wraps around search before h (bucket index) returns the larger value N + j if the wrap returns a position
		 * 
		 * @returns {uint32_t} distance of the bucket from h
		*/
		uint32_t _circular_first_empty_from_ref(const hh_element *buffer, const hh_element *end_buffer, hh_element **h_ref_ref) {   // value probe ... looking for zero
			// // 
			// search in the bucket
			hh_element *h_ref = *h_ref_ref;
			hh_element *vb_probe = h_ref;
			//
			while ( vb_probe < end_buffer ) { // search forward to the end of the array (all the way even it its millions.)
				uint64_t V = vb_probe->_V;
				if ( V == 0 ) {
					*h_ref_ref = vb_probe;
					uint32_t dist_from_h = (uint32_t)(vb_probe - h_ref);
					return dist_from_h;			// look no further
				}
				vb_probe++;
			}
			// WRAP CIRCULAR BUFFER
			// look for anything starting at the beginning of the segment
			// wrap... start searching from the start of all data...
			vb_probe = v_buffer;
			while ( vb_probe < h_ref ) {
				uint64_t V = vb_probe->_V;
				if ( V == 0 ) {
					uint32_t N = this->_max_n;
					*h_ref_ref = vb_probe;
					uint32_t dist_from_h = N + (uint32_t)(vb_probe - h_ref);	// (vb_end - h_ref) + (vb_probe - v_buffer) -> (vb_end  - v_buffer + vb_probe - h_ref)
					return dist_from_h;	// look no further (notice quasi modular addition)
				}
				vb_probe++;
			}
			// BUFFER FULL
			return UINT32_MAX;  // this will be taken care of by a modulus in the caller
		}

		/**
		 * Look at one bit pattern after another from distance `d` shifted 'down' to h by K (as close as possible).
		 * Loosen the restriction on the distance of the new buffer until K (the max) away from h is reached.
		 * If something within K (for swapping) can be found return it, otherwise 0 (indicates frozen)
		 * 
		 * If h_d is empty, no one stored a value with this bucket as the anchor of an neighborhood.
		*/

		uint32_t _hop_scotch_refs(hh_element **v_swap_ref, hh_element *buffer, hh_element *end_buffer) {  // return an index
			uint32_t K = NEIGHBORHOOD;
			//
			hh_element *v_swap = *v_swap_ref;
			hh_element *v_swap_original = v_swap;
			v_swap -= K;
			if ( v_swap < beg ) {
				vswap = end_buffer - (K - beg + v_swap);
			}
			for ( uint32_t i = (K - 1); i > 0; --i ) {
				v_swap++;
				v_swap = check_end(v_swap,buffer,end);
				uint32_t H = v_swap->c_bits; //[hi];   // CONTENTION
				if ( (H != 0) && (((uint32_t)countr_zero(H)) < i) ) {
					*v_swap_ref = v_swap;
					return (v_swap - v_swap_original);  // where the hole is in the neighborhood
				}
			}
			return 0;
		}



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


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
		//
		uint8_t		 					*_region;
		uint8_t		 					*_endof_region;
		//
		HHash							*_T1;
		HHash							*_T2;
		hh_element		 				*_region_HV_1;
		hh_element		 				*_region_HV_2;
		hh_element		 				*_region_HV_1_end;
		hh_element		 				*_region_HV_2_end;
		//
		// ---- These two regions are interleaved in reality
		uint32_t		 				*_region_C;
		// threads ...

		atomic<uint32_t> 				*_random_gen_value;

};


#endif // _H_HOPSCOTCH_HASH_SHM_