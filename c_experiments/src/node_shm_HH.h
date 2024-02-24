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



// ---- ---- ---- ---- ---- ----  HHash
// HHash <- HHASH


class HH_map : public HMap_interface, public Random_bits_generator<> {
	//
	public:

		// LRU_cache -- constructor
		HH_map(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count,bool am_initializer = false) {
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
		void setup_region(bool am_initializer,uint8_t header_size,uint32_t max_count) {
			// ----
			uint8_t *start = _region;
			HHash *T = (HHash *)start;
			//
			_T1 = T;   // keep the reference handy
			//
			uint32_t v_regions_size = (sizeof(uint64_t)*max_count);  // should be one half of the total elements configured
			uint32_t h_regions_size = (sizeof(uint32_t)*max_count);
			uint32_t c_regions_size = (sizeof(uint32_t)*max_count);

			auto v_offset_1 = header_size;
			auto h_offset_1 = (v_offset_1 + v_regions_size);
			auto c_offset_1 = (h_offset_1 + h_regions_size);
			auto next_hh_offset = (c_offset_1 + c_regions_size);

			// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

			T->_V_Offset = v_offset_1;
			T->_H_Offset = h_offset_1;
			T->_C_Offset = c_offset_1;

			// # 1
			// ----
			if ( am_initializer ) {
				T->_count = 0;
				T->_max_n = max_count;
				T->_neighbor = FLS(max_count - 1);
				T->_control_bits = 0;
			} else {
				max_count = T->_max_n;	// just in case
			}

			// # 1
			//
			_region_V_1 = (uint64_t *)(start + v_offset_1);  // start on word boundary
			_region_H_1 = (uint32_t *)(start + h_offset_1);
			_region_C_1 = (uint32_t *)(start + c_offset_1);

			//
			if ( am_initializer ) {
				if ( check_end(start + header_size) && check_end(start + header_size + (h_regions_size + v_regions_size)) ) {
					memset((void *)(start + header_size),0,(h_regions_size + v_regions_size));
				} else {
					throw "hh_map (1) sizes overrun allocated region determined by region_sz";
				}
				if ( check_end(start + c_offset_1) && check_end(start + c_offset_1 + c_regions_size ) ) {
					memset((void *)(start + c_offset_1), 0, c_regions_size);
				} else {
					throw "hh_map (2) sizes overrun allocated region determined by region_sz";
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
				T->_neighbor = FLS(max_count - 1);
				T->_control_bits = 1;
			} else {
				max_count = T->_max_n;	// just in case
			}

			// # 2
			//
			_region_V_2 = (uint64_t *)(start + v_offset_1);  // start on word boundary
			_region_H_2 = (uint32_t *)(start + h_offset_1);
			_region_C_2 = (uint32_t *)(start + c_offset_1);

			auto v_offset_2 = next_hh_offset + header_size;
			auto h_offset_2 = (v_offset_2 + v_regions_size);
			auto c_offset_2 = (h_offset_2 + h_regions_size);

			T->_V_Offset = v_offset_2;
			T->_H_Offset = h_offset_2;
			T->_C_Offset = c_offset_2;

			//
			if ( am_initializer ) {
				if ( check_end(start + header_size) && check_end(start + header_size + (h_regions_size + v_regions_size)) ) {
					memset((void *)(start + header_size),0,(h_regions_size + v_regions_size));
				} else {
					throw "hh_map (3) sizes overrun allocated region determined by region_sz";
				}
				if ( check_end(start + c_offset_1) && check_end(start + c_offset_1 + c_regions_size,true) ) {
					memset((void *)(start + c_offset_1), 0, c_regions_size);
				} else {
					throw "hh_map (4) sizes overrun allocated region determined by region_sz";
				}
			}
			//
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

			uint32_t v_regions_size = (sizeof(uint64_t)*max_count);  // should be one half of the total elements configured
			uint32_t h_regions_size = (sizeof(uint32_t)*max_count);
			uint32_t c_regions_size = (sizeof(uint32_t)*max_count);

			auto v_offset_1 = header_size;
			auto h_offset_1 = (v_offset_1 + v_regions_size);
			auto c_offset_1 = (h_offset_1 + h_regions_size);
			auto next_hh_offset = c_offset_1 + c_regions_size;
			//
			uint32_t predict = next_hh_offset*2;
			//
			return predict;
		} 



		// THREAD CONTROL

		void tick() {
			nanosleep(&request, &remaining);
		}

		

	// uint32_t *buffer = _region_H;
	// uint64_t *v_buffer = _region_V;

		/**
		 * 
		*/
		pair<uint16_t,uint16_t> bucket_counts(uint32_t h_bucket) {   // where are the bucket counts??
			//
			pair<uint16_t,uint16_t> counts;
			uint8_t *start = _region;

			HHash *T_1 = _T1;  // same for all offsets from start
			uint32_t c_offset = T_1->_C_Offset;
			uint32_t *controllers = (uint32_t *)(start + sizeof(struct HHASH) + c_offset);
			//
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			//
			uint32_t controls = controller->load(std::memory_order_consume);
			while ( controls & HOLD_BIT_MASK ) {  // while some other process is using this count bucket
				controls = controller->load(std::memory_order_consume);
			}
			//
			while ( !controller->compare_exchange_weak(controls,(controls | HOLD_BIT_MASK)) && !(controls & HOLD_BIT_MASK) );
			//
			counts.first = controls & COUNT_MASK;
			counts.second = (controls>>QUARTER) & COUNT_MASK;
			//
			return (counts);
		}


		/**
		 * 
		*/
		void bucket_count_incr(uint32_t h_bucket,uint8_t which_thread) {
			//
			uint8_t *start = _region;
			HHash *T_1 = _T1;
			//
			uint32_t c_offset = T_1->_C_Offset;
			uint32_t *controllers = (uint32_t *)(start + sizeof(struct HHASH) + c_offset);
			//
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			//
			uint32_t controls = controller->load(std::memory_order_consume);
			if ( !(controls & HOLD_BIT_MASK) ) {	// should be the only one able to get here on this bucket.
				this->_status = -1;
				return;
			}
			//
			uint16_t counter = 0;
			if ( which_thread == 0 ) {
				counter = controls & COUNT_MASK;
				counter++;
				controls = (controls & ~COUNT_MASK) | (COUNT_MASK & counter);
			} else {
				counter = (controls>>QUARTER) & COUNT_MASK;
				counter++;
				uint32_t update = (counter << QUARTER) & HI_COUNT_MASK;
				controls = (controls & ~HI_COUNT_MASK) | update;
			}
			//
			controller->store(controls & FREE_BIT_MASK,std::memory_order_release);
		}



		bool ok(void) {
			return(this->_status);
		}

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


		//  store    //  hash_bucket, full_hash,   el_key = full_hash
		uint64_t store(uint32_t hash_bucket, uint32_t el_key, uint32_t v_value) {
			if ( v_value == 0 ) return false;
			//
			uint8_t selector = ((el_key & HH_SELECT_BIT) == 0) ? 0 : 1;
			el_key = clear_selector_bit(el_key);
			HHash *T = (selector ? _T1 : _T2);
			uint32_t *buffer = (selector ? _region_H_1 : _region_H_2);
			uint64_t *v_buffer = (selector ? _region_V_1 : _region_V_2);
			//
			uint64_t loaded_value = (((uint64_t)v_value) << HALF) | el_key;
			bool put_ok = put_hh_hash(T, hash_bucket, loaded_value, buffer, v_buffer);
			if ( put_ok ) {
				uint64_t loaded_key = (((uint64_t)el_key) << HALF) | hash_bucket; // LOADED
				loaded_key = stamp_key(loaded_key,selector);
				return(loaded_key);
			} else {
				return(UINT64_MAX);
			}
		}

		// get
		uint32_t get(uint64_t key) {
			uint8_t selector = ((key & HH_SELECT_BIT) == 0) ? 0 : 1;
			key = clear_selector_bit64(key);
			HHash *T = (selector ? _T1 : _T2);
			uint32_t *buffer = (selector ? _region_H_1 : _region_H_2);
			uint64_t *v_buffer = (selector ? _region_V_1 : _region_V_2);
			return get_hh_map(T, key, buffer, v_buffer);
		}


		// get
		uint32_t get(uint32_t key,uint32_t bucket) {  // full_hash,hash_bucket
			uint8_t selector = ((key & HH_SELECT_BIT) == 0) ? 0 : 1;
			key = clear_selector_bit(key);
			HHash *T = (selector ? _T1 : _T2);
			uint32_t *buffer = (selector ? _region_H_1 : _region_H_2);
			uint64_t *v_buffer = (selector ? _region_V_1 : _region_V_2);
			return (uint32_t)(get_hh_set(T, key, bucket, buffer, v_buffer) >> HALF);  // the value is in the top half of the 64 bits.
		}


		// bucket probing
		// 
		uint8_t get_bucket(uint32_t h, uint32_t xs[32]) {
			uint8_t selector = ((h & HH_SELECT_BIT) == 0) ? 0 : 1;
			h = clear_selector_bit(h);
			HHash *T = (selector ? _T1 : _T2);
			uint32_t *buffer = (selector ? _region_H_1 : _region_H_2);
			uint64_t *v_buffer = (selector ? _region_V_1 : _region_V_2);
			uint8_t count = 0;
			uint32_t i = _succ_hh_hash(T, h, 0, buffer);
			while ( i != UINT32_MAX ) {
				uint64_t x = get_val_at_hh_hash(T, h, i, v_buffer);  // get ith value matching this hash (collision)
				xs[count++] = (uint32_t)((x >> HALF) & HASH_MASK);
				i = _succ_hh_hash(T, h, i + 1, buffer);  // increment i in some sense (skip unallocated holes)
			}
			return count;	// no value  (values will always be positive, perhaps a hash or'ed onto a 0 value)
		}


		// del
		uint32_t del(uint64_t key) {
			uint8_t selector = ((key & HH_SELECT_BIT) == 0) ? 0 : 1;
			HHash *T = (selector ? _T1 : _T2);
			uint32_t *buffer = (selector ? _region_H_1 : _region_H_2);
			uint64_t *v_buffer = (selector ? _region_V_1 : _region_V_2);
			return del_hh_map(T, key, buffer, v_buffer);
		}

		// clear
		void clear(void) {
			if ( _initializer ) {
				uint8_t sz = sizeof(HHash);
				uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
				this->setup_region(_initializer,header_size,_max_count);
			}
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


	void wakeup_random_genator(uint8_t which_region) {   // 
		// regenerate_shared(which_region);
		_random_gen_value->store(which_region);
	}

	void thread_sleep([[maybe_unused] ]uint8_t ticks) {

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

		pair<uint16_t,uint16_t> counts = this->bucket_counts(h_bucket);
		uint16_t count_1 = counts.first;		// favor the least full bucket ... but in case something doesn't move try both
		uint16_t count_2 = counts.second;
		//
		HHash *T = _T1;
		uint32_t *buffer = _region_H_1;
		uint64_t *v_buffer = _region_V_1;
		uint8_t which_table = 0;
		//
		if ( count_2 < count_1 ) {
			T = _T2;
			which_table = 1;
			buffer = _region_H_2;
			v_buffer = _region_V_2;
		} else if ( count_2 == count_1 ) {
			uint8_t bit = pop_shared_bit();
			if ( bit ) {
				T = _T2;
				which_table = 1;
				buffer = _region_H_2;
				v_buffer = _region_V_2;
			}
		}
		//
		uint64_t loaded_key = UINT64_MAX;
		uint64_t loaded_value = (((uint64_t)offset_value) << HALF) | el_key;
		bool put_ok = put_hh_hash(T, h_bucket, loaded_value, buffer, v_buffer);
		if ( put_ok ) {
			loaded_key = (((uint64_t)el_key) << HALF) | h_bucket; // LOADED
		} else {
			return(UINT64_MAX);
		}

		this->bucket_count_incr(h_bucket,which_table);
		loaded_key = stamp_key(loaded_key,which_table);

		return loaded_key;
	}


	private:
 

		// operate on the hash bucket bits
		uint32_t _next(uint32_t _H, uint32_t i) {
  			uint32_t H = _H & (~0 << i);
  			if ( H == 0 ) return UINT32_MAX;  // like -1
  			return FFS(H);	// return the count of trailing zeros
		}

		// operate on the hash bucket bits  -- find the next set bit
		uint32_t _succ(uint32_t h_bucket, uint32_t i, uint32_t *bit_mask_buckets) {
			uint32_t H = bit_mask_buckets[h_bucket];	// the one for the bucket
   			if ( GET(H, i) ) return i;			// look at the control bits of the test position... see if the position is set.
  			return _next(H, i);					// otherwise, what's next...
		}

		// operate on the hash bucket bits .. take into consideration that the bucket range is limited
		uint32_t _succ_hh_hash(HHash *T, uint32_t h_bucket, uint32_t i, uint32_t *buffer) {
			//
			if ( i == 32 ) return(UINT32_MAX);  // all the bits in the bucket have been checked
			uint32_t N = T->_max_n;
			uint32_t h = (h_bucket % N);
			return _succ(h, i, buffer);
			//
		}


		void del_hh_hash(HHash *T, uint32_t h, uint32_t i, uint32_t *buffer, uint64_t *v_buffer) {
			//
			uint32_t N = T->_max_n;
			h = (h % N);
			uint32_t j = MOD(h + i, N);  // the offset relative to the original hash bucket + bucket position = absolute address
			//
			uint32_t V = v_buffer[j];
			uint32_t H = buffer[h];		// the control bit in the original hash bucket
			//
			if ( (V == 0) || !GET(H, i)) return;
			//
			// reset the hash machine
			v_buffer[j] = 0;	// clear the value slot
			UNSET(H,i);			// remove relative position from the hash bucket
			buffer[h] = H;		// store it
			// lower the count
			T->_count--;
		}

		// put_hh_hash
		// Given a hash and a value, find a place for storing this pair
		// (If the buffer is nearly full, this can take considerable time)
		// Attempt to keep things organized in buckets, indexed by the hash module the number of elements
		//
		bool put_hh_hash(HHash *T, uint32_t h, uint64_t v, uint32_t *buffer, uint64_t *v_buffer) {
			//
			uint32_t N = T->_max_n;
			if ( (T->_count == N) || (v == 0) ) return(false);  // FULL
			//
			h = h % N;  // scale the hash .. make sure it indexes the array...
			uint32_t d = _probe(T, h, v_buffer);  // a distance starting from h (if wrapped, then past N)
			if ( d == UINT32_MAX ) return(false); // the positions in the entire buffer are full.
	//cout << "put_hh_hash: d> " << d;
			//
			uint32_t K =  T->_neighbor;
			while ( d >= K ) {						// the number may be bigger than K. if wrapping, then bigger than N. 2N < UINT32_MAX.
				uint32_t hd = MOD( (h + d), N );	// d is allowed to wrap around.
				uint32_t z = _hop_scotch(T, hd, buffer);	// hop scotch back to a moveable positions
	//cout << " put_hh_hash: z> " << z;
				if ( z == 0 ) return(false);			// could not find anything that could move. (Frozen at this point..)
				// found a position that can be moved... (offset from h <= d closer to the neighborhood)
				uint32_t j = z;
				z = MOD((N + hd - z), N);		// hd - z is an (offset from h) < h + d or (h + z) < (h + d)  ... see hopscotch 
				uint32_t i = _succ(z, 0, buffer);		// either this is moveable or there's another one. (checking the bitmap ...)
				_swap(z, i, j, T, buffer, v_buffer);				// swap bits and values between i and j offsets within the bucket h
				d = MOD( (N + z + i - h), N );  // N + z - (h - i) ... a new distance, should be less than before
			}
			//
			//
			uint32_t hd = MOD( (h + d), N );  // store the value
			v_buffer[hd] = v;
	//cout << " put_hh_hash: hd> " << hd  << " val: "  << v;

			//
			uint32_t H = buffer[h]; // update the hash machine
			SET(H,d);
			buffer[h] = H;
	//cout << " put_hh_hash: h> " << h  << " H: "  << H << endl;

			// up the count 
			T->_count++;
			return(true);
		}

		/**
		 * Swap bits and values 
		 * The bitmap is for the h'th bucket. And, i and j are bits within bitmap i'th and j'th.
		 * 
		 * i and j are used later as offsets from h when doing the value swap. 
		*/
		void _swap(uint32_t h, uint32_t i, uint32_t j, HHash *T, uint32_t *buffer, uint64_t *v_buffer) {
			//
			uint32_t H = buffer[h];
			UNSET(H, i);
			SET(H, j);
			buffer[h] = H;
			//
			uint32_t N = T->_max_n;
			i = MOD((h + i), N);		// offsets from the moveable position (i will often be 0)
			j = MOD((h + j), N);
			//
			uint64_t v = v_buffer[i];	// swap
			v_buffer[i] = 0;
			v_buffer[j] = v;
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
		uint32_t _probe(HHash *T, uint32_t h, uint64_t *v_buffer) {   // value probe ... looking for zero
			// // 
			uint32_t N = T->_max_n;		// upper bound (count of elements in buffer)
			//
			// search in the bucket
			uint64_t *vb_probe = v_buffer;
			vb_probe += h;
			for ( uint32_t i = h; i < N; ++i ) {			// search forward to the end of the array (all the way even it its millions.)
				uint64_t V = *vb_probe++;	// is this an empty slot? Usually, when the table is not very full.
				if ( V == 0 ) return (i-h);			// look no further
			}
			//
			// look for anything starting at the beginning of the segment
			// wrap... start searching from the start of all data...
			vb_probe = v_buffer;
			for ( uint32_t j = 0; j < h ; ++j ) {
				uint64_t V = *vb_probe++;	// is this an empty slot? Usually, when the table is not very full.
				if ( V == 0 ) return (N + j - h);	// look no further (notice quasi modular addition)
			}
			return UINT32_MAX;  // this will be taken care of by a modulus in the caller
		}

		/**
		 * Look at one bit pattern after another from distance `d` shifted 'down' to h by K (as close as possible).
		 * Loosen the restriction on the distance of the new buffer until K (the max) away from h is reached.
		 * If something within K (for swapping) can be found return it, otherwise 0 (indicates frozen)
		*/
		uint32_t _hop_scotch(HHash *T, uint32_t hd, uint32_t *buffer) {  // return an index
			uint32_t N = T->_max_n;
			uint32_t K =  T->_neighbor;
			for ( uint32_t i = (K - 1); i > 0; --i ) {
				uint32_t hi = MOD(N + hd - i, N);			// hop backwards towards the original hash position (h)...
				uint32_t H = buffer[hi];
				if ( (H != 0) && (((uint32_t)FFS(H)) < i) ) return i;	// count of trailing zeros less than offset from h
			}
			return 0;
		}


		/**
		 * Returns the value (for this use an offset into the data storage area.)
		*/
		uint64_t get_val_at_hh_hash(HHash *T, uint32_t h_bucket, uint32_t i, uint64_t *v_buffer) {
			uint32_t offset = (h_bucket + i);		// offset from the hash position...
			uint32_t N = T->_max_n;
			uint32_t j = (offset % N);		// if wrapping around
			return(v_buffer[j]);			// return value
		}


		// ---- ---- ---- ---- ---- ---- ----
		// the actual hash key is stored in the lower part of the value pair.
		// the top part is an index into an array of objects.
		bool _cmp(uint64_t k, uint64_t x) {		// compares the bottom part of the words
			bool eq = ((HASH_MASK & k) == (HASH_MASK & x));
			return(eq); //
		}


		// SET OPERATION
		// originally called hunt for a set type...
		// In this applicatoin k is a value comparison... and the k value is an offset into an array of stored objects 
		// walk through the list of position occupied by bucket members. (Those are the ones with the positional bit set.)
		//
		uint64_t hunt_hash_set(HHash *T, uint32_t h_bucket, uint64_t key_null, bool kill, uint32_t *buffer, uint64_t *v_buffer) {
			uint32_t i = _succ_hh_hash(T, h_bucket, 0, buffer);   // i is the offset into the hash bucket.
			while ( i != UINT32_MAX ) {
				// x is from the value region..
				uint64_t x = get_val_at_hh_hash(T, h_bucket, i, v_buffer);  // get ith value matching this hash (collision)
				if ( _cmp(key_null, x) ) {		// compare the discerning hash part of the values (in the case of map, hash of the stored value)
					if (kill) del_hh_hash(T, h_bucket, i, buffer, v_buffer);
					return x;   // the value is a pair of 32 bit words. The top 32 bits word is the actual value.
				}
				i = _succ_hh_hash(T, h_bucket, (i + 1), buffer);  // increment i in some sense (skip unallocated holes)
			}
			return 0;		// no value  (values will always be positive, perhaps a hash or'ed onto a 0 value)
		}


		// The 64 bit key_null is a hash-value pair, with the hash (pattern match) in the bottom 32 bits.
		// the top 32 bits will contain a value if it is stored. Otherwise, it will be zero (no value stored).
		// In the main use of this code, the value will be an offset into an array of objects.

		uint64_t get_hh_set(HHash *T, uint32_t hbucket, uint32_t key, uint32_t *buffer, uint64_t *v_buffer) {  // T, full_hash, hash_bucket
			uint64_t zero = 0;
			uint64_t key_null = (zero | (uint64_t)key); // hopefully this explains it... top 32 are zero (hence no value represented)
//cout << "get_hh_set: key_null: " << key_null << " hash: " << hash <<  endl;
			bool flag_delete = false;
			return hunt_hash_set(T, hbucket, key_null, flag_delete, buffer, v_buffer);
		}


		bool put_hh_set(HHash *T, uint32_t h, uint64_t key_val, uint32_t *buffer, uint64_t *v_buffer) {
			if ( key_val == 0 ) return 0;		// cannot store zero values
			if ( get_hh_set(T, h, (uint32_t)key_val, buffer, v_buffer) != 0 ) return (true);  // found, do not duplicate ... _cmp has been called
			if ( put_hh_hash(T, h, key_val,buffer,v_buffer) ) return (true); // success
			// not implementing resize
			return (false);
		}


		uint64_t del_hh_set(HHash *T, uint32_t hbucket, uint32_t key, uint32_t *buffer, uint64_t *v_buffer) {
			uint64_t zero = 0;
			uint64_t key_null = (zero | (uint64_t)key); // hopefully this explains it... 
			bool flag_delete = true;
			return hunt_hash_set(T, hbucket, key_null, flag_delete, buffer, v_buffer); 
		}

		// note: not implementing resize since the size of the share segment is controlled by the application..

		// MAP OPERATION

		// loaded value -- value is on top (high word) and the index (top of loaded hash) is on the

		// T, hash_bucket, el_key, v_value   el_key == full_hash

		uint64_t put_hh_map(HHash *T, uint32_t hash_bucket, uint32_t full_hash, uint32_t value, uint32_t *buffer, uint64_t *v_buffer) {
			if ( value == 0 ) return false;
//cout <<  " put_hh_map: loaded_value [value] " << value << " loaded_value [index] " << index;
			uint64_t loaded_value = (((uint64_t)value) << HALF) | full_hash;
//cout << " loaded_value: " << loaded_value << endl;
			bool put_ok = put_hh_set(T, hash_bucket, loaded_value, buffer, v_buffer);
			if ( put_ok ) {
				uint64_t loaded_key = (((uint64_t)full_hash) << HALF) | hash_bucket; // LOADED
				return(loaded_key);
			} else {
				return(UINT64_MAX);
			}
		}

		uint32_t get_hh_map(HHash *T, uint64_t key, uint32_t *buffer, uint64_t *v_buffer) { 
			 // UNLOADED
			uint32_t element_diff = (uint32_t)((key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t hash = (uint32_t)(key & HASH_MASK);
//cout << "get_hh_map>> element_diff: " << element_diff << " hash: " << hash << " ";
//cout << " _region_H[hash] " << _region_H[hash] << " _region_V[hash]  "  << _region_V[hash]  << endl;
			return (uint32_t)(get_hh_set(T, hash, element_diff,buffer,v_buffer) >> HALF); 
		}

		uint32_t del_hh_map(HHash *T, uint64_t key, uint32_t *buffer, uint64_t *v_buffer) {
			 // UNLOADED
			uint32_t element_diff = (uint32_t)((key >> HALF) & HASH_MASK);
			uint32_t hash = (uint32_t)(key & HASH_MASK);
			return (uint32_t)(del_hh_set(T, hash, element_diff,buffer,v_buffer) >> HALF);
		}

		// ---- ---- ---- ---- ---- ---- ----
		//
		bool							_status;
		const char 						*_reason;
		//
		bool							_initializer;
		uint32_t						_max_count;
		uint8_t		 					*_region;
		uint8_t		 					*_endof_region;
		//
		HHash							*_T1;
		HHash							*_T2;
		uint32_t		 				*_region_H_1;
		uint64_t		 				*_region_V_1;
		uint32_t		 				*_region_C_1;
		//
		uint32_t		 				*_region_H_2;
		uint64_t		 				*_region_V_2;
		uint32_t		 				*_region_C_2;
		// threads ...

		atomic<uint32_t> 				*_random_gen_value;

};


#endif // _H_HOPSCOTCH_HASH_SHM_