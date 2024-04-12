#pragma once

// ---- ---- ---- ---- ---- ---- ---- ----

/**
 * shm shared segs 
 * 
 * This module (header) preserves some of the wrapped shared memory operations for use across a number of projects.
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


// lsof | egrep "98306|COMMAND"
// ipcs -mp


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


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

		int detach(key_t key,bool forceDestroy) {
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
		 * get_addr
		*/
		size_t get_size(key_t key) {
			auto sz = _ids_to_seg_sizes[key];
			if ( sz == 0 ) {
				sz = get_seg_size(key);
				_ids_to_seg_sizes[key] = sz;
			}
			return sz;
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


