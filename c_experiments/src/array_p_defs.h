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
#include "atomic_queue.h"

// https://dennisbabkin.com/blog/?t=interprocess-communication-using-mach-messages-for-macos#mach_comm_code


#include "circbuf.h"

static const uint8_t Q_CTRL_ATOMIC_FLAG_COUNT = 2;


typedef struct PROC_COM {
  uint32_t    	_hash;
  uint32_t    	_value;			// 64
  uint8_t     	_proc_id;
  atomic_flag 	_reader;
  atomic_flag 	_writer;		// 24
  //uint8_t	_filler[5];			// + 40
} proc_com_cell;


typedef struct REQUEST {
	uint32_t	_hash;
	uint8_t		_proc_id;
	uint32_t	_next;
	uint32_t	_prev;
	//uint8_t	_filler[3];

	void		init(uint32_t hash_init = UINT32_MAX) {
		_hash = hash_init;
	}
} request_cell;


typedef struct PUT {
	uint8_t		_proc_id;
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

template<class CELL_TYPE>
class AppAtomicQueue : public AtomicQueue<CELL_TYPE> {

	public:

		size_t setup_queue(uint8_t *start, size_t q_entry_count, bool am_initializer) {
			size_t step = sizeof(CELL_TYPE);
			size_t region_size = AppAtomicQueue<CELL_TYPE>::check_expected_queue_region_size(q_entry_count);
			if ( am_initializer ) {
				this->setup_queue_region(start,step,region_size);
			} else {
				this->attach_queue_region(start,region_size);
			}
			return region_size;
		}

		static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
			return AtomicQueue<CELL_TYPE>::check_region_size(q_entry_count);
		}

};


class RequestEntries : public AppAtomicQueue<request_cell> {

	public:

		static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
			return AppAtomicQueue<request_cell>::check_region_size(q_entry_count) + Q_CTRL_ATOMIC_FLAG_COUNT*sizeof(atomic_flag);
		}

		size_t setup_queue(uint8_t *start, size_t q_entry_count, bool am_initializer) {
			return AppAtomicQueue<request_cell>::setup_queue(start, q_entry_count, am_initializer);
		}

};


class PutEntries : public  AppAtomicQueue<put_cell> {

	public:

		static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
			return AppAtomicQueue<put_cell>::check_region_size(q_entry_count) + Q_CTRL_ATOMIC_FLAG_COUNT*sizeof(atomic_flag);
		}

		size_t setup_queue(uint8_t *start, size_t q_entry_count, bool am_initializer) {
			return AppAtomicQueue<put_cell>::setup_queue(start, q_entry_count, am_initializer);
		}
};


typedef struct PUT_QUEUE_MANAGER {
	atomic_flag								*_write_awake;
	atomic_flag								*_client_privilege;
	PutEntries								_put_queue;


	static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
		return PutEntries::check_expected_queue_region_size(q_entry_count);
	}
	
} put_queue_manager;


typedef struct GET_QUEUE_MANAGER {
	atomic_flag								*_get_awake;
	atomic_flag								*_client_privilege;
	RequestEntries							_get_queue;

	static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
		return RequestEntries::check_expected_queue_region_size(q_entry_count);
	}

} get_queue_manager;



template<uint8_t max_req_procs = 8,uint8_t max_service_threads = 16>
struct TAB_PROC_DESCR {
	//
	proc_com_cell							*_outputs;
	put_queue_manager 						_put_com[max_service_threads];
	get_queue_manager 						_get_com[max_service_threads];
	//
	uint8_t									_num_client_p{255};			// <= max_req_procs
	uint8_t									_num_service_threads{255};	// <= max_service_threads
	uint8_t									_proc_id{255};

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	void set_region(void *data,size_t q_entry_count,bool am_initializer = false) {
		_outputs = (proc_com_cell *)data;
		//
		uint8_t *start = (uint8_t *)data;
		auto m_procs = min(max_req_procs,_num_client_p);
		start += sizeof(proc_com_cell)*m_procs;
		//
		proc_com_cell *out_cell = _outputs;
		while ( out_cell < (proc_com_cell *)start ) {
			out_cell->_proc_id = 255;
			out_cell->_reader.clear();
			out_cell->_writer.clear();
			out_cell++;
		}
		//
		setup_all_queues(start, q_entry_count, am_initializer);
	}

	void setup_all_queues(uint8_t *start, size_t q_entry_count,bool am_initializer = false) {
		//
		auto num_t = min(max_service_threads,_num_service_threads);

		for ( uint8_t t = 0; t < num_t; t++ ) {
			_put_com[t]._write_awake = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			_put_com[t]._client_privilege = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			//
			start += _put_com[t]._put_queue.setup_queue(start, q_entry_count, am_initializer);
			//
			_get_com[t]._get_awake = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			_get_com[t]._client_privilege = (atomic_flag *)start;
			//
			start += _get_com[t]._get_queue.setup_queue(start, q_entry_count, am_initializer);
		}
		//
	}


