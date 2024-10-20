#ifndef _H_SPARSE_SLABS_HASH_SHM_
#define _H_SPARSE_SLABS_HASH_SHM_

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
#include "slab_provider.h"

#include "worker_waiters.h"


/**
 * API exposition
 * 
 * 1. `prepare_for_add_key_value_known_refs` 
 * 				--	start the intake of new elements. Assign a table slice and get memory references
 * 					for use in the call to `add_key_value_known_refs`
 * 
 * 2. `add_key_value_known_refs`
 * 				--	must be set up by the caller with a call to `prepare_for_add_key_value_known_refs`
 * 					Given the slice has been selected, calls `_adder_bucket_queue_release`
 * 
 * 3. `update`	--	
 * 4. `del`		--	
 * 
 * 5. `get`		--	
 * 
*/



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


#include "hh_queues_and_states.h"



// ---- ---- ---- ---- ---- ----  HHash
// HHash <- HHASH

/**
 * CLASS: SS_map
 * 
 * Sparse Slabs
 * 
*/


template<const uint32_t NEIGHBORHOOD = 32>
class SSlab_map : public RestoreAndCropWaiters, public Random_bits_generator<>, public HMap_interface {

	public:

		// SSlab_map LRU_cache -- constructor
		SSlab_map(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) {
			initialize_all(region, seg_sz, max_element_count, num_threads, am_initializer);
		}

		virtual ~SSlab_map() {
		}

	public: 


