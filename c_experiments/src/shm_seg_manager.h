#pragma once

// ---- ---- ---- ---- ---- ---- ---- ----

/**
 * shm seg manager 
 * 
 * This module is fairly custom. While the shared segment management is 
 * fairly common on POSIX style machines, the number and size of the regions are determined here
 * specifically for use with the atomic LRU and Hopscotch Hash table using random table selection.
 * 
**/

// ---- ---- ---- ---- ---- ---- ---- ----

#include <errno.h>

// ---- ---- ---- ---- ---- ---- ---- ----

#include <unistd.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>

#include <iostream>
#include <sstream>

// ---- ---- ---- ---- ---- ---- ---- ----


#ifndef _POSIX_THREAD_PROCESS_SHARED
#warning This system does not support process shared mutex -- alternative method will be used
#endif


using namespace std;


#include <map>
#include <unordered_map>
#include <list>

#include "node_shm_HH.h"
#include "node_shm_LRU.h"




#define NUM_ATOMIC_FLAG_OPS_PER_TIER		(4)
#define LRU_ATOMIC_HEADER_WORDS				(8)


// lsof | egrep "98306|COMMAND"
// ipcs -mp



namespace node_shm {

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

const uint32_t keyMin = 1;
const uint32_t keyMax = UINT32_MAX - keyMin;
const uint32_t lengthMin = 1;
const uint32_t lengthMax = UINT16_MAX;   // for now


/**
 * SharedSegments
*/
class SharedSegments {
	public:

		SharedSegments() {}
		virtual ~SharedSegments() {}

	public:

		/**
		 * shm_getter
		*/
		int shm_getter(key_t key, int at_shmflg,  int shmflg = 0, bool isCreate = false, size_t size = 0) {
			//
			int res_id = shmget(key, size, shmflg);
			//
			if ( res_id == -1 ) {
				switch(errno) {
					case EEXIST: // already exists
					case EIDRM:  // scheduled for deletion
					case ENOENT: // not exists
						return -1;
					case EINVAL: // should be SHMMIN <= size <= SHMMAX
						return -2;
					default:
						return -2;  // tells caller to get the errno
				}
			} else {
				//
				if ( !isCreate ) {		// means to attach.... 
					//
					struct shmid_ds shminf;
					//
					int err = shmctl(res_id, IPC_STAT, &shminf);
					if ( err == 0 ) {
						size = shminf.shm_segsz;   // get the seg size from the system
						_ids_to_seg_sizes[key] = size;
					} else {
						return -2;							
					}
				} else {
					_ids_to_seg_sizes[key] = size;
				}
				//
				void* res = shmat(res_id, NULL, at_shmflg);
				//
				if ( res == (void *)-1 ) return -2;
				//
				_ids_to_seg_addrs[key] = res;
			}
			
			return 0;
		}

		/**
		 * get_seg_size
		*/
		size_t get_seg_size(key_t key) {
			int res_id = shmget(key, 0, 0);
			if (res_id == -1) {
				switch(errno) {
					case ENOENT: // not exists
					case EIDRM:  // scheduled for deletion
						return -1;
					default:
						return -2;
				}
			}
			struct shmid_ds shminf;
			size_t seg_size;
			//
			int err = shmctl(res_id, IPC_STAT, &shminf);
			if ( err == 0 ) {
				seg_size = shminf.shm_segsz;   // get the seg size from the system
				_ids_to_seg_sizes[key] = seg_size;
			} else {
				return -2;							
			}
			return seg_size;
		}


		/**
		 * _detach_op
		*/
		int _detach_op(key_t key, bool force, bool onExit = false) {
			//
			int resId = this->key_to_id(key);
			if ( resId < 0 ) return resId;
			//

			void *addr = _ids_to_seg_addrs[key];
			struct shmid_ds shminf;
			int err = shmdt(addr);
			if ( err ) {
				if ( !(onExit) ) return -2;
				return err;
			}
				//get stat
			err = shmctl(resId, IPC_STAT, &shminf);
			if ( err ) {
				if ( !(onExit) ) return -2;
				return err;
			}
			//destroy if there are no more attaches or force==true
			if ( force || shminf.shm_nattch == 0 ) {
				//
				err = shmctl(resId, IPC_RMID, 0);
				if ( err ) {
					if ( !(onExit) ) return -2;
					return err;
				}
				//
				_ids_to_seg_addrs.erase(key);
				_ids_to_seg_sizes.erase(key);
				//
				return 0;
			} else {
				return shminf.shm_nattch; //detached, but not destroyed
			}
			return -1;
		}

		/**
		 * detach
		*/
		void _application_removals([[maybe_unused]] key_t key) {}

		size_t detach(key_t key,bool forceDestroy) {
			//
			int status = this->_detach_op(key,forceDestroy);
			if ( status == 0 ) {
				_application_removals(key);
				return _ids_to_seg_addrs.size();   // how many segs now?
			}
			//
			return status;
		}


		/**
		 * detach_all
		*/

