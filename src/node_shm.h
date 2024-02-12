// ---- ---- ---- ---- ---- ---- ---- ----

#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <nan.h>
#include <errno.h>
#include <asm/errno.h>

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


using namespace node;
using namespace v8;
using namespace std;


#include <map>
#include <unordered_map>
#include <list>

#include "node_shm_HH.h"
#include "node_shm_LRU.h"




#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;
#define SAFE_DELETE_ARR(a) if( (a) != NULL ) delete [] (a); (a) = NULL;


#define NUM_ATOMIC_FLAG_OPS_PER_TIER		(4)


namespace node {
namespace node_shm {



	template<typename K,typename V>
	inline void js_map_maker_destruct(map<K,V> &jmap,Local<Object> &jsObject) {
		if ( jmap.size() > 0 ) {
			for ( auto p : jmap ) {
				stringstream ss;
				ss << p.first;
				string key = ss.str();
				//
				Local<String> propName = Nan::New(key).ToLocalChecked();
				Local<String> propValue = Nan::New(p.second).ToLocalChecked();
				//
				Nan::Set(jsObject, propName, propValue);
				delete p.second;
			}
			jmap.clear();
		}
	}



	// SHM   ----  ----  ----  ----  ----  ----  ----
	/**
	 * shm_get
	 * 
	 * Create or get shared memory
	 * Params:
	 *  key_t key
	 *  size_t count - count of elements, not bytes
	 *  int shmflg - flags for shmget()
	 *  int at_shmflg - flags for shmat()
	 *  enum ShmBufferType type
	 * Returns buffer or typed array, depends on input param type
	 */
	NAN_METHOD(shm_get);

	/**
	 * Destroy shared memory segment
	 * Params:
	 *  key_t key
	 *  bool force - true to destroy even there are other processed uses this segment
	 * Returns count of left attaches or -1 on error
	 */
	NAN_METHOD(detach);

	/**
	 * Detach all created and getted shared memory segments
	 * Returns count of destroyed segments
	 */
	NAN_METHOD(detachAll);

	/**
	 * Get total size of all shared segments in bytes
	 */
	NAN_METHOD(getTotalSize);

	/**
	 * Constants to be exported:
	 * IPC_PRIVATE
	 * IPC_CREAT
	 * IPC_EXCL
	 * SHM_RDONLY
	 * NODE_BUFFER_MAX_LENGTH (count of elements, not bytes)
	 * enum ShmBufferType: 
	 *  SHMBT_BUFFER, SHMBT_INT8, SHMBT_UINT8, SHMBT_UINT8CLAMPED, 
	 *  SHMBT_INT16, SHMBT_UINT16, SHMBT_INT32, SHMBT_UINT32, 
	 *  SHMBT_FLOAT32, SHMBT_FLOAT64
	 */

	// LRU -   ----  ----  ----  ----  ----  ----  ----
	//	hash default or Hop Scotch

	/**
	 * get LRU segment size
	 */
	NAN_METHOD(getSegSize);
	/**
	 * get Max Element count of a segment (pass this to initHopScotch)
	 */
	NAN_METHOD(getMaxCount);
	/**
	 * get Current Element of an LRU (for apps that need to know, e.g. syslog)
	 */
	NAN_METHOD(getCurrentCount);
	/**
	 * get Free Count of an LRU -- e.g. if checking for nearing limits, if rationing resources e.g rate limiting
	 */
	NAN_METHOD(getFreeCount);

	/**
	 * time_since_epoch -- unix epoch offset in milliseconds
	 */
	NAN_METHOD(time_since_epoch);

	/**
	 * add hash key and value
	 */
	NAN_METHOD(set_el);

	/**
	 * add a list of hash key and value
	 */
	NAN_METHOD(set_many);

	/**
	 * get element at index
	 */
	NAN_METHOD(get_el);

	/**
	 * get element at index
	 */
	NAN_METHOD(get_el_hash);

	/**
	 * delete element at index
	 */
	NAN_METHOD(del_el);

	/**
	 * delete element having a key...
	 */
	NAN_METHOD(del_key);

	/**
	 * remove a key from the local hash table, don't examine the record deleted elsewhere
	 */
	NAN_METHOD(remove_key);
	
	/**
	 * get_last_reason and reset to OK...
	 */
	
	NAN_METHOD(get_last_reason);

	/**
	 *  reload_hash_map  -- clear and rebuild...
	 */
	NAN_METHOD(reload_hash_map);
	NAN_METHOD(reload_hash_map_update);
	NAN_METHOD(set_share_key);

	/**
	 *  run_lru_eviction  -- clear and rebuild...
	 */
	NAN_METHOD(run_lru_eviction);
	NAN_METHOD(run_lru_eviction_get_values);
	NAN_METHOD(run_lru_targeted_eviction_get_values);
	NAN_METHOD(run_lru_eviction_move_values);

	NAN_METHOD(debug_dump_list);



	// CREATE  ----  ----  ----  ----  ----  ----
	/**
	 * Assign a memory section for a CREATE
	 */
	NAN_METHOD(create);


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	const uint32_t keyMin = 1;
	const uint32_t keyMax = UINT32_MAX - keyMin;
	const uint32_t lengthMin = 1;
	const uint32_t lengthMax = UINT16_MAX;   // for now


	using node::AtExit;
	using v8::Local;
	using v8::Number;
	using v8::Object;
	using v8::Value;



	class SharedSegmentsManager {

