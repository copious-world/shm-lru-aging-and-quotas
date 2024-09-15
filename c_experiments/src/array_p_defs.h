#ifndef _H_ARRAY_P_DEFS_
#define _H_ARRAY_P_DEFS_

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


// USING
using namespace std;
// 

// REPLACE SharedQueue_SRSW with a shared queue. Test SharedQueue_SRSW once again.


#include "time_bucket.h"
#include "random_selector.h"
#include "shm_seg_manager.h"

#include "node_shm_tiers_and_procs.h"

#include "atomic_queue.h"




typedef struct PROC_COM {
  uint32_t    _hash;
  uint32_t    _value;			// 64
  uint8_t     _proc_id;
  atomic_flag _reader;
  atomic_flag _writer;			// 24
  //uint8_t	_filler[5];			// + 40
} proc_com_cell;


typedef struct REQUEST {
	uint32_t	_hash;
	uint8_t	_proc_id;
	uint32_t	_next;
	uint32_t	_prev;
	//uint8_t	_filler[3];

	void		init(uint32_t hash_init = UINT32_MAX) {
		_hash = hash_init;
	}
} request_cell;


typedef struct PUT {
	uint8_t	_proc_id;
	uint32_t	_hash;
	uint32_t	_value;
	uint32_t	_next;
	uint32_t	_prev;
	//uint8_t	_filler[3];

	void		init(uint32_t hash_init = UINT32_MAX) {
		_hash = hash_init;
	}

} put_cell;

/**
 * RequestEntries uses request_cell in SharedQueue_SRSW<request_cell,ExpectedMax>
 * 
 * 
*/

class RequestEntries : public  AtomicQueue<request_cell> {

	public:

		size_t setup_queue(uint8_t *start, size_t el_count, bool _am_initializer) {
			size_t step = sizeof(request_cell);
			size_t region_size = (el_count + 4)*step;
			if ( _am_initializer ) {
				setup_queue_region(start,step,region_size);
			} else {
				attach_queue_region(start,region_size);
			}
			return region_size;
		}

		static uint32_t check_expected_queue_region_size(size_t el_count) {
			return sizeof(request_cell)*(el_count + 4);
		}

};


class PutEntries : public  AtomicQueue<put_cell> {

	public:

		size_t setup_queue(uint8_t *start, size_t el_count, bool _am_initializer) {
			size_t step = sizeof(put_cell);
			size_t region_size = (el_count + 4)*step;
			if ( _am_initializer ) {
				setup_queue_region(start,step,region_size);
			} else {
				attach_queue_region(start,region_size);
			}
			return region_size;
		}

		static uint32_t check_expected_queue_region_size(size_t el_count) {
			return sizeof(put_cell)*(el_count + 4);
		}

};


typedef struct PUT_QUEUE_MANAGER {
	atomic_flag								*_write_awake;
	atomic_flag								*_client_privilege;
	PutEntries								_put_queue;
} put_queue_manager;


typedef struct GET_QUEUE_MANAGER {
	atomic_flag								*_get_awake;
	atomic_flag								*_client_privilege;
	RequestEntries							_get_queue;
} get_queue_manager;



template<uint8_t max_req_procs = 8,uint8_t max_service_threads = 16>
struct TAB_PROC_DESCR {
	//
	proc_com_cell							*_outputs;
	put_queue_manager 						_put_com[max_service_threads];
	get_queue_manager 						_get_com[max_service_threads];
	//
	uint8_t									_num_client_p;
	uint8_t									_num_service_threads;
	uint8_t									_proc_id{255};

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	void set_region(void *data,size_t el_count,bool _am_initializer = false) {
		_outputs = (proc_com_cell *)data;
		setup_all_queues((uint8_t *)(_outputs + 1), el_count,_am_initializer);
	}

	void setup_all_queues(uint8_t *start, size_t el_count,bool _am_initializer = false) {
		start += sizeof(proc_com_cell)*max_req_procs;
		for ( uint8_t t = 0; t < max_service_threads; t++ ) {
			_put_com[t]._write_awake = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			_put_com[t]._client_privilege = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			//
			start += _put_com[t]._put_queue.setup_queue(start, el_count, _am_initializer);
			//
			_get_com[t]._get_awake = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			_get_com[t]._client_privilege = (atomic_flag *)start;
			//
			start += _get_com[t]._get_queue.setup_queue(start, el_count, _am_initializer);
		}
	}


	static uint32_t check_expected_region_size(size_t el_count) {
		uint32_t sz = 0;
		sz += PutEntries::check_expected_queue_region_size(el_count) + 2*sizeof(atomic_flag);
		sz += RequestEntries::check_expected_queue_region_size(el_count) + 2*sizeof(atomic_flag);
		sz *= max_service_threads;
		sz += sizeof(proc_com_cell)*max_req_procs;
		sz += sizeof(struct TAB_PROC_DESCR<max_req_procs,max_service_threads>);
		return sz;
	}


};