	static uint32_t check_expected_region_size(size_t q_entry_count) {
		uint32_t sz = 0;
		sz += put_queue_manager::check_expected_queue_region_size(q_entry_count);
		sz += get_queue_manager::check_expected_queue_region_size(q_entry_count);
		sz *= max_service_threads;		// on queue per service thread
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
template<const uint32_t Q_SIZE>
class ExternalInterfaceQs {
  public:


	ExternalInterfaceQs() {}

    ExternalInterfaceQs(uint8_t client_count,uint8_t thread_count,void *data_region,size_t max_els_stored,bool am_initializer = false) {
		initialize(client_count, thread_count, data_region, max_els_stored, am_initializer);
    }

    virtual ~ExternalInterfaceQs(void) {}



public:


    void initialize(uint8_t client_count,uint8_t thread_count,void *data_region,size_t max_els_stored,bool am_initializer = false) {
		//
		_proc_refs.set_region(data_region,Q_SIZE,am_initializer);
		//
		_sect_size = max_els_stored/thread_count;

cout << "thread_count: " << thread_count << endl;
cout << "_sect_size: " << _sect_size << endl;
		_max_els_stored = max_els_stored;
		//
		_thread_count = thread_count;
		_client_count = client_count;
		_proc_refs._num_client_p = client_count;
		_proc_refs._num_service_threads = thread_count;
		//
		for ( uint8_t t = 0; t < thread_count; t++ ) {
			_proc_refs._put_com[t]._write_awake->clear();
			_proc_refs._put_com[t]._client_privilege->clear();
			_proc_refs._get_com[t]._get_awake->clear();
			_proc_refs._get_com[t]._client_privilege->clear();
		}
    	//
    }


	/**
	 * next_thread_id
	 * 	find a spot in the output section of the shared com buffer, and return its index.
	 * 	Overwrite the marker showing that it is not taken with the index.
	 */
	uint8_t next_thread_id(void) {
		proc_com_cell *tpc = _proc_refs._outputs;
		for ( uint8_t p = 0; p < _client_count; p++, tpc++ ) {
			auto pid = tpc->_proc_id;
			if ( pid == 255 ) {
				tpc->_proc_id = p;
				return p;
			}
		}
		return 255;
	}

    // ---- check_expected_com_region_size
    //
	static uint32_t check_expected_com_region_size(uint8_t q_entry_count) {
		//
		uint32_t c_regions_size = table_proc_com::check_expected_region_size((size_t)q_entry_count);
		//
		return c_regions_size;
	}

    // 
    void await_put(uint8_t tnum) {
      while ( !( _proc_refs._put_com[tnum]._write_awake->test() ) ) tick();
    }

    // 
    void await_get(uint8_t tnum) {
      while ( !( _proc_refs._get_com[tnum]._get_awake->test() ) ) tick();
    }

    // 
    void clear_put(uint8_t tnum) {
      _proc_refs._put_com[tnum]._write_awake->clear();
    }

    // 
    void clear_get(uint8_t tnum) {
      _proc_refs._get_com[tnum]._get_awake->clear();
    }


	/**
	 * com_put
	 * 
	 * Parameters: 
	 * 		- uint32_t hh - the bucket number...
	 * 		- uint32_t val - the value
	 * 		- uint8_t return_to_pid - the index of the output block for return values.
	 */
    uint32_t com_put(uint32_t hh,uint32_t val,uint8_t return_to_pid) {
		put_cell entry;
		entry._hash = hh;
		entry._value = val;
		entry._proc_id = return_to_pid;

		uint8_t tnum = (hh%_max_els_stored)/_sect_size;
		if ( _thread_count <= tnum ) {
			cout << "BEYOND THREAD COUNT!!! " << endl;
			return UINT32_MAX;
		}
		//
		uint32_t cnt = 0;
		while ( _proc_refs._put_com[tnum]._put_queue.full() ) { tick(); if ( cnt++ > 10000 ) { return UINT32_MAX; } }
		//
		auto result = _proc_refs._put_com[tnum]._put_queue.push_queue(entry);
		uint16_t pcnt = 0;
		while ( (result == UINT32_MAX) && (pcnt < 1024) ) {
			for ( int i = 0; i < 100; i++ ) tick();
			pcnt++;
			result = _proc_refs._put_com[tnum]._put_queue.push_queue(entry);
		}
		//
		return result;
    }



	/**
	 * com_req
	 */
    uint32_t com_req(uint32_t hh,uint32_t &val,uint8_t return_to_pid) {
		request_cell entry;
		entry._hash = hh;
		entry._proc_id = return_to_pid;
		uint8_t tnum = (hh%_max_els_stored)/_sect_size;
		if ( _thread_count <= tnum ) {
			cout << "BEYOND THREAD COUNT!!! " << endl;
			return UINT32_MAX;
		}
		//
		while ( !(_proc_refs._outputs[return_to_pid]._writer.test_and_set()) ) { tick(); }  // if doubling back on itself for any reason.
		while ( !(_proc_refs._outputs[return_to_pid]._reader.test_and_set()) ) { tick(); }   // for com with service.
		
		while ( _proc_refs._get_com[tnum]._get_queue.full() ){ tick(); cout << '.'; cout.flush(); }  // let the tnum thread clear some things out
	
		auto result = _proc_refs._get_com[tnum]._get_queue.push_queue(entry);
		uint16_t pcnt = 0;
		while ( (result == UINT32_MAX) && pcnt < 1024 ) {
			for ( int i = 0; i < 100; i++ ) tick();
			pcnt++;
			result = _proc_refs._get_com[tnum]._get_queue.push_queue(entry);
		}
		if ( UINT32_MAX == result ) return UINT32_MAX;
		
		uint16_t cnt = 0;
		while ( !(_proc_refs._outputs[return_to_pid]._reader.test_and_set()) && cnt < 1024 ) { cnt++, tick(); } // for com with service.
		if ( cnt >= 1024 ) {
			while ( _proc_refs._outputs[return_to_pid]._reader.test() ) {
				_proc_refs._outputs[return_to_pid]._reader.clear();
				cout << 'Y'; cout.flush(); 
			}
			val = UINT32_MAX;
		} else {
			val = _proc_refs._outputs[return_to_pid]._value;
		}
		_proc_refs._outputs[return_to_pid]._writer.clear();
		return result;
    }


	/**
	 * unload_put_req
	 */
    bool unload_put_req(put_cell &setter,uint8_t tnum) {
		if ( _proc_refs._put_com[tnum]._put_queue.empty() ) return false;
		auto result = _proc_refs._put_com[tnum]._put_queue.pop_queue(setter);
		if ( result == UINT32_MAX ) return false;
		return true;
    }


	/**
	 * unload_get_req
	 */
    bool unload_get_req(request_cell &getter,uint8_t tnum) {
		if ( _proc_refs._get_com[tnum]._get_queue.empty() ) return false;
cout << 'G'; cout.flush();
		auto result = _proc_refs._get_com[tnum]._get_queue.pop_queue(getter);
cout << "getter._hash: " << getter._hash << " " << getter._proc_id << endl;
		if ( result == UINT32_MAX ) return false;
		return true;
    }

	/**
	 * write_to_proc
	 */
    void write_to_proc(uint32_t hh,uint32_t val,uint8_t return_to_pid) {
		_proc_refs._outputs[return_to_pid]._hash = hh;
		_proc_refs._outputs[return_to_pid]._value = val;
		//
		_proc_refs._outputs[return_to_pid]._reader.clear();
		while ( _proc_refs._outputs[return_to_pid]._reader.test() ) {
			_proc_refs._outputs[return_to_pid]._reader.clear();
		}
		//
    }


  public:

  bool report{false};

    table_proc_com        _proc_refs;
    uint8_t               _thread_count{0};
    uint8_t               _client_count{0};
    uint32_t              _sect_size{0};
    uint32_t              _max_els_stored{0};

};








/**
 *  ExternalInterfaceWaitQs
 * 
 */
template<const uint32_t Q_SIZE>
class ExternalInterfaceWaitQs {

public:

	ExternalInterfaceWaitQs() {}

    ExternalInterfaceWaitQs(uint8_t client_count,uint8_t thread_count,void *data_region,size_t max_els_stored,bool am_initializer = false) {
		initialize(client_count, thread_count, data_region, max_els_stored, am_initializer);
    }

    virtual ~ExternalInterfaceWaitQs(void) {}



public:


    void initialize(uint8_t client_count,uint8_t thread_count,void *data_region,size_t max_els_stored,bool am_initializer = false) {
		//
		_proc_refs.set_region(data_region,Q_SIZE,am_initializer);
		//
		_sect_size = max_els_stored/thread_count;
		_max_els_stored = max_els_stored;
		//
		_thread_count = thread_count;
		_client_count = client_count;
		_proc_refs._num_client_p = client_count;
		_proc_refs._num_service_threads = thread_count;
		//
		for ( uint8_t t = 0; t < thread_count; t++ ) {
			_proc_refs._put_com[t]._write_awake->clear();
			_proc_refs._put_com[t]._client_privilege->clear();
			_proc_refs._get_com[t]._get_awake->clear();
			_proc_refs._get_com[t]._client_privilege->clear();
		}
    	//
		_data_ref = data_region; /// temporarily set....

    }



	/**
	 * next_thread_id
	 * 	find a spot in the output section of the shared com buffer, and return its index.
	 * 	Overwrite the marker showing that it is not taken with the index.
	 */
	uint8_t next_thread_id(void) {
		c_proc_com_cell *tpc = _proc_refs._outputs;
		for ( uint8_t p = 0; p < _client_count; p++, tpc++ ) {
			auto pid = tpc->_proc_id;
			if ( pid == 255 ) {
				tpc->_proc_id = p;
				return p;
			}
		}
		return 255;
	}

    // ---- check_expected_com_region_size
    //
	static uint32_t check_expected_com_region_size(uint8_t q_entry_count) {
		//
		uint32_t c_regions_size = table_proc_com::check_expected_region_size((size_t)q_entry_count);
		//
		return c_regions_size;
	}


    // 
    void await_put(uint8_t tnum) {
      while ( !( _proc_refs._put_com[tnum]._write_awake->test_and_set() ) ) tick();
    }

    // 
    void await_get(uint8_t tnum) {
      while ( !( _proc_refs._get_com[tnum]._get_awake->test_and_set() ) ) tick();
    }

    // 
    void clear_put(uint8_t tnum) {
      _proc_refs._put_com[tnum]._write_awake->clear();
    }

    // 
    void clear_get(uint8_t tnum) {
      _proc_refs._get_com[tnum]._get_awake->clear();
    }


	/**
	 * com_put
	 * 
	 * Parameters: 
	 * 		- uint32_t hh - the bucket number...
	 * 		- uint32_t val - the value
	 * 		- uint8_t return_to_pid - the index of the output block for return values.
	 */
    uint32_t com_put(uint32_t hh,uint32_t val,uint8_t return_to_pid) {
		put_cell entry;
		entry._hash = hh;
		entry._value = val;
		entry._proc_id = return_to_pid;

		uint8_t tnum = (hh%_max_els_stored)/_sect_size;
		if ( _thread_count <= tnum ) {
			cout << "BEYOND THREAD COUNT!!! " << endl;
			return UINT32_MAX;
		}
		//
		uint32_t result = 0;
		//
		await_put(tnum);

		if ( tnum != 0 ) {
			cout << "put writing to " << (int)tnum << " hh: " << hh << " val " << val << endl;
		}

		clear_put(tnum);
		//
		return result;
    }



	/**
	 * com_req
	 */
    uint32_t com_req(uint32_t hh,uint32_t &val,uint8_t return_to_pid) {
		request_cell entry;
		entry._hash = hh;
		entry._proc_id = return_to_pid;
		uint8_t tnum = (hh%_max_els_stored)/_sect_size;
		if ( _thread_count <= tnum ) {
			cout << "BEYOND THREAD COUNT!!! " << endl;
			return UINT32_MAX;
		}
		uint32_t result = 0;
		//
		await_get(tnum);

		if ( tnum != 0 ) {
			cout << "get requesting hh: " << hh << "from " << (int)tnum << endl;
		}

		clear_get(tnum);
		//
		return result;
    }


	/**
	 * unload_put_req
	 */
    bool unload_put_req(put_cell &setter,uint8_t tnum) {
		await_put(tnum);
cout << "put request handled by: " << (int)tnum << endl;
for ( int i = 0; i < 10; i++ ) tick();
		clear_put(tnum);
		return false;
    }


	/**
	 * unload_get_req
	 */
    bool unload_get_req(request_cell &getter,uint8_t tnum) {
		await_get(tnum);
cout << "get request handled by: " << (int)tnum << endl;
for ( int i = 0; i < 10; i++ ) tick();
		clear_get(tnum);
		return false;
    }

	/**
	 * write_to_proc
	 */
    void write_to_proc(uint32_t hh,uint32_t val,uint8_t return_to_pid) {

		// _proc_refs._outputs[return_to_pid]._hash = hh;
		// _proc_refs._outputs[return_to_pid]._value = val;
		// //
		// _proc_refs._outputs[return_to_pid]._reader.clear();
		// while ( _proc_refs._outputs[return_to_pid]._reader.test() ) {
		// 	_proc_refs._outputs[return_to_pid]._reader.clear();
		// }
		//
    }

public:

	void *_data_ref;

    c_table_proc_com      _proc_refs;
    uint8_t               _thread_count{0};
    uint8_t               _client_count{0};
    uint32_t              _sect_size{0};
    uint32_t              _max_els_stored{0};

};



#endif // _H_ARRAY_P_DEFS_