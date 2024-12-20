#ifndef _H_ARRAY_P_STORE_DEFS_
#define _H_ARRAY_P_STORE_DEFS_

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
#include <array>
#include <deque>


// USING
using namespace std;
// 

// REPLACE SharedQueue_SRSW with a shared queue. Test SharedQueue_SRSW once again.


#include "tick.h"
#include "array_p_defs.h"
#include "node_shm_queued.h"


typedef struct STORES {

  uint32_t      _min_hash{0};
  atomic_flag   _reading;
  unordered_map<uint32_t,uint32_t>  _table;

  void await_reading(void) {
    while ( _reading.test_and_set() ) tick();
  }

  void release_reading(void) {
    _reading.clear();
  }

} stores;


template<const uint8_t THREAD_COUNT>
class StoreHVPairs {
  public:

    StoreHVPairs(void) {
    }
    virtual ~StoreHVPairs(void) {}


	void initialize(size_t max_els_stored) {
		//
		_sect_size = max_els_stored/THREAD_COUNT;
		for ( uint8_t t = 0; t < THREAD_COUNT; t++ ) {
			stores &thread_section = _sections[t];
			thread_section._reading.clear();
			thread_section._min_hash = t*_sect_size;
		}
		//
	}

    bool store_pair(uint32_t hash, uint32_t val, uint8_t thread_index) {
		stores &thread_section = _sections[thread_index];
		//
		thread_section.await_reading();   // manage just the relation between store and get
		//
		auto ref = hash - thread_section._min_hash;
		if ( val == UINT32_MAX ) {
			thread_section._table.erase(ref);
		} else {
			thread_section._table[ref] = val;
		}
		//
		thread_section.release_reading();
		return true;
    }



    uint32_t get_val(uint32_t hash, uint8_t thread_index) {
		stores &thread_section = _sections[thread_index];
		//
		thread_section.await_reading();   // manage just the relation between store and get
		//
		auto ref = hash - thread_section._min_hash;

		auto itr = thread_section._table.find(ref);
		uint32_t val = UINT32_MAX;
		if ( itr != thread_section._table.end() ) {
			val = itr->second;
		}

		//uint32_t val = thread_section._table[ref];
		//
		thread_section.release_reading();
		return val;
    }

    uint32_t                              		_sect_size{0};
    array<stores,(size_t)(THREAD_COUNT)>       	_sections;

};

/**
 *  Storage_ExternalInterfaceQs
 * 
 */
template<const uint8_t THREAD_COUNT,const uint32_t Q_SIZE>
class Storage_ExternalInterfaceQs : public ExternalInterfaceWaitQs<Q_SIZE> {
  public:

	Storage_ExternalInterfaceQs(atomic_flag *global_shutdown,uint8_t client_count,uint8_t thread_count,
									void *data_region,size_t max_els_stored,bool _am_initializer = false)
		: ExternalInterfaceWaitQs<Q_SIZE> (client_count,thread_count,data_region,max_els_stored,_am_initializer,global_shutdown) {
			_storage.initialize(max_els_stored);
	}

    virtual ~Storage_ExternalInterfaceQs(void) {}


  public:


	static uint32_t check_expected_com_region_size(uint8_t q_entry_count, uint8_t max_rq_procs = 255, uint8_t max_srvce_threads = 255) {
		return ExternalInterfaceWaitQs<Q_SIZE>::check_expected_com_region_size(q_entry_count,max_rq_procs,max_srvce_threads);
	}

	/**
	 * put_handler
	 * 
	 * parameter: t_num = the number of the calling thread...
	 */
	void put_handler(uint8_t t_num) {			/// t_num a thread number
		//
        c_put_cell setter;
        while ( this->unload_put_req(setter,t_num) && !(this->shutting_down()) ) {
          auto hh = setter._hash;
          auto val = setter._value;
          _storage.store_pair(hh,val,t_num);
        }
		//
	}


	/**
	 * get_handler
	 * 
	 * parameter:  t_num
	 */
	void get_handler(uint8_t t_num) {
		//
		c_request_cell getter;
		while ( this->unload_get_req(getter,t_num) && !(this->shutting_down()) ) {
			auto hh = getter._hash;
			auto return_to_pid = getter._proc_id;
			auto val = _storage.get_val(hh,t_num);
			this->write_to_proc(t_num,hh,val,return_to_pid);
		}
		//
	}


  public:


	StoreHVPairs<THREAD_COUNT> _storage;


};




#endif // _H_ARRAY_P_STORE_DEFS_