typedef struct TAB_PROC_DESCR<> table_proc_com;






/**
 *  ExternalInterfaceQs
 * 
 */
template<const uint32_t N>
class ExternalInterfaceQs {
  public:

    ExternalInterfaceQs(uint8_t client_count,uint8_t thread_count,void *data_region,size_t el_count,bool _am_initializer = false) {
      _proc_refs->set_region(data_region,el_count,_am_initializer);
      
      _sect_size = N/thread_count;
      //
      _thread_count = thread_count;
      _client_count = client_count;
      _proc_refs->_num_client_p = client_count;
      _proc_refs->_num_service_threads = thread_count;
      //
      for ( uint8_t t = 0; t < thread_count; t++ ) {
        _proc_refs->_put_com[t]._write_awake->clear();
        _proc_refs->_put_com[t]._client_privilege->clear();
        _proc_refs->_get_com[t]._get_awake->clear();
        _proc_refs->_get_com[t]._client_privilege->clear();
      }
      //
    }
    virtual ~ExternalInterfaceQs(void) {}


	uint8_t next_thread_id(void) {
		table_proc_com *tpc = _proc_refs;
		for ( uint8_t p = 0; p < _thread_count; p++ ) {
			auto pid = tpc->_proc_id;
			if ( pid == 255 ) {
				pid = p;
				return p;
			}
			return 255;
		}
	}

    // ---- check_expected_com_region_size
    //
	static uint32_t check_expected_com_region_size(uint8_t q_entry_count) {
		//
      	size_t el_count = q_entry_count;
		uint32_t c_regions_size = sizeof(table_proc_com) + table_proc_com::check_expected_region_size(el_count);
		//
		return c_regions_size;
	}

    // 
    void await_put(uint8_t tnum) {
      while ( !( _proc_refs->_put_com[tnum]._write_awake->test() ) ) tick();
    }

    // 
    void await_get(uint8_t tnum) {
      while ( !( _proc_refs->_get_com[tnum]._get_awake->test() ) ) tick();
    }

    // 
    void clear_put(uint8_t tnum) {
      _proc_refs->_put_com[tnum]._write_awake->clear();
    }

    // 
    void clear_get(uint8_t tnum) {
      _proc_refs->_get_com[tnum]._get_awake->clear();
    }


	/**
	 * com_put
	 */
    void com_put(uint32_t hh,uint32_t val,uint8_t return_to_pid) {
      put_cell entry;
      entry._hash = hh;
      entry._value = val;
      entry._proc_id = return_to_pid;
      uint8_t tnum = (hh%N)/_sect_size;
      //
	  while ( _proc_refs->_put_com[tnum]._put_queue.full() ) tick();
      _proc_refs->_put_com[tnum]._put_queue.push_queue(entry);
      //
      _proc_refs->_put_com[tnum]._client_privilege->clear();     // wake up the service thread
    }



	/**
	 * com_req
	 */
    void com_req(uint32_t hh,uint32_t &val,uint8_t return_to_pid) {
      request_cell entry;
      entry._hash = hh;
      entry._proc_id = return_to_pid;
      uint8_t tnum = (hh%N)/_sect_size;
      //
	  while ( _proc_refs->_get_com[tnum]._get_queue.full() ) tick();
      _proc_refs->_get_com[tnum]._get_queue.push_queue(entry);
      //
      _proc_refs->_outputs[return_to_pid]._reader.test_and_set();
      _proc_refs->_get_com[tnum]._client_privilege->clear();     // wake up the service thread

      while( _proc_refs->_outputs[return_to_pid]._reader.test() ) tick();
      val = _proc_refs->_outputs[return_to_pid]._value;
    }


	/**
	 * unload_put_req
	 */
    bool unload_put_req(put_cell &setter,uint8_t tnum) {
      if ( _proc_refs->_put_com[tnum]._put_queue.empty() ) return false;
      _proc_refs->_put_com[tnum]._put_queue.pop_queue(setter);
      return true;
    }

	/**
	 * unload_get_req
	 */
    bool unload_get_req(request_cell &getter,uint8_t tnum) {
      if ( _proc_refs->_get_com[tnum]._get_queue.empty() ) return false;
      _proc_refs->_get_com[tnum]._get_queue.pop_queue(getter);
      return true;
    }

	/**
	 * write_to_proc
	 */
    void write_to_proc(uint32_t hh,uint32_t val,uint8_t return_to_pid) {
      _proc_refs->_outputs[return_to_pid]._hash = hh;
      _proc_refs->_outputs[return_to_pid]._value = val;
      _proc_refs->_outputs[return_to_pid]._reader.clear();
    }

  public:

    table_proc_com        *_proc_refs;
    uint8_t               _thread_count{0};
    uint8_t               _client_count{0};
    uint32_t              _sect_size{0};

};




#endif // _H_ARRAY_P_DEFS_