		pair<uint16_t,size_t> detach_all(bool forceDestroy = false) {
			unsigned int deleted = 0;
			size_t total_freed = 0;
			while ( _ids_to_seg_sizes.size() > 0 ) {
				auto p = *(_ids_to_seg_sizes.begin());
				key_t key = p.first;
				if ( this->detach(key,forceDestroy) >= 0 ) {
					deleted++;
					total_freed += p.second;
				}
			}
			return pair<uint16_t,size_t>(deleted,total_freed);
		}



		/**
		 * key_to_id
		*/
		int key_to_id(key_t key) {
			int resId = shmget(key, 0, 0);
			if ( resId == -1 ) {
				switch(errno) {
					case ENOENT: // not exists
					case EIDRM:  // scheduled for deletion
						return(-1);
					default:
						return(-1);
				}
			}
			return resId;
		}


		/**
		 * total_mem_allocated
		*/
		size_t total_mem_allocated(void) {
			size_t total_mem = 0;
			for ( auto p : _ids_to_seg_sizes ) {
				total_mem += p.second;
			}
			return total_mem;
		}


		/**
		 * check_key
		*/
		bool check_key(key_t key) {
			if ( auto search = _ids_to_seg_addrs.find(key); search != _ids_to_seg_addrs.end() ) return true;
			int resId =  this->key_to_id(key);
			if ( resId == -1 ) { return false; }
			return true;
		}

		/**
		 * get_addr
		*/
		void *get_addr(key_t key) {
			auto seg = _ids_to_seg_addrs[key];
			return seg;
		}

		/**
		 * _shm_creator
		*/

		int _shm_creator(key_t key,size_t seg_size) {
			auto perm = 0660;
			int at_shmflg = 0;
			int shmflg = IPC_CREAT | IPC_EXCL | perm;
			int status = this->shm_getter(key, at_shmflg, shmflg, true, seg_size);
			return status;
		}
		
		int _shm_attacher(key_t key,int at_shmflg) {
			int status = this->shm_getter(key, at_shmflg);
			return status;
		}


	public:

		map<key_t,void *>					_ids_to_seg_addrs;
		map<key_t,size_t> 					_ids_to_seg_sizes;

};



/**
 * SharedSegmentsManager
*/

class SharedSegmentsManager : public SharedSegments {

	public:

		SharedSegmentsManager() {
			_container_node_size = sizeof(uint32_t)*8;
		}
		virtual ~SharedSegmentsManager() {}

	public:

		/**
		 * remove_if_lru
		*/

		void remove_if_lru(key_t key) {
			if ( auto search = _seg_to_lrus.find(key); search != _seg_to_lrus.end() ) {
				_seg_to_lrus.erase(key);
			}
		}

		/**
		 * remove_if_hh_map
		*/

		void remove_if_hh_map(key_t key) {
			if ( auto search = _seg_to_hh_tables.find(key); search != _seg_to_hh_tables.end() ) {
				_seg_to_hh_tables.erase(key);
			}
		}

		void _application_removals(key_t key) {
			this->remove_if_lru(key);
			this->remove_if_hh_map(key);
		}

		/**
		 * initialize_com
		*/

		// return ((Com_element *)(_com_buffer + _NTiers*sizeof(atomic_flag *));
		// _owner_proc_area = ((Com_element *)(_com_buffer + _NTiers*sizeof(atomic_flag *)) + (_proc*_NTiers);

		int initialize_com_shm(key_t com_key, bool am_initializer, uint32_t num_procs, uint32_t num_tiers) {
			//
			int status = 0;
			//
			size_t tier_atomics_sz = NUM_ATOMIC_FLAG_OPS_PER_TIER*num_tiers*sizeof(atomic_flag *);  // ref to the atomic flag
			size_t proc_tier_com_sz = sizeof(Com_element)*num_procs*num_tiers;
			//
			size_t seg_size = tier_atomics_sz + proc_tier_com_sz;
			_com_buffer_size = seg_size;
			//
			if ( am_initializer ) {
				status = _shm_creator(com_key,seg_size);
			} else {
				int at_shmflg = 0;
				status = this->_shm_attacher(com_key, at_shmflg);
			}
			//
			if ( status == 0 ) _com_buffer = _ids_to_seg_addrs[com_key];
			//
			return status;
		}


		// initialize_randoms_shm
		//
		int initialize_randoms_shm(key_t randoms_key, bool am_initializer) {
			int status = 0;
			//
			size_t tier_atomics_sz = 4*sizeof(uint32_t);  // ref to the atomic flag
			size_t bit_word_store_sz = 256;
			//
			size_t seg_size = 4*(tier_atomics_sz + bit_word_store_sz);
			_random_bits_buffer_size = seg_size;
			//
			if ( am_initializer ) {
				status = _shm_creator(randoms_key,seg_size);
			} else {
				int at_shmflg = 0;
				status = this->_shm_attacher(randoms_key, at_shmflg);
			}
			//
			if ( status == 0 ) _random_bits_buffer = _ids_to_seg_addrs[randoms_key];
			//
			return status;
		}