		public:

			SharedSegmentsManager() {
				_container_node_size = sizeof(uint32_t)*8;
			}
			virtual ~SharedSegmentsManager() {}

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
						err = shmctl(res_id, IPC_STAT, &shminf);
						if ( err == 0 ) {
							size = shminf.shm_segsz;   // get the seg size from the system
							_ids_to_seg_sizes[key] = size;
						} else {
							return return -2;							
						}
					}
					//
					void* res = shmat(resId, NULL, at_shmflg);
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
				int resId = shmget(key, 0, 0);
				if (resId == -1) {
					switch(errno) {
						case ENOENT: // not exists
						case EIDRM:  // scheduled for deletion
							info.GetReturnValue().Set(Nan::New<Number>(-1));
							return;
						default:
							return Nan::ThrowError(strerror(errno));
					}
				}
				struct shmid_ds shminf;
				size_t seg_size;
				//
				err = shmctl(res_id, IPC_STAT, &shminf);
				if ( err == 0 ) {
					seg_size = shminf.shm_segsz;   // get the seg size from the system
					_ids_to_seg_sizes[key] = seg_size;
				} else {
					return return -2;							
				}
				return seg_size;
			}


			/**
			 * _detach_op
			*/
			int _detach_op(key_t key, bool force, bool onExit) {
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
					delete _ids_to_seg_addrs[key];
					delete _ids_to_seg_sizes[key];
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

			size_t detach(key_t key,bool forceDestroy) {
				//
				int status = this->_detach_op(key,forceDestroy);
				if ( status == 0 ) {
					this->remove_if_lru(key);
					this->remove_if_hh_map(key);
					this->remove_if_com_buffer(key);
					return _ids_to_seg_addrs.size();
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
				for ( auto p : _ids_to_seg_sizes ) {
					key_t key = p.first;
					if ( this->detach(key,forceDestroy) == 0 ) {
						deleted++;
						total_freed += p.second;
					}
				}
				return pair<uint16_t,size_t>(deleted,total_freed);
			}


			void remove_if_lru(key_t key) {}
			void remove_if_hh_map(key_t key) {}
			void remove_if_com_buffer(key_t key) {}

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
				if ( find(_ids_to_seg_addrs.begin(),_ids_to_seg_addrs.end(),key) != _ids_to_seg_addrs.end() ) return true;
				int resId  this->key_to_id(key);
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
				int status = this->shm_getter(key, at_shmfl, shmflg, true, seg_size);
				return status;
			}
			
			int _shm_attacher(key_t key,int at_shmflg) {
				int status = this->shm_getter(key, at_shmfl);
				return status;
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
					int shmflg = 0;
					//
					status = this->_shm_attacher(key, at_shmfl);
				}
				//
				if ( status == 0 ) _com_buffer = _ids_to_seg_addrs[com_key];
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
			// _reserve_count_free = (_max_count/100)*_reserve_percent;

			int initialize_lru_shm(key_t key, bool am_initializer, uint32_t els_per_tier, uint32_t max_obj_size, uint32_t num_procs, uint32_t num_tiers, uint32_t els_per_tier) {
				int status = 0;
				//
				size_t boundaries_atomics_sz = 2*sizeof(atomic<uint32_t>);
				size_t com_read_per_proc_sz = sizeof(Com_element)*num_procs;
				size_t max_count_lru_regions_sz = (sizeof(LRU_element) + max_obj_size)*(els_per_tier  + 4);
				//
				size_t seg_size = boundaries_atomics_sz + com_read_per_proc_sz + max_count_lru_regions_sz;
				//
				if ( am_initializer ) {
					status = _shm_creator(com_key,seg_size);
				} else {
					int at_shmflg = 0;
					int shmflg = 0;
					//
					status = this->_shm_attacher(key, at_shmfl);
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
					status = _shm_creator(com_key,seg_size);
				} else {
					int at_shmflg = 0;
					int shmflg = 0;
					//
					status = this->_shm_attacher(key, at_shmfl);
				}
				if ( status == 0 ) {
					_seg_to_hh_tables[key] = _ids_to_seg_addrs[key];
				}
				return status;
			}


			int tier_segments_initializers(bool am_initializer,list<uint32_t> &lru_keys,list<uint32_t> &hh_keys,uint32_t max_obj_size,uint32_t num_procs,uint32_t num_tiers,uint32_t els_per_tier) {
				//
				for ( auto lru_key : lru_keys ) {
					if ( lru_key < keyMin || lru_key >= keyMax ) {
						return -(i+1);
					}
					status = this->initialize_lru_shm(lru_key,am_initializer,max_obj_size,num_procs,num_tiers,els_per_tier);
					if ( status != 0 ) { return status; }
				}
				for ( auto hh_key : hh_keys ) {
					if ( hh_key < keyMin || hh_key >= keyMax ) {
						return -(i+1);
					}
					status = this->initialize_hmm_shm(hh_key,am_initializer,els_per_tier);
					if ( status != 0 ) { return status; }
				}
				//
				return 0;
			}

	public:

			map<key_t,void *>					_ids_to_seg_addrs;
			map<key_t,size_t> 					_ids_to_seg_sizes;
			//
			void 								*_com_buffer;
			size_t								_com_buffer_size;
			map<key_t,void *>					_seg_to_lrus;
			map<key_t,void *>					_seg_to_hh_tables;
			//
			size_t								_container_node_size;

	};

}
}
