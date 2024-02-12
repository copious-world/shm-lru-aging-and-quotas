

#pragma once

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


using namespace std;


#include <map>
#include <unordered_map>
#include <list>



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
		MutexHolder 		*_mutex_lock;
		bool				_status;
		string				_last_reason;
};