		// _step = (sizeof(LRU_element) + _record_size);
		// _lb_time = (atomic<uint32_t> *)(region);   // these are governing time boundaries of the particular tier
		// _ub_time = _lb_time + 1;
		// _cascaded_com_area = (Com_element *)(_ub_time + 1);
		// _end_cascaded_com_area = _cascaded_com_area + _Procs;
		// initialize_com_area(num_procs)  .. 
		// _max_count*_step;
		// _reserve_end = _region + _region_size;
		// _reserve_start = end;
		// _reserve_count_free = factor*num_procs// (_max_count/100)*_reserve_percent;

		int initialize_lru_shm(key_t key, bool am_initializer, uint32_t max_obj_size, uint32_t num_procs, uint32_t els_per_tier) {
			int status = 0;
			//
			size_t boundaries_atomics_sz = LRU_ATOMIC_HEADER_WORDS*sizeof(atomic<uint32_t>);		// atomics used to gain control of specific ops
			size_t com_read_per_proc_sz = sizeof(Com_element)*num_procs;	// for requesting and returning values 
			size_t max_count_lru_regions_sz = (sizeof(LRU_element) + max_obj_size)*(els_per_tier  + 4); // storage
			size_t holey_buffer_sz = sizeof(pair<uint32_t,uint32_t>)*els_per_tier + sizeof(pair<uint32_t,uint32_t>)*num_procs; // storage for timeout management
			//
			size_t seg_size = (boundaries_atomics_sz + com_read_per_proc_sz + max_count_lru_regions_sz + holey_buffer_sz);
			//
			if ( am_initializer ) {
				status = _shm_creator(key,seg_size);
			} else {
				int at_shmflg = 0;
				status = this->_shm_attacher(key, at_shmflg);
			}
			//
			if ( status == 0 ) {
				_seg_to_lrus[key] = _ids_to_seg_addrs[key];
			}
			return status;
		}

		// x2 the sum of the following
		//uint32_t v_regions_size = (sizeof(uint64_t)*max_count);
		//uint32_t h_regions_size = (sizeof(uint32_t)*max_count);
		//sz = sizeof(HHash)
		//uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));

		int initialize_hmm_shm(key_t key,  bool am_initializer, uint32_t els_per_tier) {
			int status = 0;
			//
			size_t hhash_header_allotment_sz = 2*sizeof(HHash);
			size_t value_reagion_sz = sizeof(uint64_t)*els_per_tier;
			size_t bucket_region_sz = sizeof(uint32_t)*els_per_tier;
			size_t control_bits_sz = sizeof(atomic<uint32_t>)*els_per_tier;
			size_t seg_size = hhash_header_allotment_sz + value_reagion_sz + bucket_region_sz + control_bits_sz;
			//
			if ( am_initializer ) {
				status = _shm_creator(key,seg_size);
			} else {
				int at_shmflg = 0;
				status = this->_shm_attacher(key, at_shmflg);
			}
			if ( status == 0 ) {
				_seg_to_hh_tables[key] = _ids_to_seg_addrs[key];
			}
			return status;
		}


		//  tier_segments_initializers

		int tier_segments_initializers(bool am_initializer,list<uint32_t> &lru_keys,list<uint32_t> &hh_keys,uint32_t max_obj_size,uint32_t num_procs,uint32_t els_per_tier) {
			//
			int status = 0;
			for ( auto lru_key : lru_keys ) {
				if ( lru_key < keyMin || lru_key >= keyMax ) {
					return -1;
				}
				status = this->initialize_lru_shm(lru_key,am_initializer,max_obj_size,num_procs,els_per_tier);

				if ( status != 0 ) { return status; }
			}
			for ( auto hh_key : hh_keys ) {
				if ( hh_key < keyMin || hh_key >= keyMax ) {
					return -1;
				}
				status = this->initialize_hmm_shm(hh_key,am_initializer,els_per_tier);

				if ( status != 0 ) { return status; }
			}
			//
			return 0;
		}


		// region_intialization_ops
		//
		int region_intialization_ops(list<uint32_t> &lru_keys,list<uint32_t> &hh_keys, bool am_initializer, 
										uint32_t num_procs, uint32_t num_tiers, uint32_t els_per_tier,
											uint32_t max_obj_size, key_t com_key, key_t randoms_key) {
			int status = 0;
			
			status = this->initialize_com_shm(com_key,am_initializer,num_procs,num_tiers);
			if ( status != 0 ) return status;

			status = this->initialize_randoms_shm(randoms_key,am_initializer);
			if ( status != 0 ) return status;

			status = this->tier_segments_initializers(am_initializer,lru_keys,hh_keys,max_obj_size,num_procs,els_per_tier);
			if ( status != 0 ) return status;
			
			return status;
		}


	public:

		//
		void 								*_com_buffer;
		void								*_random_bits_buffer;
		size_t								_com_buffer_size;
		size_t								_random_bits_buffer_size;
		//
		map<key_t,void *>					_seg_to_lrus;
		map<key_t,void *>					_seg_to_hh_tables;
		//
		size_t								_container_node_size;

};

}

