#ifndef _H_QUEUED_HASH_SHM_
#define _H_QUEUED_HASH_SHM_

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
#include "hh_queues_and_states.h"

#include "array_p_defs.h"




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


// ---- ---- ---- ---- ---- ----  HHash
// HHash <- HHASH

/**
 * CLASS: SS_map
 * 
 * Sparse Slabs
 * 
*/


template<const uint32_t TABLE_SIZE = 100>
class QUEUED_map : public Random_bits_generator<>, public HMap_interface {

	public:

		// SSlab_map LRU_cache -- constructor
		QUEUED_map(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) {
			initialize_all(region, seg_sz, max_element_count, num_threads, am_initializer);
		}

		virtual ~QUEUED_map() {
		}

	public:

		// LRU_cache -- constructor
		void initialize_all(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) {
			if ( am_initializer ) {
				_reason = "Queued store is never the initializer in the module. See the main executable.";
				_status = false;
				return;
			}
			//
			_reason = "OK";
			//
			_region = region;
			_endof_region = _region + seg_sz;
			//
			_num_threads = num_threads;
			//
			_status = true;
			_initializer = false;
			_max_count = max_element_count;
			//
			// initialize from constructor
			setup_region();
			//
			_proc_id = _com.next_thread_id();
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// REGIONS...

		/**
		 * setup_region -- part of initialization if the process is the intiator..
		 * -- header_size --> HHash
		 *  the regions are setup as, [values 1][buckets 1][values 2][buckets 2][controls 1 and 2]
		*/
		void setup_region(void) {
			// ----
			_com.initialize(_num_threads,_num_threads,_region,TABLE_SIZE,false);
			//
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * check_expected_hh_region_size
		 * 
		*/

		static uint32_t check_expected_hh_region_size([[maybe_unused]] uint32_t els_per_tier, uint32_t num_threads) {
			auto reg_sz = ExternalInterfaceQs<TABLE_SIZE>::check_expected_com_region_size(TABLE_SIZE);
			uint32_t predict = reg_sz*num_threads;
			return predict;
		}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	public:

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

		void _adder_bucket_queue_release([[maybe_unused]] atomic<uint32_t> *control_bits, uint32_t el_key, [[maybe_unused]] uint32_t h_bucket, uint32_t offset_value, [[maybe_unused]] uint8_t which_table, [[maybe_unused]] uint32_t cbits, [[maybe_unused]] uint32_t cbits_op, [[maybe_unused]] uint32_t cbits_base_op, [[maybe_unused]] hh_element *bucket, [[maybe_unused]] hh_element *buffer, [[maybe_unused]] hh_element *end_buffer,[[maybe_unused]] CBIT_stash_holder *cbit_stashes[4]) {
			_com.com_put(el_key,offset_value,_proc_id);
		}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	public:

		// ---- ---- ---- STATUS

		bool ok(void) {
			return(_status);
		}


		bool (*_timout_tester)(uint32_t,uint32_t,uint32_t){nullptr};
		void set_timeout_test(bool (*timeouter)(uint32_t,uint32_t,uint32_t)) {
			_timout_tester = timeouter;
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
		bool prepare_for_add_key_value_known_refs([[maybe_unused]] atomic<uint32_t> **control_bits_ref,[[maybe_unused]] uint32_t h_bucket,[[maybe_unused]] uint8_t &which_table,[[maybe_unused]] uint32_t &cbits,[[maybe_unused]] uint32_t &cbits_op,[[maybe_unused]] uint32_t &cbits_base_ops,[[maybe_unused]] hh_element **bucket_ref,[[maybe_unused]] hh_element **buffer_ref,[[maybe_unused]] hh_element **end_buffer_ref,[[maybe_unused]] CBIT_stash_holder *cbit_stashes[4]) override {
			//
			// keeping this call compatible with the interface used by LRU without inspection in to references, just passing them on			
			//
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
			//
			_adder_bucket_queue_release(control_bits,el_key,h_bucket,offset_value,which_table,cbits,cbits_op,cbits_base_op,(hh_element *)bucket,(hh_element *)buffer,(hh_element *)end_buffer,cbit_stashes);
			//
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

		uint32_t get(uint32_t el_key, [[maybe_unused]] uint32_t h_bucket) override {  // full_hash,hash_bucket
			//
			if ( el_key == UINT32_MAX ) return UINT32_MAX;

			uint32_t val = 0;
			_com.com_req(el_key,val,_proc_id);
			//
			return val;
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
		uint64_t update(uint32_t el_key, [[maybe_unused]] uint32_t h_bucket, uint32_t v_value) override {
			//
			if ( v_value == 0 ) return UINT64_MAX;
			if ( el_key == UINT32_MAX ) return UINT64_MAX;
			_com.com_put(el_key,v_value,_proc_id);
			//
			uint64_t loaded_key = (((uint64_t)el_key) << HALF) | v_value; // LOADED
			loaded_key = stamp_key(loaded_key,1);
			//
			return loaded_key;
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
		uint32_t del(uint32_t el_key, [[maybe_unused]] uint32_t h_bucket) override {
			//
			if ( el_key == UINT32_MAX ) return UINT32_MAX;
			//
			_com.com_put(el_key,UINT32_MAX,_proc_id);

			return el_key;
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
		void clear(void) override {}



		// pointless

		virtual void 		value_restore_runner([[maybe_unused]] uint8_t slice_for_thread, [[maybe_unused]] uint8_t assigned_thread_id) override {
		}
		virtual void		cropper_runner([[maybe_unused]] uint8_t slice_for_thread, [[maybe_unused]] uint8_t assigned_thread_id) override {
		}
		virtual void		set_random_bits([[maybe_unused]] void *shared_bit_region) override {
		}
		virtual void share_lock(void) override {
		}
		virtual void share_unlock(void) override {
		}


	public: 

		/**
		 * DATA STRUCTURES:
		 */  


		ExternalInterfaceQs<TABLE_SIZE> 		_com;
		uint8_t									_proc_id;

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
		/**
		 * DATA STRUCTURES:
		 */  

		uint32_t						_num_threads;
		uint32_t						_max_count;
		uint32_t						_max_n;   // max_count/2 ... the size of a slice.
		//
};


#endif // _H_QUEUED_HASH_SHM_
