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
		sp_element 			*hash_ref;
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
		sp_element 		*hash_ref;
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

	bool		compare_key(uint32_t key, q_entry *el,uint32_t &value) {
		uint32_t ky = el->el_key;
		value = el->value;
		return (ky == key);
	}

};



/**
 * QueueEntryHolder uses q_entry in SharedQueue_SRSW<q_entry,ExpectedMax>
 * 
 * 
*/

template<uint16_t const ExpectedMax = 64>
class CropEntryHolder : public  SharedQueue_SRSW<crop_entry,ExpectedMax> {
};




typedef struct PRODUCT_DESCR {
	//
	uint32_t						partner_thread;
	uint32_t						stats;
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
/**
 * zero_above
 */
static uint32_t zero_above(uint8_t hole) {
	if ( hole >= 31 ) {
		return  0xFFFFFFFF;
	}
	return zero_levels[hole];
}


/**
 * ones_above
 */

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

static inline sp_element *el_check_end(sp_element *ptr, sp_element *buffer, sp_element *end) {
	if ( ptr >= end ) return buffer;
	return ptr;
}


/**
 * el_check_beg_wrap
*/

static inline sp_element *el_check_beg_wrap(sp_element *ptr, sp_element *buffer, sp_element *end) {
	if ( ptr < buffer ) return (end - buffer + ptr);
	return ptr;
}


/**
 * el_check_end_wrap
*/

static inline sp_element *el_check_end_wrap(sp_element *ptr, sp_element *buffer, sp_element *end) {
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

/**
 * CLASS: HH_map
 * 
 * Initialization of shared memory sections... 
 * 
*/


template<const uint32_t NEIGHBORHOOD = 32>
class HH_map : public Random_bits_generator<>, public HMap_interface {


	public:

		// LRU_cache -- constructor
		HH_map(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false) {
			initialize_all(region, seg_sz, max_element_count, num_threads, am_initializer);
		}

		virtual ~HH_map() {
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

			_rand_gen_thread_waiting_spinner = (atomic_flag *)start;
			_random_share_lock = (atomic_flag *)(_rand_gen_thread_waiting_spinner + 1);
			_random_gen_region = (atomic<uint32_t> *)(_random_share_lock + 1);

			_rand_gen_thread_waiting_spinner->clear();
			_random_share_lock->clear();

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
			auto proc_regions_size = num_threads*sizeof(proc_descr);
			_process_table = (proc_descr *)(_region_HV_1_end);
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
		void share_unlock(void) override {
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

		void random_generator_thread_runner(void) override {
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
#ifndef __APPLE__
			while ( !(_rand_gen_thread_waiting_spinner->test_and_set()) );
			_rand_gen_thread_waiting_spinner->notify_one();
#else
			_rand_gen_thread_waiting_spinner->clear(std::memory_order_release);
#endif
		}




		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	public: 

		// GET REAL BITS (no operational membership and memory maps)

		// CONTROL BITS - cbits - for a base, a bit pattern of membership positions, for a member, a backreference to the base
		// TAKEN BITS - tbits - for a base, a bit pattern of allocated positions for the NEIGHBOR window from the base
		//					  - for a member, a coefficient for some ordering of member elements such as time (for example).

		// C BITS
		/**
		 * fetch_real_cbits
		 * 
		 */
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
		/**
		 * fetch_real_tbits
		 * 
		 */
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
			return 0;
		}


		/**
		 * get_real_base_cbits
		 * 
		 * 
		*/

		uint32_t get_real_base_cbits(hh_element *base_probe) {
			atomic<uint32_t> *a_cbits = (atomic<uint32_t> *)(&(base_probe->c.bits));
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
		 * 
		 * 
		*/

		uint32_t get_real_base_tbits(hh_element *base_probe) {
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base_probe->tv.taken));
			auto tbits = a_tbits->load(std::memory_order_acquire);
			if ( !(is_base_tbits(tbits)) ) {
				tbits = fetch_real_cbits(tbits);
			}
			return tbits;
		}


		/**
		 * cbits_base_from_backref_or_stashed_backref
		 * 
		 */

		hh_element *cbits_base_from_backref_or_stashed_backref(uint32_t cbits, uint8_t &backref, hh_element *from, hh_element *begin, hh_element *end) {
			auto stash_index = cbits_member_stash_index_of(cbits);
			if ( stash_index == 0 ) {
				return cbits_base_from_backref(cbits,backref,from,begin,end);
			} else {
				CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
				if ( csh != nullptr ) {
					auto member_bits = csh->stored._csm._member_bits.load(std::memory_order_acquire);
					return cbits_base_from_backref(member_bits,backref,from,begin,end);
				}
			}
			return nullptr;
		}




	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 

		// CBIT STASHING  -- CBIT RETRIEVAL


	public: 

		/**
		 * load_cbits
		 * 
		 */

		atomic<uint32_t> *load_cbits(atomic<uint32_t> *a_cbits_base,uint32_t &cbits,uint32_t &cbits_op) {
			cbits = a_cbits_base->load(std::memory_order_acquire);
			if ( !(is_base_noop(cbits)) && (cbits != 0) ) {
				cbits_op = cbits;
				cbits = fetch_real_cbits(cbits);
			}
			return a_cbits_base;
		}

		/**
		 * load_cbits
		 * 
		 */

		atomic<uint32_t> *load_cbits(hh_element *base,uint32_t &cbits,uint32_t &cbits_op) {
			atomic<uint32_t> *a_cbits_base = (atomic<uint32_t> *)(&(base->c.bits));
			return load_cbits(a_cbits_base,cbits,cbits_op);
		}

		/**
		 * load_cbits  -- 64 bit version
		 * 
		 * Out Parameter: base_ky --- the key for that value in the base
		 */

		atomic<uint64_t> *load_cbits(atomic<uint64_t> *a_cbits_n_ky,uint32_t &cbits,uint32_t &cbits_op,uint32_t &base_ky) {
			auto cbits_n_ky = a_cbits_n_ky->load(std::memory_order_acquire);
			cbits = ((uint32_t)cbits_n_ky & UINT32_MAX);
			cbits_op = 0;
			if ( !(is_base_noop(cbits)) &&  (cbits != 0) ) {
				cbits_op = cbits;
				cbits = fetch_real_cbits(cbits);
			}
			base_ky = (uint32_t)((cbits_n_ky >> sizeof(uint32_t)) & UINT32_MAX);
			return a_cbits_n_ky;
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


		/**
		 * stash_member_bits
		 * 
		 */

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

		void stash_cbits(atomic<uint32_t> *a_cbits, hh_element *base, uint32_t &c_bits, uint32_t &c_bits_op, uint32_t &c_bits_base_ops, CBIT_stash_holder *cbit_stashes[4]) {
			//
			if ( base == nullptr ) {	// not a member  i.e. this is a base

				if ( c_bits_op == 0 ) {  // means not yet stashed, zero or membership bits (so be the first to stash it)
					CBIT_stash_holder *csh = stash_base_bits(a_cbits, c_bits, c_bits_op);  // this puts bits (cbits) into the otherwise empty cell
					cbit_stashes[0] = csh;
				} else {				// increment the reference count and then go see what can be done by the calling thread
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


		/**
		 * _unstash_base_cbits
		 */

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


		/**
		 * _unstash_member_cbits
		 */

		void _unstash_member_cbits(atomic<uint32_t> *a_cbits,uint32_t mem_cbits,CBIT_stash_holder *csh) {
			csh->_updating--;
			if ( csh->_updating == 0 ) {
				uint32_t updating_cbits = 0;
				if ( mem_cbits & USURPED_CBIT_SET ) {  // restore to real roots
					updating_cbits = csh->_real_bits;
				} else {
					updating_cbits = csh->stored._csm._member_bits;
				}
				while ( !(a_cbits->compare_exchange_weak(mem_cbits,updating_cbits,std::memory_order_release)) && (mem_cbits != updating_cbits) );
			}
		}


		/**
		 * unstash_base_cbits
		 * 
		 */

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

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 



		/**
		 * stash_or_get_member_bits
		 * 
		 */

		CBIT_stash_holder 	*stash_or_get_member_bits(hh_element *mem, uint32_t &cbits_op, uint32_t modifier = 0) {
			//
			atomic<uint32_t> *a_mem_bits = (atomic<uint32_t> *)(&(mem->c.bits));
			cbits_op = a_mem_bits->fetch_or(modifier,std::memory_order_acquire); // some threads resond to the modifier...
			if ( is_member_bucket(cbits_op) ) {
				if ( cbits_op & modifier ) return nullptr; // accept that another thread has the op true when also stashed
				//
				auto stash_index = cbits_member_stash_index_of(cbits_op);
				if ( stash_index != 0 ) {
					CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
					return csh;
				} else {
					auto cbits = cbits_op;
					cbits_op |= modifier;
					CBIT_stash_holder *csh  = stash_member_bits(a_mem_bits,cbits,cbits_op);
					return csh;
				}
				//
			}
			return nullptr;
		}


		/**
		 * cbits_stash_swap_offset
		 */

		void cbits_stash_swap_offset(CBIT_stash_holder *csh1,CBIT_stash_holder *csh2) {
			auto mem_bits_1 = csh1->_real_bits;
			auto mem_bits_2 = csh2->_real_bits;
			//
			uint8_t back1 = member_backref_offset(mem_bits_1);
			uint8_t back2 = member_backref_offset(mem_bits_2);

			csh1->_real_bits = gen_bitsmember_backref_offset(mem_bits_1,back2);
			csh2->_real_bits = gen_bitsmember_backref_offset(mem_bits_2,back1);
		}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 

		// TBIT STASHING


		/**
		 * stash_taken_spots
		 * 
		 * 
		 */

		TBIT_stash_el *stash_taken_spots(atomic<uint32_t> *a_tbits) {
			//
			uint32_t tbits = a_tbits->load(std::memory_order_acquire);
			if ( is_base_tbits(tbits) || (tbits == 0) ) {
				//
				uint8_t stash_index = _tbit_stash.pop_one_wait_free();
				uint32_t tbits_op = tbit_stash_index_stamp(0,stash_index);
				//
				while ( !a_tbits->compare_exchange_weak(tbits,tbits_op,std::memory_order_acq_rel) ) {
					auto already_stashed = tbits_stash_index_of(tbits_op);
					if ( already_stashed != stash_index ) {
						// maybe update a stash counter
						TBIT_stash_el *tse = _tbit_stash.stash_el_reference(already_stashed);
						tse->_updating++;
						tbits = tse->_real_bits;
						_tbit_stash.return_one_to_free(stash_index);
						return tse;
					}
				}
				//
				TBIT_stash_el *tse = _tbit_stash.stash_el_reference(stash_index);
				tse->_real_bits = tbits;
				tse->_updating = 1; // this should be the first one.. 
				return tse;
			} else {
				auto already_stashed = tbits_stash_index_of(tbits);
				TBIT_stash_el *tse = _tbit_stash.stash_el_reference(already_stashed);
				if ( tse != nullptr ) {
					tbits = tse->_real_bits;
					tse->_updating++;
					return tse;
				}
			}

			return nullptr;
		}



		///
		/**
		 * _unstash_base_tbits
		 * 
		 */
		void _unstash_base_tbits(atomic<uint32_t> *a_tbits,TBIT_stash_el *tse) {
			if ( tse->_updating > 0 ) {
				tse->_updating--;
			}
			bool last_out = (tse->_updating == 0);
			if ( last_out ) {
				auto tbits_expected = a_tbits->load(std::memory_order_acquire);
				auto tbits_update = tse->_real_bits;
				tbits_update &= ~(tse->_remove_update);
				tbits_update |= tse->_add_update;
				while ( !(a_tbits->compare_exchange_weak(tbits_expected,tbits_update,std::memory_order_acq_rel)) ) {
					if ( tbits_expected & 0x1 ) break;
					if ( tse->_updating > 0 ) break;
				}
			}
		}


		void _unstash_base_tbits(hh_element *base,TBIT_stash_el *tse) {
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			 _unstash_base_tbits(a_tbits,tse);
		}



		/**
		 * stash_taken_spots
		 */


		TBIT_stash_el *stash_taken_spots(hh_element *nxt_base) {
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(nxt_base->tv.taken));
			return stash_taken_spots(a_tbits);
		}



		TBIT_stash_el *stash_taken_spots(hh_element *nxt_base,uint32_t &real_tbits) {
			TBIT_stash_el *tse = stash_taken_spots(nxt_base);
			real_tbits = tse->_real_bits;
			return tse;
		}




		TBIT_stash_el *stash_taken_spots(atomic<uint32_t> *a_tbits,uint32_t &real_tbits) {
			TBIT_stash_el *tse = stash_taken_spots(a_tbits);
			real_tbits = tse->_real_bits;
			return tse;
		}



		/// atomic<uint32_t> *a_tbits
		TBIT_stash_el *stash_taken_spots(atomic<uint32_t> *a_tbits,uint32_t &real_tbits,uint32_t &tbits_op) {
			TBIT_stash_el *tse = stash_taken_spots(a_tbits);
			real_tbits = tse->_real_bits;
			tbits_op = a_tbits->load(std::memory_order_acquire);
			return tse;
		}


		/**
		 * load_tbits
		 */


		atomic<uint32_t> *load_tbits(atomic<uint32_t> *a_tbits_base,uint32_t &tbits,uint32_t &tbits_op) {
			tbits_op = 0;
			tbits = a_tbits_base->load(std::memory_order_acquire);
			if ( !(is_base_noop(tbits)) &&  (tbits != 0) ) {
				tbits_op = tbits;		// these are the op tbits
				tbits = fetch_real_tbits(tbits);		// get the mem alloc pattern
			}
			return a_tbits_base;
		}



		/**
		 * load_tbits
		 */


		atomic<uint32_t> *load_tbits(hh_element *hash_base,uint32_t &tbits,uint32_t &tbits_op) {
			atomic<uint32_t> *a_tbits_base = (atomic<uint32_t> *)(&(hash_base->tv.taken));
			return load_tbits(a_tbits_base,tbits,tbits_op);
		}





		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * get_stash_updating_count
		 */

		uint32_t get_stash_updating_count(uint32_t cbits_op) {
			auto stash_index = cbits_stash_index_of(cbits_op);
			CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
			return csh->_updating;
		}

		/**
		 * get_stash_member_updating_count
		 */

		uint32_t get_stash_member_updating_count(uint32_t cbits_op) {
			auto stash_index = cbits_member_stash_index_of(cbits_op);
			CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);
			return csh->_updating;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



		/**
		 * is_deletion_marked
		 */

		bool is_deletion_marked(hh_element *bucket) {
			atomic<uint32_t> *a_b_cbits = (atomic<uint32_t> *)(&(bucket->c.bits));
			auto cbits = a_b_cbits->load(std::memory_order_acquire);
			if ( cbits & 0x1 ) return false;
			if ( cbits & DELETE_CBIT_SET ) return true;
			return false;
		}


		/**
		 * ready_for_delete
		 */

		bool ready_for_delete(hh_element *bucket,uint32_t &cbits) {
			atomic<uint32_t> *a_b_cbits = (atomic<uint32_t> *)(&(bucket->c.bits));
			cbits = a_b_cbits->load(std::memory_order_acquire);
			if ( cbits & 0x1 ) return false;
			if ( cbits & MOBILE_CBIT_SET ) return true;
			return false;
		}

		/**
		 * stash_key_value
		 * 
		 * The empty key value, put the key value into the cbit stash element 
		 * enabling temporary look up of an element in movement.
		 */

		void stash_key_value(uint32_t key, uint32_t value, CBIT_stash_holder *csh) {
			csh->_key = key;
			csh->_value = value;
		}



		/**
		 * stash_bits_clear
		 * 
		 * adds (by or) a membership bit(s) that will be cleared when the use of the stash ends.
		 */

		void stash_bits_clear(uint32_t clear_bits, CBIT_stash_holder *csh) {
			csh->stored._cse._remove_update |= clear_bits;
		}



		/**
		 * stash_real_cbits
		 * 
		 * adds (by or) a membership bit(s) that will be set when the use of the stash ends for a base element
		 * 
		 */


		void stash_real_cbits(uint32_t add_bits, CBIT_stash_holder *csh) {
			csh->stored._cse._add_update |= add_bits;
		}

		
		/**
		 * stash_real_cbits_immediate
		 * 
		 */

		void stash_real_cbits_immediate(uint32_t add_bits, CBIT_stash_holder *csh) {
			auto update = csh->stored._cse._add_update | add_bits;
			csh->stored._cse._add_update = update;
			csh->_real_bits = update;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * _check_key_value_stash
		 * 
		 * 
		 * Allows for a search to check for a key value pair that is being moved.
		 * 
		 */

		bool _check_key_value_stash(uint32_t cbits_op, uint32_t key, uint32_t &value) {
			//
			auto already_stashed = cbits_stash_index_of(cbits_op);
			CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(already_stashed);
			if ( csh != nullptr ) {
				if ( csh->_key == key ) {
					value = csh->_value;
					return true;
				}
			}
			//
			return false;
		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * stash_tbits_clear
		 * 
		 * adds (by or) a bit(s) that will be cleared from memory when the use of the stash ends.
		 */

		void stash_tbits_clear(uint32_t clear_bits, TBIT_stash_el *tse) {
			tse->_remove_update |= clear_bits;
		}



		/**
		 * stash_tbits_clear_immediate
		 * 
		 * adds (by or) a bit(s) that will be cleared from memory and updates the reat stashed tbits
		 * before the stash element is returned to stash memory.
		 * 
		 */

		uint32_t stash_tbits_clear_immediate(uint32_t clear_bits, TBIT_stash_el *tse) {
			auto update = tse->_remove_update | clear_bits;
			tse->_remove_update = update;
			auto tbits = tse->_real_bits & ~(update);
			tse->_real_bits = tbits;
			return tbits;
		}

		/**
		 * stash_real_tbits
		 * 
		 */

		void stash_real_tbits(uint32_t add_bits, TBIT_stash_el *tse) {
			tse->_add_update |= add_bits;
		}



		// TBITS READERS
		// 		-- 	alert operators that readers a searching a bucket with out regard to other ops
		// 			generally enforce a wait until readers a clear



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
		 * tbits_add_reader
		 * 
		 * calls `stash_taken_spots`
		 * 
		*/

		TBIT_stash_el *tbits_add_reader(atomic<uint32_t> *a_tbits, uint32_t &tbits) {
			//
			uint32_t tbits_op = 0;
			TBIT_stash_el *tse = stash_taken_spots(a_tbits,tbits,tbits_op);
			if ( tse != nullptr ) {
				if ( !(tbits_sem_at_max(tbits_op)) ) {
					a_tbits->fetch_add(TBITS_READER_SEM_INCR,std::memory_order_acq_rel);
				}
			}
			//
			return tse;
		}


		/**
		 * tbits_remove_reader
		*/

		void tbits_remove_reader(atomic<uint32_t> *a_tbits, TBIT_stash_el *tse_base) {
			//
			auto tbits = a_tbits->load(std::memory_order_acquire);
			//
			if ( tbits & 0x1 ) {  // already unstashed
				return;  // the tbits are where they are supposed to be
			}
			//
			if ( !(tbits_sem_at_zero(tbits)) ) {  // it should have been that the last condition returned (maybe in transition)
				a_tbits->fetch_sub(TBITS_READER_SEM_INCR,std::memory_order_acq_rel);
			}
			//
			_unstash_base_tbits(a_tbits,tse_base);    // unstash if last out
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


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// TBITS SWAPPY OP
		//		--	alert readers and some operators that elements of interest will be trading places
		//			during cropping

		/**
		 * tbits_add_swappy_op
		*/

		TBIT_stash_el *tbits_add_swappy_op(hh_element *base,uint32_t &tbits) {
			atomic<uint32_t> *a_tbits = (atomic<uint32_t> *)(&(base->tv.taken));
			return tbits_add_swappy_op(a_tbits,tbits);
		}

		TBIT_stash_el *tbits_add_swappy_op(atomic<uint32_t> *a_tbits,uint32_t &tbits) {
			//
			uint32_t tbits_op = 0;
			TBIT_stash_el *tse = stash_taken_spots(a_tbits,tbits,tbits_op);
			if ( tse != nullptr ) {
				if ( !(tbits_swappy_at_max(tbits_op)) ) {
					a_tbits->fetch_add(TBITS_READER_SWPY_INCR,std::memory_order_acq_rel);
				}
			}
			//
			return tse;
		}


		/**
		 * tbits_remove_swappy_op
		*/
		void tbits_remove_swappy_op(atomic<uint32_t> *a_tbits,TBIT_stash_el *tse_base) {

			auto tbits = a_tbits->load(std::memory_order_acquire);
			//
			if ( tbits & 0x1 ) {  // already unstashed
				return;  // the tbits are where they are supposed to be
			}
			//
			if ( !(tbits_swappy_at_zero(tbits)) ) {  // it should have been that the last condition returned (maybe in transition)
				a_tbits->fetch_sub(TBITS_READER_SWPY_INCR,std::memory_order_acq_rel);
			}
			//
			_unstash_base_tbits(a_tbits,tse_base);    // unstash if last out

		}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * wait_on_reader_count_incr
		*/

		void wait_on_reader_count_incr(atomic<uint32_t> *control_bits) {
			//
			uint32_t cbits_op = control_bits->load(std::memory_order_acquire);
			while ( cbits_reader_count_at_max(cbits_op) ) { 
				tick();
				cbits_op = control_bits->load(std::memory_order_acquire);
			}
			control_bits->fetch_add(CBIT_MEM_R_COUNT_INCR,std::memory_order_acq_rel);
			//
		}

		// q_count -- counts the work that will be done for a particular cell in sequence.
		//

		/**
		 * reader_count_decr
		 * 
		 * 
		 */


		inline bool reader_count_decr(atomic<uint32_t> *control_bits) {
			//
			uint32_t cbits_op = control_bits->load(std::memory_order_acquire);
			//
			if ( !(cbits_reader_count_at_zero(cbits_op)) ) {  // it should have been that the last condition returned (maybe in transition)
				auto prior_cbits = control_bits->fetch_sub(CBIT_MEM_R_COUNT_INCR,std::memory_order_acq_rel);
				if ( cbits_reader_count_at_zero(prior_cbits - CBIT_MEM_R_COUNT_INCR) ) {  // check against precondition
					// note that the member is not being unstashed as part of the operation
					return true;
				}
			}
			//
			return false;
		}


		/**
		 * wait_on_max_queue_incr
		*/

		bool wait_on_max_queue_incr(atomic<uint32_t> *control_bits,uint32_t &cbits_op) {

			auto stash_index = cbits_stash_index_of(cbits_op);   // stash_index == 0 -> program broken upstream
			CBIT_stash_holder *csh = _cbit_stash.stash_el_reference(stash_index);

			if ( csh != nullptr ) {
				while ( cbits_q_count_at_max(cbits_op) ) { 
					tick();
					cbits_op = control_bits->load(std::memory_order_acquire);
				}
				control_bits->fetch_add(CBIT_Q_COUNT_INCR,std::memory_order_acq_rel);
				csh->_updating++;
				return true;
			}

			return false;;
		}

		// q_count -- counts the work that will be done for a particular cell in sequence.
		//

		/**
		 * q_count_decr
		 * 
		 * 
		 */


		inline bool q_count_decr(atomic<uint32_t> *control_bits, uint32_t &cbits_op) {

			cbits_op = control_bits->load(std::memory_order_acquire);
			//
			if ( cbits_op & 0x1 ) {  // already unstashed
				return true;  // the tbits are where they are supposed to be
			}
			auto stash_index = cbits_stash_index_of(cbits_op);
			auto csh = _cbit_stash.stash_el_reference(stash_index);
			//
			if ( !(cbits_q_count_at_zero(cbits_op)) ) {  // it should have been that the last condition returned (maybe in transition)
				auto prior_cbits = control_bits->fetch_sub(CBIT_Q_COUNT_INCR,std::memory_order_acq_rel);
				if ( cbits_q_count_at_zero(prior_cbits - CBIT_Q_COUNT_INCR) ) {
					_unstash_base_cbits(control_bits,csh);
					return true;
				}
			}
			//
			return false;
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



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		/**
		 * _get_member_bits_slice_info
		 * 
		 * Only one parameter is passed as true input. All the others are by reference. 
		 * 
		 * 	This method looks at the number of bits in the membership of the `h_bucket` bucket in each slice. Or, it looks at
		 *  the base of the bucket if the bucket is a member. The slice used for the new element will be the bucket 
		 * 	that has the least number of members. If there is a tie, this method picks a bucket at random.
		 * 
		 * 	Finally, this method attempts to stash the bucket if it is not already stashed. If it is already stashed, 
		 * 	the stashing process will increment the bucket `_updating` count (atomically). The updating count is basically
		 * 	a reference count.
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
		atomic<uint32_t> *_get_member_bits_slice_info(uint32_t h_bucket,uint8_t &which_table,uint32_t &c_bits,uint32_t &c_bits_op,uint32_t &c_bits_base_ops,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref,CBIT_stash_holder *cbit_stashes[4]) {
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
			hh_element *buffer0		= _region_HV_0;
			hh_element *end_buffer0	= _region_HV_0_end;
			//
			hh_element *el_0 = buffer0 + h_bucket;
			atomic<uint32_t> *a_cbits0 = (atomic<uint32_t> *)(&(el_0->c.bits));
			auto c0 = a_cbits0->load(std::memory_order_acquire);
			uint32_t c0_ops = 0;
			uint8_t backref0 = 0;
			uint32_t c0_op_base = 0;
			hh_element *base0 = nullptr;		// base0 stays null unless a member refers to a base
			//
			if ( c0 != 0 ) {  					// no elements
				if ( is_base_noop(c0) ) {		// the cbits are a membership map (lsb bit is 1)
					count0 = popcount(c0);
				} else {
					if ( is_base_in_ops(c0) ) {	// otherwise, it must already be stashed or be a member
						auto real_bits = fetch_real_cbits(c0);	// membership bits from stash
						c0_ops = c0;
						c0 = real_bits;
						count0 = popcount(real_bits);
					} else if ( is_member_bucket(c0) ) {	// otherwise, this is a member
						c0_ops = c0;
						base0 = cbits_base_from_backref_or_stashed_backref(c0,backref0,el_0,buffer0,end_buffer0);
						load_cbits(base0,c0,c0_op_base);
						count0 = 1;   // indicates the member element ... some work will be done to reinsert the previously stored member
					}
				}
			}
			//
			// look in the buffer 1
			hh_element *buffer1		= _region_HV_1;
			hh_element *end_buffer1	= _region_HV_1_end;
			//
			hh_element *el_1 = buffer1 + h_bucket;
			atomic<uint32_t> *a_cbits1 = (atomic<uint32_t> *)(&(el_1->c.bits));
			auto c1 = a_cbits1->load(std::memory_order_acquire);
			uint32_t c1_ops = 0;
			uint8_t backref1 = 0;
			uint32_t c1_op_base = 0;
			hh_element *base1 = nullptr;		// base1 stays null unless a member refers to a base
			//
			if ( c1 != 0 ) {  					// no elements
				if ( is_base_noop(c1) ) {		// the cbits are a membership map (lsb bit is 1)
					count1 = popcount(c1);
				} else {
					if ( is_base_in_ops(c1) ) {	// otherwise, it must already be stashed or be a member
						auto real_bits = fetch_real_cbits(c1);	// membership bits from stash
						c1_ops = c1;
						c1 = real_bits;
						count1 = popcount(real_bits);
					} else if ( is_member_bucket(c0) ) {
						c1_ops = c1;
						base1 = cbits_base_from_backref_or_stashed_backref(c1,backref1,el_1,buffer1,end_buffer1);
						load_cbits(base1,c1,c1_op_base);
						count1 = 1;   // indicates the member element ... some work will be done to reinsert the previously stored member
					}
				}
			}
			//	make selection of slice
			auto selector = _hlpr_select_insert_buffer(count0,count1);
			if ( selector == 0xFF ) return nullptr;		// selection failed (error state)
			//
			// confirmed... set output parameters
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
				base = base0;		// may be null if el_0 is a base
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
				base = base1;		// may be null if el_1 is a base
				a_cbits = a_cbits1;
			}
			//

			// stash the cell for operation cooperation
			stash_cbits(a_cbits,base,c_bits,c_bits_op,c_bits_base_ops,cbit_stashes);
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

		atomic_flag		 				*_rand_gen_thread_waiting_spinner;
		atomic_flag		 				*_random_share_lock;

		atomic<uint32_t>				*_random_gen_region;


		FreeOperatorStashStack<CBIT_stash_holder,MAX_THREADS,EXPECTED_THREAD_REENTRIES>		_cbit_stash;
		FreeOperatorStashStack<TBIT_stash_el,MAX_THREADS,EXPECTED_THREAD_REENTRIES>			_tbit_stash;


/**
 * SUB: HH_thread_manager -- could be a subclass, but everything is one class.
 * 
*/

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
		void enqueue_restore(hh_adder_states update_type, atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t cbits_op, uint32_t cbits_op_base, sp_element *hash_ref, uint32_t h_bucket, uint32_t el_key, uint32_t value, uint8_t which_table, hh_element *buffer, hh_element *end,uint8_t require_queue) {
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
		void dequeue_restore(hh_adder_states &update_type, atomic<uint32_t> **control_bits_ref, uint32_t &cbits, uint32_t &cbits_op, uint32_t &cbits_op_base, sp_element **hash_ref_ref, uint32_t &h_bucket, uint32_t &el_key, uint32_t &value, uint8_t &which_table, uint8_t assigned_thread_id, hh_element **buffer_ref, hh_element **end_ref) {
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
			sp_element *hash_ref = get_entry.hash_ref;
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
		void enqueue_cropping(sp_element *hash_ref,uint32_t cbits,hh_element *buffer,hh_element *end,uint8_t which_table) {
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
		void dequeue_cropping(sp_element **hash_ref_ref, uint32_t &cbits, uint8_t &which_table, uint8_t assigned_thread_id , hh_element **buffer_ref, hh_element **end_ref) {
			//
			crop_entry get_entry;
			//
			proc_descr *p = _process_table + assigned_thread_id;
			//
			p->_to_cropping[which_table].pop(get_entry); // by ref
			//
			sp_element *hash_ref = get_entry.hash_ref;
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

		void wakeup_value_restore(hh_adder_states update_type, atomic<uint32_t> *control_bits, uint32_t cbits, uint32_t cbits_op, uint32_t cbits_op_base, hh_element *bucket, uint32_t h_start, uint32_t el_key, uint32_t value, uint8_t which_table, hh_element *buffer, hh_element *end, CBIT_stash_holder *csh) {
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

		void submit_for_cropping(hh_element *base,uint32_t cbits,hh_element *buffer,hh_element *end,uint8_t which_table) {
			enqueue_cropping(base,cbits,buffer,end,which_table);
			wake_up_one_cropping();
		}


	public: 

		// threads ...
		proc_descr						*_process_table;						
		proc_descr						*_end_procs;

		uint8_t							_round_robbin_proc_table_threads{1};
		//
		atomic_flag						_sleeping_reclaimer;
		atomic_flag						_sleeping_cropper;

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

		inline bool empty_bucket(atomic<uint64_t> *a_c,uint32_t &cbits,uint32_t &op_cbits,uint32_t &root_ky) {
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

		inline bool empty_bucket(atomic<uint64_t> *a_c,uint32_t &cbits,uint32_t &op_cbits) {
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

		void _adder_bucket_queue_release(atomic<uint32_t> *control_bits, uint32_t el_key, uint32_t h_bucket, uint32_t offset_value, uint8_t which_table, uint32_t cbits, uint32_t cbits_op,uint32_t cbits_base_op, hh_element *bucket, hh_element *buffer, hh_element *end_buffer,CBIT_stash_holder *cbit_stashes[4]) {
			//
			wakeup_value_restore(HH_FROM_BASE_AND_WAIT, control_bits, cbits, cbits_op, cbits_base_op, bucket, h_bucket, tmp_key, tmp_value, which_table, buffer, end_buffer, cbit_stashes[0]);
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

		void _cropper(sp_element *base, hh_element *buffer, hh_element *end) {
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
			if ( (_SP.bytes_needed(byte)/2) > base->_bucket_count ) {
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
		void wait_for_readers(sp_element *base,bool lock) {
			// ----
		}

		void release_to_readers(sp_element *base) {
			// ----
		}

		void expand_base(sp_element *base) {
			auto st = base->_slab_type;
			auto si = base->_slab_index;
			auto so = base->_slab_offset;
			_SP.expand(st,si,so)
		}

		void contract_base(sp_element *base) {

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
			hh_element *buffer = nullptr;
			hh_element *end_buffer = nullptr;
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
					hh_element *oldest = pop_oldest(elements_buffer);
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
			hh_element *buffer = nullptr;
			hh_element *end = nullptr;
			//
			while ( is_cropping_queue_empty(assigned_thread_id,slice_for_thread) ) wait_notification_cropping();
			//
			dequeue_cropping(&base, cbits, which_table, assigned_thread_id, &buffer, &end);
			// store... if here, it should be locked
			// cbits, 
			_cropper(base, buffer, end);
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
			//
			atomic<uint32_t> *a_c_bits = _get_member_bits_slice_info(h_bucket,which_table,cbits,cbits_op,cbits_base_ops,bucket_ref,buffer_ref,end_buffer_ref,cbit_stashes);
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
				_adder_bucket_queue_release(control_bits,el_key,h_bucket,offset_value,which_table,cbits,cbits_op,cbits_base_op,bucket,buffer,end_buffer,cbit_stashes);
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
			//
			auto btype = base->_slab_type;
			uint16_t bytes_needed = _SP.bytes_needed(btype);
			//
			uint8_t elements_buffer[bytes_needed];
			hh_element *el = (hh_element *)(&elements_buffer[0]);
			hh_element *end_els = (hh_element *)(&elements_buffer[0] + bytes_needed);

			_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);

			auto count = base->_bucket_count;
			while ( (el < end_els) && (count > 0)) {
				if ( el->c.key != UINT32_MAX ) {
					if ( el->c.key == el_key ) {
						return el->tv.value;
					}
				}
				el++; count--;
			}

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
			//
			// WAIT
			wait_for_readers(base,true);
			//
			auto btype = base->_slab_type;
			uint16_t bytes_needed = _SP.bytes_needed(btype);
			//
			uint8_t elements_buffer[bytes_needed];
			hh_element *el = (hh_element *)(&elements_buffer[0]);
			hh_element *end_els = (hh_element *)(&elements_buffer[0] + bytes_needed);

			_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);

			auto count = base->_bucket_count;
			while ( (el < end_els) && (count > 0)) {
				if ( el->c.key != UINT32_MAX ) {
					if ( el->c.key == el_key ) {
						el->tv.value = v_value;
						_SP.unload_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
						return loaded_key;
					}
				}
				el++; count--;
			}
			release_to_readers(base);
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
			atomic<uint64_t> *a_c_bits = (atomic<uint64_t> *)(&(base->c));
			uint32_t cbits = 0;
			uint32_t op_cbits = 0;
			uint32_t root_ky = 0;
			if ( empty_bucket(a_c_bits,cbits,op_cbits,root_ky) ) return UINT32_MAX;   // empty_bucket cbits by ref
			//
			// WAIT
			wait_for_readers(base,true);
			//
			auto btype = base->_slab_type;
			uint16_t bytes_needed = _SP.bytes_needed(btype);
			//
			uint8_t elements_buffer[bytes_needed];
			hh_element *el = (hh_element *)(&elements_buffer[0]);
			hh_element *end_els = (hh_element *)(&elements_buffer[0] + bytes_needed);

			_SP.load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);

			auto count = base->_bucket_count;
			while ( (el < end_els) && (count > 0)) {
				if ( el->c.key != UINT32_MAX ) {
					if ( el->c.key == el_key ) {
						el->tv.value = 0;
						el->c.key = UINT32_MAX;
						el->tv.taken = 0;
						_SP.unload_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed);
						submit_for_cropping(base,cbits,buffer,end,selector);  // after a total swappy read, all BLACK keys will be at the end of members
						release_to_readers(base);
						return el_key;
					}
				}
				el++; count--;
			}
			release_to_readers(base);
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


#endif // _H_HOPSCOTCH_HASH_SHM_