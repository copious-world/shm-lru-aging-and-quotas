#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <nan.h>
#include <errno.h>
#include <asm/errno.h>


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


class MutexHolder {
	public:
		//
		MutexHolder(void *mem_ptr,bool am_initializer) {
			_mutex_ptr = nullptr;
			_status = true;
			if ( am_initializer ) {
				this->init_mutex(mem_ptr);
			} else {
				_mutex_ptr = (pthread_mutex_t *)(mem_ptr);
			}
		}

		// ~
		virtual ~MutexHolder(void) {
			if ( _mutex_ptr ) {
				pthread_mutex_destroy(_mutex_ptr);
			}
		}

		/**
		 * Called by the constructor if the `am_initializer` parameter is **true**.
		 * Performs standard POSIX style mutex initizalization.
		*/
		void init_mutex(void *mutex_mem) {
			//
			_mutex_ptr = (pthread_mutex_t *)mutex_mem;
			//
			int result = pthread_mutexattr_init(&_mutex_attributes);
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthreas_mutexattr_init: ";
				_last_reason += strerror(result);
				return;
			}
			result = pthread_mutexattr_setpshared(&_mutex_attributes,PTHREAD_PROCESS_SHARED);
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_mutexattr_setpshared: ";
				_last_reason += strerror(result);
				return;
			}

			result = pthread_mutex_init(_mutex_ptr, &_mutex_attributes);
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_mutex_init: ";
				_last_reason += strerror(result);
				return;
			}
			//
		}

		/**
		 * Calls the posix mutex try lock. Hence, if the thread is locked this will return
		 * immediatly. If the mutex is locked, the status EBUSY will be returned to this method, and the method
		 * will return **false** with the object `_status` set to true.
		 */
		bool try_lock() {
			reset_status();
			if ( _mutex_ptr == nullptr ) {
				return(false);
			}
			 int result = pthread_mutex_trylock( _mutex_ptr );
			 if ( result == EBUSY ) {
				 _status = true;
				 return(false);
			 }
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_mutex_trylock: ";
				_last_reason += strerror(result);
				return (false);
			}
			return(true);
		}

		bool lock() {
			reset_status();
			if ( _mutex_ptr == nullptr ) {
				return(false);
			}
			int result = pthread_mutex_lock( _mutex_ptr );
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_mutex_lock: ";
				_last_reason += strerror(result);
				return (false);
			}
			return(true);
		}

		bool unlock() {
			reset_status();
			if ( _mutex_ptr == nullptr ) {
				return(false);
			}
			int result = pthread_mutex_unlock( _mutex_ptr );
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_mutex_lock: ";
				_last_reason += strerror(result);
				return (false);
			}
			return(true);
		}


		/// status ---- ---- ---- ---- ---- ---- ---- ----

		//
		bool ok(void) {
			return _status;
		}

		void reset_status(void) {
			_status = true;
		}

		string get_last_reason(void) {
			if ( _status ) return("OK");
			string report = _last_reason;
			_last_reason = "OK";
			_status = true;
			return report;
		}

		// 
		pthread_mutex_t		*_mutex_ptr;
		pthread_mutexattr_t	_mutex_attributes;
		bool				_status;
		string				_last_reason;
};




// There is one condition variable per tier

class ConditionHolder {
	public:
		// (+)
		ConditionHolder(void *mem_ptr,MutexHolder *mutex_lock,bool am_initializer) {
			_cond_ptr = nullptr;
			_status = true;
			if ( am_initializer ) {
				this->init_condition(mem_ptr);
			} else {
				_cond_ptr = (pthread_cond_t *)(mem_ptr);
			}
			_mutex_lock = mutex_lock;
		}

		// ~
		virtual ~ConditionHolder(void) {
			if ( _cond_ptr ) {
				pthread_cond_destroy(_cond_ptr);
			}
		}

