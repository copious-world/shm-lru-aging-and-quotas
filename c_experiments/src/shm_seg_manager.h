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

#include "shm_shared_segs.h"

#include "node_shm_HH.h"
#include "node_shm_LRU_defs.h"

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
 * SharedSegmentsManager
*/

template<class HH>
class SharedSegmentsTForm : public SharedSegments {

	public:

		SharedSegmentsTForm() {
			_container_node_size = sizeof(uint32_t)*8;
		}
		virtual ~SharedSegmentsTForm() {}

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
		 * initialize_com_shm
		*/

		int initialize_com_shm(key_t com_key, bool am_initializer, uint32_t num_procs, uint32_t num_tiers) {
			//
			int status = 0;
			//
			_com_buffer_size = LRU_Consts::check_expected_com_size(num_procs,num_tiers);
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


		/**
		 * initialize_randoms_shm
		 */
		int initialize_randoms_shm(key_t randoms_key, bool am_initializer) {
			int status = 0;
			//
			size_t seg_size = Random_bits_generator<>::check_expected_region_size;
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


		/**
		 * initialize_lru_shm
		 */

		int initialize_lru_shm(key_t key, bool am_initializer, uint32_t max_obj_size, uint32_t num_procs, uint32_t els_per_tier) {
			int status = 0;
			//
			size_t seg_size = LRU_Consts::check_expected_lru_region_size(max_obj_size,els_per_tier,num_procs);
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
			//
			return status;
		}


		/**
		 * initialize_hmm_shm
		 */
		int initialize_hmm_shm(key_t key,  bool am_initializer, uint32_t els_per_tier,uint32_t num_threads) {
			int status = 0;
			size_t seg_size = HH::check_expected_hh_region_size(els_per_tier,num_threads);
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

		/**
		 * tier_segments_initializers
		 */

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
				status = this->initialize_hmm_shm(hh_key,am_initializer,els_per_tier,num_procs);

				if ( status != 0 ) { return status; }
			}
			//
			return 0;
		}


		/**
		 * region_intialization_ops
		 */

		int region_intialization_ops(list<uint32_t> &lru_keys,list<uint32_t> &hh_keys, bool am_initializer, 
										uint32_t num_procs, uint32_t num_tiers, uint32_t els_per_tier,
											uint32_t max_obj_size, key_t com_key, key_t randoms_key = 0) {
			int status = 0;
			//
			status = this->initialize_com_shm(com_key,am_initializer,num_procs,num_tiers);
			if ( status != 0 ) return status;
			//
			if ( randoms_key != 0 ) {
				status = this->initialize_randoms_shm(randoms_key,am_initializer);
				if ( status != 0 ) return status;
			} else {
				_random_bits_buffer = nullptr;
			}
			//
			status = this->tier_segments_initializers(am_initializer,lru_keys,hh_keys,max_obj_size,num_procs,els_per_tier);
			if ( status != 0 ) return status;
			//
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






class SharedSegmentsManager : public SharedSegmentsTForm<HH_map<>> {

	public:

		SharedSegmentsManager() {}
		virtual ~SharedSegmentsManager() {}

};



}