		// LRU_cache -- constructor
		void initialize_all(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) {
			_reason = "OK";
			//
			_region = region;
			_endof_region = _region + seg_sz;
			//
			_num_threads = num_threads;

			initialize_waiters();
			//
			_status = true;
			_initializer = am_initializer;
			_max_count = max_element_count;
			//
			uint8_t sz = sizeof(HHash);
			uint8_t header_size = (sz  + (sz % sizeof(uint64_t)));
			//
			// initialize from constructor
			setup_region(am_initializer,header_size,(max_element_count/2),num_threads);
			//
			initialize_randomness();
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

			initialize_random_waiters((atomic_flag *)start);
			_random_gen_region = (atomic<uint32_t> *)(start + sizeof(atomic_flag)*2);

			// start is now passed the atomics...
			start = (uint8_t *)(_random_gen_region + 1);

			HHash *T = (HHash *)(start);

			//
			_max_n = max_count;

			//
			_T0 = T;   // keep the reference handy
			//
			uint32_t vh_region_size = (sizeof(hh_element)*max_count);
			//uint32_t c_regions_size = (sizeof(uint32_t)*max_count);

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
			_region_HV_0 = (sp_element *)(start + hv_offset_past_header_from_start);  // start on word boundary
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
			_region_HV_1 = (sp_element *)(_region_HV_0_end);  // start on word boundary
			_region_HV_1_end = _region_HV_1 + max_count;

			auto hv_offset_2 = next_hh_offset + header_size;

			T->_HV_Offset = hv_offset_2;  // from start
			T->_C_Offset = hv_offset_2 + c_offset;  // from start
			//

		// threads ...
			auto proc_regions_size = num_threads*sizeof(sp_proc_descr);
			_process_table = (sp_proc_descr *)(_region_HV_1_end);
			_end_procs = _process_table + num_threads;
			//
			if ( am_initializer ) {
				//
				if ( check_end((uint8_t *)_region_HV_1) && check_end((uint8_t *)_region_HV_1_end) ) {
					memset((void *)(_region_HV_1),0,vh_region_size);
				} else {
					throw "hh_map (2) sizes overrun allocated region determined by region_sz";
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
		 * 
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
			uint32_t proc_region_size = num_threads*sizeof(sp_proc_descr);
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
		 * bucket_at(h_start, buffer, end)  -- check wrap
		*/

		inline sp_element *bucket_at(hh_element *buffer,uint32_t h_start) {
			return (sp_element *)(buffer) + h_start;
		}

		// use the index to get the element and wrap if necessary
		inline sp_element *bucket_at(uint32_t h_start, sp_element *buffer, sp_element *end) {
			sp_element *el = buffer + h_start;
			el =  el_check_end_wrap(el,buffer,end);
			return el;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


/**
 * SUB: SliceSelector  -- could be a subclass, but everything is one class.
 * 
 * Retains reference to the data structures and provides methods interfacing stashes.
 * Provides methods for managing the selection of a slice based upon bucket membership size.
 * Also, this class introduces methods that provide final implementations of the random bits for slice selections.
 * 
*/


		// RANDOMNESS -- randomness is avaiable (and updated by another thread) for deciding ties 
		// between slices. There are situations encountered by processes here that may interfere
		// with providing perfect randomness for tie breaking, but there is no need for perfection.
		// This needs enough randomness to keep bucket sizes minimal. 
	
		/**
		 * 
		 */
		void initialize_randomness(void) {
			_random_gen_region->store(0);
		}
		

		// * set_random_bits
		/**
		*/
		// 4*(_bits.size() + 4*sizeof(uint32_t))

		void set_random_bits(void *shared_bit_region) override {
			uint32_t *bits_for_test = (uint32_t *)(shared_bit_region);
			for ( int i = 0; i < _max_r_buffers; i++ ) {
				set_region(bits_for_test,i);    // inherited method set the storage (otherwise null and not operative)
				regenerate_shared(i);
				bits_for_test += _bits.size() + 4*sizeof(uint32_t);  // 
			}
		}


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
		void share_lock(void) override {
			randoms_worker_lock();
		}


		/**
		 * share_unlock 
		 * 
		 * override the lock for shared random bits (in parent this is a no op)
		*/
		void share_unlock(void) override {
			randoms_worker_unlock();
		}


		/**
		 * random_generator_thread_runner 
		 * 
		 * override the lock for shared random bits (in parent this is a no op)
		*/

		void random_generator_thread_runner(void) override {
			while ( true ) {
				random_waiter_wait_for_signal();
				bool which_region = _random_gen_region->load(std::memory_order_acquire);
				regenerate_shared(which_region);		
			}
		}



		/**
		 * wakeup_random_generator
		 * 
		 * 
		*/
		void wakeup_random_generator(uint8_t which_region) {   //
			//
			_random_gen_region->store(which_region);
			//
			random_waiter_notify();
		}




		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	public: 

		// GET REAL BITS (no operational membership and memory maps)

		// CONTROL BITS - cbits - for a base, a bit pattern of membership positions, for a member, a backreference to the base
		// TAKEN BITS - tbits - for a base, a bit pattern of allocated positions for the NEIGHBOR window from the base
		//					  - for a member, a coefficient for some ordering of member elements such as time (for example).



	public: 


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

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



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * _get_member_bits_slice_info
		 * 
		 * Only one parameter is passed as true input. All the others are by reference. 
		 * 
		 *	uint8_t				_bucket_count;		// + 8		// counts all included space takers including deletes -- reduced by cropping
		 * 	uint32_t			_stash_ops;			// same as cbits ... except that membership role is not in use
		 *  uint32_t			_reader_ops;		// same as tbits ... except that memory allocation is not kept
		 * 
		 * 
		 * 	In this version of the hash table, the bucket is a reference to a storage of hh_elements. Hence, 
		 * every hh_bucket is a member. The bucket count is just a count of how many elements are set with a key-value. In the 
		 * other implementation of the table, the count is given by the popcount of the membership map. But, as there is no
		 * overlap in this version, the **cbits** will not be in use.
		 * 
		 * Instead, `_stash_ops` and `_reader_ops` are used to manage wait conditions for readers:
		 * 
		 * * `wait_for_readers`
		 * * `release_to_readers`
		 * * `add_reader`
		 * * `remove_reader`
		 * 
		 * 	NOTE: this means that this method always ensures that the bucket is associated with a cbit stash element 
		 * 	before moving on to be a bucket master or any other sort of bucket change participant.
		 * 
		 * Parameter (in): 
		 * 	`h_bucket` - this is the bucket number resulting from taking the object hash modulo the number of elements stored in a slice
		 * Parameters (out):
		 * 	`which_table` - one or zero {0,1} indicating the slice chosen for the new entry
		 * 	`c_bits` - the membership bits of the base element identified by the bucket information.
		 * 	`c_bits_op` - the operation bits of the bucket whether a member or a base
		 * 	`c_bits_base_ops` - the operation bits of the base if the `h_bucket` resolves to a member, zero otherwise
		 * 	`bucket_ref` - the address of the address valued variable which will reference the bucket in the established slice
		 * 	`buffer_ref` - the address of the start of memory of the established slice
		 * 	`end_buffer_ref` - the address of the end of the memory region containing the slice
		 * 	`cbit_stashes` - an array of references to stash elements. There will be at least one. For `h_buckets` landing on base and empty buckets, 
		 * 					 this will be one. For `h_buckets` landing on members (precursor to a usurp operation), there will be two. 
		 * 
		 * 	<-- called by prepare_for_add_key_value_known_refs -- which is application facing.
		*/
		atomic<uint32_t> *_get_member_bits_slice_info(uint32_t h_bucket,uint8_t &which_table,uint32_t &c_bits,uint32_t &c_bits_op,uint32_t &c_bits_base_ops,sp_element **bucket_ref,sp_element **buffer_ref,sp_element **end_buffer_ref,[[maybe_unused]] CBIT_stash_holder *cbit_stashes[4]) {
			//
			if ( h_bucket >= _max_n ) {   // likely the caller should sort this out
				h_bucket -= _max_n;  // let it be circular...
			}
			//
			c_bits_op = 0;
			c_bits = 0;
			c_bits_base_ops = 0;
			//
			uint8_t count0{0};
			uint8_t count1{0};
			//
			// look in the buffer 0
			sp_element *buffer0		= _region_HV_0;
			sp_element *end_buffer0	= _region_HV_0_end;
			//
			sp_element *el_0 = buffer0 + h_bucket;
			atomic<uint32_t> *a_cbits0 = (atomic<uint32_t> *)(&(el_0->_bucket_count));
			count0 = a_cbits0->load(std::memory_order_acquire);
			//

			//
			// look in the buffer 1
			sp_element *buffer1		= _region_HV_1;
			sp_element *end_buffer1	= _region_HV_1_end;
			//
			sp_element *el_1 = buffer1 + h_bucket;
			atomic<uint32_t> *a_cbits1 = (atomic<uint32_t> *)(&(el_0->_bucket_count));
			count1 = a_cbits1->load(std::memory_order_acquire);

			//	make selection of slice
			auto selector = _hlpr_select_insert_buffer(count0,count1);
			if ( selector == 0xFF ) return nullptr;		// selection failed (error state)
			//
			// confirmed... set output parameters
			atomic<uint32_t> *a_cbits = nullptr;
			//
			which_table = selector;
			//
			if ( selector == 0 ) {				// select data structures
				*bucket_ref = el_0;
				*buffer_ref = buffer0;
				*end_buffer_ref = end_buffer0;
				//
				a_cbits = a_cbits0;
			} else {
				*bucket_ref = el_1;
				*buffer_ref = buffer1;
				*end_buffer_ref = end_buffer1;
				//
				a_cbits = a_cbits1;
			}
			//
			return a_cbits;
		}


	public:


		/**
		 * DATA STRUCTURES:
		 */  

		uint32_t						_num_threads;
		uint32_t						_max_count;
		uint32_t						_max_n;   // max_count/2 ... the size of a slice.
		//
		sp_element		 				*_region_HV_0;
		sp_element		 				*_region_HV_1;
		sp_element		 				*_region_HV_0_end;
		sp_element		 				*_region_HV_1_end;

		atomic<uint32_t>				*_random_gen_region;

/**
 * SUB: HH_thread_manager -- could be a subclass, but everything is one class.
 * 
*/

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * is_restore_queue_empty - check on the state of the restoration queue.
		 * 
		 * 
		*/
		bool is_restore_queue_empty(uint8_t thread_id, uint8_t which_table) {
			sp_proc_descr *p = _process_table + thread_id;
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
		void enqueue_restore(hh_adder_states update_type, atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t cbits_op, uint32_t cbits_op_base, sp_element *hash_ref, uint32_t h_bucket, uint32_t el_key, uint32_t value, uint8_t which_table, sp_element *buffer, sp_element *end,uint8_t require_queue) {
			sp_q_entry get_entry;
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
			sp_proc_descr *p = _process_table + require_queue;
			//
			p->_process_queue[which_table].push(get_entry); // by ref
		}



		/**
		 * dequeue_restore
		*/
		void dequeue_restore(hh_adder_states &update_type, atomic<uint32_t> **control_bits_ref, uint32_t &cbits, uint32_t &cbits_op, uint32_t &cbits_op_base, sp_element **hash_ref_ref, uint32_t &h_bucket, uint32_t &el_key, uint32_t &value, uint8_t &which_table, uint8_t assigned_thread_id, sp_element **buffer_ref, sp_element **end_ref) {
			//
			sp_q_entry get_entry;
			//
			sp_proc_descr *p = _process_table + assigned_thread_id;
			//
			p->_process_queue[which_table].pop(get_entry); // by ref
			//
			update_type = get_entry.update_type;
			*control_bits_ref = get_entry.control_bits;
			cbits = get_entry.cbits;
			cbits_op = get_entry.cbits_op;
			cbits_op_base = get_entry.cbits_op_base;
			//
			sp_element *hash_ref = get_entry.hash_ref;
			h_bucket = get_entry.h_bucket;
			el_key = get_entry.el_key;
			value = get_entry.value;
			which_table = get_entry.which_table;
			sp_element *buffer = get_entry.buffer;
			sp_element *end = get_entry.end;
			//
			*hash_ref_ref = hash_ref;
			*buffer_ref = buffer;
			*end_ref = end;
		}



		/**
		 * enqueue_cropping
		*/
		void enqueue_cropping(sp_element *hash_ref,uint32_t cbits,sp_element *buffer,sp_element *end,uint8_t which_table) {
			sp_crop_entry get_entry;
			//
			get_entry.hash_ref = hash_ref;
			get_entry.cbits = cbits;
			get_entry.buffer = buffer;
			get_entry.end = end;
			get_entry.which_table = which_table;
			//
			sp_proc_descr *p = _process_table + _round_robbin_proc_table_threads;
			//
			_round_robbin_proc_table_threads++;
			if ( _round_robbin_proc_table_threads >= _num_threads ) _round_robbin_proc_table_threads = 1;
			//
			p->_to_cropping[which_table].push(get_entry); // by ref
		}



		/**
		 * dequeue_cropping
		*/
		void dequeue_cropping(sp_element **hash_ref_ref, uint32_t &cbits, uint8_t &which_table, uint8_t assigned_thread_id , sp_element **buffer_ref, sp_element **end_ref) {
			//
			sp_crop_entry get_entry;
			//
			sp_proc_descr *p = _process_table + assigned_thread_id;
			//
			p->_to_cropping[which_table].pop(get_entry); // by ref
			//
			sp_element *hash_ref = get_entry.hash_ref;
			cbits = get_entry.cbits;
			which_table = get_entry.which_table;
			//
			sp_element *buffer = get_entry.buffer;
			sp_element *end = get_entry.end;
			//
			*hash_ref_ref = hash_ref;
			*buffer_ref = buffer;
			*end_ref = end;
		}



		/**
		 * is_restore_queue_empty - check on the state of the restoration queue.
		*/
		bool is_cropping_queue_empty(uint8_t thread_id,uint8_t which_table) {
			sp_proc_descr *p = _process_table + thread_id;
			bool is_empty = p->_to_cropping[which_table].empty();
#ifndef __APPLE__

#endif
			return is_empty;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * wakeup_value_restore
		*/

		void wakeup_value_restore(hh_adder_states update_type, atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t cbits_op, uint32_t cbits_op_base, sp_element *bucket, uint32_t h_start, uint32_t el_key, uint32_t value, uint8_t which_table, sp_element *buffer, sp_element *end, CBIT_stash_holder *csh) {
			// this queue is jus between the calling thread and the service thread belonging to just this process..
			// When the thread works it may content with other processes for the hash buckets on occassion.
			auto service_thread = csh->_service_thread;
			if ( service_thread == 0 ) {
				_round_robbin_proc_table_threads++;
				if ( _round_robbin_proc_table_threads >= _num_threads ) _round_robbin_proc_table_threads = 1;
				service_thread = csh->_service_thread = _round_robbin_proc_table_threads;
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

		void submit_for_cropping(sp_element *base,uint32_t cbits,sp_element *buffer,sp_element *end,uint8_t which_table) {
			enqueue_cropping(base,cbits,buffer,end,which_table);
			wake_up_one_cropping();
		}


	public: 

		// threads ...
		sp_proc_descr						*_process_table;						
		sp_proc_descr						*_end_procs;

		uint8_t							_round_robbin_proc_table_threads{1};
		//

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

/**
 * SUB: HH_map_atomic_no_wait -- could be a subclass, but everything is one class.
 * 
*/

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * empty_bucket
		 * 
		*/

		inline bool empty_bucket(sp_element *base) {
			atomic<uint32_t> *a_c = (atomic<uint32_t> *)(&(base->_bucket_count));
			auto  count = a_c->load(std::memory_order_acquire);
			return (count == 0);
		}


		/**
		 * _adder_bucket_queue_release
		 * 
		 * 
		 * cbits -- remain last known state of the bucket.
		 * 
		 * parameters:
		 * 
		 * control_bits 		- (atomic<uint32_t> *) - atomic reference to the cell's control bits. 
		 * el_key 			- (uint32_t) - the new element key
		 * h_bucket 		- (uint32_t) - the array index derived from the hash value of the object data
		 * offset_value 	- (uint32_t) - the byte offset to the stored object of that hash which will be the value of the new key-value element
		 * cbits			- the real cbits of the cell or of the base if the bucket is a member
		 * cbits_op			- the operation cbits of the bucket in their current state, either just made when stashing or with a reference count update
		 * cbits_base_op	- if the bucket is a member, then the operational bits of the base (also stashed) otherwise zero.
		 * bucket			- the memory reference of the bucket, an hh_element
		 * buffer			- start of memory slice
		 * end_buffer		- end of memory slice
		 * cbit_stashes		- stash object references, one if the bucket is a base, two if the bucket is a member with the second being the stash of the base
		*/

		void _adder_bucket_queue_release(atomic<uint32_t> *control_bits, uint32_t el_key, uint32_t h_bucket, uint32_t offset_value, uint8_t which_table, uint32_t cbits, uint32_t cbits_op,uint32_t cbits_base_op, sp_element *bucket, sp_element *buffer, sp_element *end_buffer,CBIT_stash_holder *cbit_stashes[4]) {
			//
			wakeup_value_restore(HH_FROM_BASE_AND_WAIT, control_bits, cbits, cbits_op, cbits_base_op, bucket, h_bucket, el_key, offset_value, which_table, buffer, end_buffer, cbit_stashes[0]);
			//
		}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	public:

		// ---- ---- ---- STATUS

		bool ok(void) {
			return(_status);
		}

	public:



		/**
		 * _cropper
		 * 
		 * 
		 * del_cbits -- last known state of the bucket when the del submitted the base for cropping...
		 * 
		*/

		void _cropper(sp_element *base) {
			//
			wait_for_readers(base,true);

			auto btype = base->_slab_type;
			uint16_t bytes_needed = _SP.bytes_needed(btype);
			//
			uint8_t elements_buffer[bytes_needed];
			hh_element *el = (hh_element *)(&elements_buffer[0]);
			hh_element *end_els = (hh_element *)(&elements_buffer[0] + bytes_needed);
			//
			_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
			//
			sort(el,end_els,[](hh_element &a, hh_element &b) {		// largest to smallet
				return a.tv.taken > b.tv.taken;
			});
			while ( el < end_els ) {
				if ( el->tv.taken != 0 ) {
					memset(el,0,sizeof(hh_element));
					base->_bucket_count--;
				}
			}
			//
			_SP.unload_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
			if ( (_SP.bytes_needed(btype)/2) > base->_bucket_count ) {
				contract_base(base);
			}
			//
			release_to_readers(base);
		}


		/**
		 * short_list_old_entry - calls on the application's method for puting displaced values, new enough to be kept,
		 * into a short list that may be searched in the order determined by the application (e.g. first for preference, last
		 * for normal lookup except fails.)
		 * 
		 * If the key value and time info result in a decision to pass the data out, either to a new tier or to a seconday 
		 * database or to final removal, then the caller's method set in `_timout_tester` will perform the necessary action 
		 * to send the data item along. In the case of the shared LRU, the item will have to be identified by its time and
		 * be removed from a timer list. 
		 * 
		*/
		bool (*_timout_tester)(uint32_t,uint32_t,uint32_t){nullptr};
		void set_timeout_test(bool (*timeouter)(uint32_t,uint32_t,uint32_t)) {
			_timout_tester = timeouter;
		}
		//
		bool short_list_old_entry(uint32_t key,uint32_t value,uint32_t store_time) {
			if ( _timout_tester != nullptr ) {
				return (*_timout_tester)(key,value,store_time);
			}
			return true;
		}


		/**
		 * wait_for_readers
		 */
		void wait_for_readers(sp_element *base,[[maybe_unused]] bool lock) {
			//
			uint32_t stash = 0;
			atomic<uint32_t> *a_readers = (atomic<uint32_t> *)(&(base->_reader_ops));
			atomic<uint32_t> *a_stash = (atomic<uint32_t> *)(&(base->_stash_ops));
			do {
				auto readers = a_readers->load(std::memory_order_acquire);
				if ( !tbits_sem_at_zero(readers) ) {
					tick();
					readers = a_readers->load(std::memory_order_acquire);
				}
				//
				stash = a_stash->load(std::memory_order_acquire);
				//
				while ( base_in_operation(stash) ) {  // also wait for other edit operations
					tick();
					stash = a_stash->load(std::memory_order_acquire);
				}
				auto prev_stash = stash;
				while ( (!a_stash->compare_exchange_weak(stash,(prev_stash | EDITOR_CBIT_SET),std::memory_order_acq_rel) || (prev_stash == stash)) && !(base_in_operation(stash)) );
			} while ( base_in_operation(stash) );
			//
		}

		/**
		 * release_to_readers
		 */
		void release_to_readers(sp_element *base) {
			// ----
			atomic<uint32_t> *a_stash = (atomic<uint32_t> *)(&(base->_stash_ops));
			auto stash = a_stash->load(std::memory_order_acquire);
			auto prev_stash = stash;
			while ( (!a_stash->compare_exchange_weak(stash,(prev_stash & EDITOR_CBIT_RESET),std::memory_order_acq_rel) || (prev_stash == stash)) && !(stash & EDITOR_CBIT_SET) );
		}

		/**
		 * add_reader
		 */
		void add_reader(sp_element *base) {
			//
			atomic<uint32_t> *a_stash = (atomic<uint32_t> *)(&(base->_stash_ops));
			auto stash = a_stash->load(std::memory_order_acquire);
			//
			do {
				while ( editors_are_active(stash) ) {
					tick();
					stash = a_stash->fetch_or(READER_CBIT_SET,std::memory_order_acq_rel);
				}
				//
				while ( (stash & READER_CBIT_SET) == 0 ) {
					while ( !a_stash->compare_exchange_weak(stash,(stash | READER_CBIT_SET)) && !editors_are_active(stash) );
				}
			} while ( editors_are_active(stash) );
			//
			atomic<uint32_t> *a_readers = (atomic<uint32_t> *)(&(base->_reader_ops));
			auto readers = a_readers->load(std::memory_order_acquire);
			//
			while ( tbits_sem_at_max(readers) ) {
				tick();			// just too many readers in this bucket 
				readers = a_readers->load(std::memory_order_acquire);
			}
			//
			a_readers->fetch_add(TBITS_READER_SEM_INCR,std::memory_order_acq_rel);
		}

		/**
		 * remove_reader
		 */
		void remove_reader(sp_element *base) {
			atomic<uint32_t> *a_stash = (atomic<uint32_t> *)(&(base->_stash_ops));
			auto stash = a_stash->load(std::memory_order_acquire);
			//
			if ( readers_are_active(stash) ) {
				//
				atomic<uint32_t> *a_readers = (atomic<uint32_t> *)(&(base->_reader_ops));
				auto readers = a_readers->load(std::memory_order_acquire);
				if ( !tbits_sem_at_zero(readers) ) {
					readers = a_readers->fetch_sub(TBITS_READER_SEM_INCR,std::memory_order_acq_rel);
				}
				if ( (readers-1) == 0 ) {
					while ( (stash & READER_CBIT_SET) != 0 ) {
						while ( !a_stash->compare_exchange_weak(stash,(stash & READER_CBIT_RESET)) && !editors_are_active(stash) );
					}
				}
				//
			}

		}


		/**
		 * expand_base
		 */
		void expand_base(sp_element *base) {
			auto st = base->_slab_type;
			auto si = base->_slab_index;
			auto so = base->_slab_offset;
			_SP.expand(st,si,so,( _max_n/(1 << (st+1)) ));
		}


		/**
		 * contract_base
		 */
		void contract_base(sp_element *base) {
			auto st = base->_slab_type;
			auto si = base->_slab_index;
			auto so = base->_slab_offset;
			_SP.contract(st,si,so,( _max_n/(1 << (st+1)) ));
		}


		hh_element *ref_oldest(hh_element *elements_buffer,hh_element *end_els) {
			hh_element *el = elements_buffer;
			hh_element *oldest = el;
			auto timed = el->tv.taken;
			el++;
			while ( el < end_els ) {
				auto cmp_timed = el->tv.taken;
				if ( cmp_timed < timed ) {
					timed = cmp_timed;
					oldest = el;
				}
				el++;
			}
			return el;
		}


		/**
		 * value_restore_runner   --- a thread method...
		 * 
		 * One loop of the main thread for restoring values that have been replaced by a new value.
		 * The element being restored may still be in play, but 
		*/
		void value_restore_runner(uint8_t slice_for_thread, uint8_t assigned_thread_id) override {
			atomic<uint32_t> *control_bits = nullptr;
			uint32_t real_cbits = 0;
			uint32_t cbits_op = 0;
			uint32_t cbits_op_base = 0;
			//
			sp_element *base = nullptr;
			uint32_t h_bucket = 0;
			uint32_t el_key = 0;
			uint32_t offset_value = 0;
			uint8_t which_table = slice_for_thread;
			sp_element *buffer = nullptr;
			sp_element *end_buffer = nullptr;
			hh_adder_states update_type;
			//
			while ( is_restore_queue_empty(assigned_thread_id,slice_for_thread) ) wait_notification_restore();
			//
			dequeue_restore(update_type, &control_bits, real_cbits, cbits_op, cbits_op_base, &base, h_bucket, el_key, offset_value, which_table, assigned_thread_id, &buffer, &end_buffer);
			// store... if here, it should be locked
			//
			uint32_t store_time = now_time(); // now
			//
			wait_for_readers(base,true);
			//
			auto btype = base->_slab_type;
			uint16_t bytes_needed = _SP.bytes_needed(btype);
			//
			uint8_t elements_buffer[bytes_needed];
			hh_element *el = (hh_element *)(&elements_buffer[0]);
			hh_element *end_els = (hh_element *)(&elements_buffer[0] + bytes_needed);

			_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);

			uint8_t max_els = _SP.max_els(base->_slab_type);

			if ( base->_bucket_count == max_els ) {
				if ( max_els == 32 ) {
					hh_element *oldest = ref_oldest((hh_element *)elements_buffer,end_els);
					short_list_old_entry(oldest->c.key, oldest->tv.value, store_time);
					memset((void *)oldest,0,_SP.bytes_needed(btype));
					oldest->c.key = el_key;
					oldest->tv.value = offset_value;
					oldest->tv.taken = store_time;
				} else {
					expand_base(base);
					_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
				}
			} else {
				auto count = base->_bucket_count;
				while ( (el < end_els) && (count > 0)) {
					if ( el->c.key == UINT32_MAX ) {
						memset((void *)el,0,_SP.bytes_needed(btype));
						el->c.key = el_key;
						el->tv.value = offset_value;
						el->tv.taken = store_time;
						break;
					} else if ( el->c.key == 0 ) {
						el->c.key = el_key;
						el->tv.value = offset_value;
						el->tv.taken = store_time;
						base->_bucket_count++;
						break;
					}
					el++; count--;
				}
			}
			_SP.unload_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
			release_to_readers(base);
		}




	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * cropper_runner   --- a thread method...
		 * 
		 * One loop of the main thread for restoring values that have been replaced by a new value.
		 * The element being restored may still be in play, but 
		*/
		void cropper_runner(uint8_t slice_for_thread, uint8_t assigned_thread_id)  override {
			sp_element *base = nullptr;
			uint32_t cbits = 0;
			uint8_t which_table = slice_for_thread;
			sp_element *buffer = nullptr;
			sp_element *end = nullptr;
			//
			while ( is_cropping_queue_empty(assigned_thread_id,slice_for_thread) ) wait_notification_cropping();
			//
			dequeue_cropping(&base, cbits, which_table, assigned_thread_id, &buffer, &end);
			// store... if here, it should be locked
			// cbits, 
			_cropper(base);
		}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * two methods: `prepare_for_add_key_value_known_refs` and `add_key_value_known_refs`
		 * 				work together to insert a new value into the table under the best chosen slice
		 * 				and with as little delay as possible. 
		 */

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
		 * cbits -- the membership map for the base bucket (which may be empty) -- these are always the real bits
		 * cbits_op -- the operational bits of the bucket for either case of being a member or being a base.
		 * cbits_base_ops -- if the bucket is a member, this conains the operation bits of the base if there is an op or zero otherwise
		 * 
		 * Returns:  bucket, buffer, end_buffer by address `**`
		 * 
		 * bucket_ref -- contains the address of the element at the offset `h_bucket`
		 * buffer_ref -- contains the address of the storage region for the selected slice
		 * end_buffer_ref -- contains the address of the end of the storage region
		 * 
		*/
		bool prepare_for_add_key_value_known_refs(atomic<uint32_t> **control_bits_ref,uint32_t h_bucket,uint8_t &which_table,uint32_t &cbits,uint32_t &cbits_op,uint32_t &cbits_base_ops,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,CBIT_stash_holder *cbit_stashes[4]) override {
			//
			// keeping this call compatible with the interface used by LRU without inspection in to references, just passing them on			
			//
			atomic<uint32_t> *a_c_bits = _get_member_bits_slice_info(h_bucket,which_table,cbits,cbits_op,cbits_base_ops,(sp_element **)bucket_ref,(sp_element **)buffer_ref,(sp_element **)end_buffer_ref,cbit_stashes);
			//
			if ( a_c_bits == nullptr ) return false;
			*control_bits_ref = a_c_bits;
			//
			return true;
		}


		/**
		 * add_key_value_known_refs
		 * 
		 * 
		 * cbits -- reflects the last known state of the bucket in op or a membership map
		*/
		void add_key_value_known_refs(atomic<uint32_t> *control_bits,uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table,uint32_t cbits,uint32_t cbits_op,uint32_t cbits_base_op,hh_element *bucket,hh_element *buffer,hh_element *end_buffer,CBIT_stash_holder *cbit_stashes[4]) override {
			//
			uint8_t selector = 0x3;  // don't check the selector bit
			//
			if ( (offset_value != 0) &&  selector_bit_is_set(h_bucket,selector) ) {
				//
				_adder_bucket_queue_release(control_bits,el_key,h_bucket,offset_value,which_table,cbits,cbits_op,cbits_base_op,(sp_element *)bucket,(sp_element *)buffer,(sp_element *)end_buffer,cbit_stashes);
				//
			}
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



		/**
		 * get
		 * 
		 * parameters:
		 * 	`el_key` - a 32 bit hash key derived from the full data representation of the stored object
		 * 	`h_bucket` - the hash bucket calculated as the `el_key` modulo the number of possible buckets
		 * 
		 * 	returns:  the value stored under the key
		 * 
		*/
		uint32_t get(uint64_t key) override {
			uint32_t el_key = (uint32_t)((key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t hash = (uint32_t)(key & HASH_MASK);
			//
			return get(hash,el_key);
		}

		// el_key == hull_hash (usually)

		uint32_t get(uint32_t el_key, uint32_t h_bucket) override {  // full_hash,hash_bucket
			//
			if ( el_key == UINT32_MAX ) return UINT32_MAX;
			//
			uint8_t selector = 0;
			if ( selector_bit_is_set(h_bucket,selector) ) { // get the selector
				h_bucket = clear_selector_bit(h_bucket);	// set h_bucket to be just its index
			} else return UINT32_MAX;
			//
			// This is the region where the element is to be found (if it is there)
			sp_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			sp_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			sp_element *base = bucket_at(h_bucket, buffer, end);		// get the bucket 
			if ( empty_bucket(base) ) return UINT32_MAX;   // empty_bucket cbits by ref
			//
			auto btype = base->_slab_type;
			uint16_t bytes_needed = _SP.bytes_needed(btype);
			//
			uint8_t elements_buffer[bytes_needed];
			hh_element *el = (hh_element *)(&elements_buffer[0]);
			hh_element *end_els = (hh_element *)(&elements_buffer[0] + bytes_needed);
			//
			add_reader(base);
			//
			_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
			//
			auto count = base->_bucket_count;
			while ( (el < end_els) && (count > 0)) {
				if ( el->c.key != UINT32_MAX ) {
					if ( el->c.key == el_key ) {
						remove_reader(base);
						return el->tv.value;
					}
				}
				el++; count--;
			}
			//
			remove_reader(base);
			//
			return UINT32_MAX;
		}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * update
		 * 
		 * parameters:
		 * 	`el_key` - a 32 bit hash key derived from the full data representation of the stored object
		 * 	`h_bucket` - the hash bucket calculated as the `el_key` modulo the number of possible buckets
		 * 	`v_value` - the new value to be stored under the key
		 *
		 * 	This method overrides the abstract declaration given in `HMap_interface` found in `hmap_interface.h`. 
		 * 
		 * Note that for this method the element key contains the selector bit for the even/odd buffers.
		 * The hash value and location must already exist in the neighborhood of the hash bucket.
		 * The value passed is being stored in the location for the key...
		 * 
		*/
		// el_key == hull_hash (usually)
		uint64_t update(uint32_t el_key, uint32_t h_bucket, uint32_t v_value) override {
			//
			if ( v_value == 0 ) return UINT64_MAX;
			if ( el_key == UINT32_MAX ) return UINT64_MAX;
			//
			uint64_t loaded_key = (((uint64_t)el_key) << HALF) | h_bucket; // LOADED
			// 
			uint8_t selector = 0;
			if ( selector_bit_is_set(h_bucket,selector) ) { // get the selector
				h_bucket = clear_selector_bit(h_bucket);	// set h_bucket to be just its index
			} else return UINT32_MAX;
			//
			// This is the region where the element is to be found (if it is there)
			sp_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			sp_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			sp_element *base = bucket_at(h_bucket, buffer, end);		// get the bucket 
			if ( empty_bucket(base) ) return UINT64_MAX;   // empty_bucket cbits by ref
			//
			add_reader(base);
			wait_for_readers(base,true);
			//
			auto btype = base->_slab_type;
			uint16_t bytes_needed = _SP.bytes_needed(btype);
			//
			uint8_t elements_buffer[bytes_needed];
			hh_element *el = (hh_element *)(&elements_buffer[0]);
			hh_element *end_els = (hh_element *)(&elements_buffer[0] + bytes_needed);
			//
			_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
			//
			auto count = base->_bucket_count;
			while ( (el < end_els) && (count > 0)) {
				if ( el->c.key != UINT32_MAX ) {
					if ( el->c.key == el_key ) {
						remove_reader(base);
						wait_for_readers(base,true);
						el->tv.value = v_value;
						_SP.unload_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
						release_to_readers(base);
						return loaded_key;
					}
				}
				el++; count--;
			}
			remove_reader(base);
			return UINT64_MAX;
		}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * del - removes an element by making it unreadable and then submitting the element for erasure by 
		 * the cropping thread(s).
		 * 
		*/

		// loade key version supplied for callers with unparsed key-value pairs.
		uint32_t del(uint64_t loaded_key) override {
			uint32_t el_key = (uint32_t)((loaded_key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t h_bucket = (uint32_t)(loaded_key & HASH_MASK);
			return del(el_key, h_bucket);
		}


		/**
		 * 
		 * del
		 * parameters:
		 * 	`el_key` - a 32 bit hash key derived from the full data representation of the stored object
		 * 	`h_bucket` - the hash bucket calculated as the `el_key` modulo the number of possible buckets
		 *
		 * 	This method overrides the abstract declaration given in `HMap_interface` found in `hmap_interface.h`. 
		 * 
		 * 	Overview:
		 * 
		 * 		The full key must be in the interval (0,UINT32_MAX), excluding the endpoints
		 * 		As this module must have previously assigned the bucket, the 32-bit bucket word carries the segment selector
		 * 		derived for the stored element.
		 */
		uint32_t del(uint32_t el_key, uint32_t h_bucket) override {
			//
			if ( el_key == UINT32_MAX ) return UINT32_MAX;
			//
			uint8_t selector = 0;
			if ( selector_bit_is_set(h_bucket,selector) ) { // get the selector
				h_bucket = clear_selector_bit(h_bucket);	// set h_bucket to be just its index
			} else return UINT32_MAX;
			//
			sp_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			sp_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			sp_element *base = bucket_at(h_bucket, buffer, end);		// get the bucket 
			if ( empty_bucket(base) ) return UINT32_MAX;   // empty_bucket cbits by ref
			//
			add_reader(base);
			//
			auto btype = base->_slab_type;
			uint16_t bytes_needed = _SP.bytes_needed(btype);
			//
			uint8_t elements_buffer[bytes_needed];
			hh_element *el = (hh_element *)(&elements_buffer[0]);
			hh_element *end_els = (hh_element *)(&elements_buffer[0] + bytes_needed);
			//
			_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
			//
			auto count = base->_bucket_count;
			while ( (el < end_els) && (count > 0)) {
				if ( el->c.key != UINT32_MAX ) {
					if ( el->c.key == el_key ) {
						el->tv.value = 0;
						remove_reader(base);			// no reading anymore
						wait_for_readers(base,true);	// wait for other readers and ops
						el->c.key = UINT32_MAX;
						el->tv.taken = 0;
						_SP.unload_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
						release_to_readers(base);
						submit_for_cropping(base,0,buffer,end,selector);  // after a total swappy read, all BLACK keys will be at the end of members
						return el_key;
					}
				}
				el++; count--;
			}
			remove_reader(base);
			return UINT32_MAX;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * clear --
		 * 
		 * Obliterats the data in the shared regions. Call on `setup_region`, which zeros out the buffers 
		 * and rebuilds any intialized data structures.
		 * 
		 * The caller must be the initializer. Other threads and processes must only observe the change.
		 * They may then reattach.
		 * 
		*/
		void clear(void) override {
			if ( _initializer ) {
				uint8_t sz = sizeof(HHash);
				uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
				setup_region(_initializer,header_size,_max_count,_num_threads);
			}
		}


	public: 

		SlabProvider							_SP;

};


#endif // _H_SPARSE_SLABS_HASH_SHM_