		// ----
		//
		void init_condition(void *cond_mem) {
			//
			_cond_ptr = (pthread_cond_t *)cond_mem;
			//
			int result = pthread_condattr_init(&_cond_attributes);
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_condattr_init: ";
				_last_reason += strerror(result);
				return;
			}
			result = pthread_condattr_getpshared(&_cond_attributes,PTHREAD_PROCESS_SHARED);
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_condattr_getpshared: ";
				_last_reason += strerror(result);
				return;
			}
			result = pthread_cond_init(_cond_ptr, &_cond_attributes);
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_cond_init: ";
				_last_reason += strerror(result);
				return;
			}
			//
		}

		// 

		bool signal() {
			int result = pthread_cond_signal(_cond_ptr);
			if ( result != 0 ) {
				_status = false;
				_last_reason = "pthread_cond_signal: ";
				_last_reason += strerror(result);
				return (false);
			}
			return true;
		}

		// ---- 
		//
		bool wait_on(char *shared_state) {
			if ( _mutex_lock->lock() ) {
				if ( *shared_state == 0 ) {
					int result = pthread_cond_wait(_cond_ptr,_mutex_lock->_mutex_ptr);
				} else {
					_status = false;
					_last_reason = "pthread_cond_wait: ";
					_last_reason += strerror(result);
					return (false);
				}
				_mutex_lock->unlock();
			} else {
				return false;
			}
			return true;
		}


		/**
		*/
		bool wait_on_timed(char *shared_state,uint8_t delta_seconds) {
			return wait_on_timed(shared_state,delta_seconds,-1);
		}


		bool wait_on_timed(char *shared_state,uint8_t delta_seconds,int32_t max_reps) {
			if ( _mutex_lock->lock() ) {
				// timespec is a structure holding an interval broken down into seconds and nanoseconds.
				struct timespec max_wait = {0, 0};
				//
				int result = clock_gettime(CLOCK_REALTIME, &max_wait);
				if ( result !== 0 ) {
					_status = false;
					_last_reason = "wait_on_timed: Could not get time from clock.\n ";
					_last_reason += strerror(result);
					_mutex_lock->unlock();
					return (false);
				}
				//
				max_wait.tv_sec += delta_seconds;

				while ( (*shared_state) == 0 ) {
					result = pthread_cond_timedwait(_cond_ptr,_mutex_lock->_mutex_ptr, &max_wait);
					if ( (result != 0) && (result != ETIMEDOUT || (max_reps == 0) ) ) {
						_status = false;
						_last_reason = "wait_on_timed: condition variable failed but not a timeout.\n ";
						_last_reason += strerror(result);
						_mutex_lock->unlock();
						return (false);
					}
					if ( result == 0 ) {
						*shared_state = 1;
						break;
					}
					if ( max_reps > 0 ) {
						max_reps--;
					}
					//
					max_wait.tv_sec += delta_seconds;
				}
				_mutex_lock->unlock();
			} else {
				return false
			}
			return true
		}

		// 
		pthread_cond_t		*_cond_ptr;
		pthread_condattr_t	_cond_attributes;
		MutexHolder 		*_mutex_lock
		bool				_status;
		string				_last_reason;
};




/*
namespace imp {
	static const size_t kMaxLength = 0x3fffffff;
}

namespace node {
namespace Buffer {
	// 2^31 for 64bit, 2^30 for 32bit
	static const unsigned int kMaxLength = 
		sizeof(int32_t) == sizeof(intptr_t) ? 0x3fffffff : 0x7fffffff;
}
}
*/

#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;
#define SAFE_DELETE_ARR(a) if( (a) != NULL ) delete [] (a); (a) = NULL;


enum ShmBufferType {
	SHMBT_BUFFER = 0, //for using Buffer instead of TypedArray
	SHMBT_INT8,
	SHMBT_UINT8,
	SHMBT_UINT8CLAMPED,
	SHMBT_INT16,
	SHMBT_UINT16,
	SHMBT_INT32,
	SHMBT_UINT32,
	SHMBT_FLOAT32,
	SHMBT_FLOAT64
};

inline int getSize1ForShmBufferType(ShmBufferType type) {
	size_t size1 = 0;
	switch(type) {
		case SHMBT_BUFFER:
		case SHMBT_INT8:
		case SHMBT_UINT8:
		case SHMBT_UINT8CLAMPED:
			size1 = 1;
		break;
		case SHMBT_INT16:
		case SHMBT_UINT16:
			size1 = 2;
		break;
		case SHMBT_INT32:
		case SHMBT_UINT32:
		case SHMBT_FLOAT32:
			size1 = 4;
		break;
		default:
		case SHMBT_FLOAT64:
			size1 = 8;
		break;
	}
	return size1;
}


namespace node {
namespace Buffer {

	MaybeLocal<Object> NewTyped(
		Isolate* isolate, 
		char* data, 
		size_t length
	#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback
	#else
	    , node::smalloc::FreeCallback callback
	#endif
	    , void *hint
		, ShmBufferType type = SHMBT_FLOAT64
	);

}
}


namespace Nan {

	inline MaybeLocal<Object> NewTypedBuffer(
	      char *data
	    , size_t length
#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback
#else
	    , node::smalloc::FreeCallback callback
#endif
	    , void *hint
		, ShmBufferType type = SHMBT_FLOAT64
	);

}


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
	 * Setup LRU data structure on top of the shared memory
	 */
	NAN_METHOD(initLRU);

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


	// HOPSCOTCH  ----  ----  ----  ----  ----
	/**
	 * Setup LRU data structure on top of the shared memory
	 */
	NAN_METHOD(initHopScotch);


	// CREATE  ----  ----  ----  ----  ----  ----
	/**
	 * Assign a memory section for a CREATE
	 */
	NAN_METHOD(create);







	/**
	 * Get access to the semaphore identified by a key
	 */
	NAN_METHOD(init_mutex);

	/**
	 * 	Wrap try_wait ... try_lock  ... will return if the lock is busy...
	 */
	NAN_METHOD(try_lock);

	/**
	 * 	Wrap lock  ... queue up for the lock...
	 */
	NAN_METHOD(lock);

	/**
	 *  Release the lock
	 */
	NAN_METHOD(unlock);

}
}
