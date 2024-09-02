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

		size_t setup_queue(uint8_t *start, size_t el_count) {
			size_t step = sizeof(request_cell);
			size_t region_size = (el_count + 4)*step;
			setup_queue_region(start,step,region_size);
			return region_size;
		}

		static uint32_t check_expected_queue_region_size(size_t el_count) {
			return sizeof(request_cell)*(el_count + 4);
		}

};


class PutEntries : public  AtomicQueue<put_cell> {

	public:

		size_t setup_queue(uint8_t *start, size_t el_count) {
			size_t step = sizeof(put_cell);
			size_t region_size = (el_count + 4)*step;
			setup_queue_region(start,step,region_size);
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


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	void set_region(void *data,size_t el_count) {
		_outputs = (proc_com_cell *)data;
		setup_all_queues((uint8_t *)(_outputs + 1), el_count);
	}

	void setup_all_queues(uint8_t *start, size_t el_count) {
		start += sizeof(proc_com_cell)*max_req_procs;
		for ( uint8_t t = 0; t < max_service_threads; t++ ) {
			_put_com[t]._write_awake = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			_put_com[t]._client_privilege = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			//
			start += _put_com[t]._put_queue.setup_queue(start, el_count);
			//
			_get_com[t]._get_awake = (atomic_flag *)start;
			start += sizeof(atomic_flag);
			_get_com[t]._client_privilege = (atomic_flag *)start;
			//
			start += _get_com[t]._get_queue.setup_queue(start, el_count);
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




#endif // _H_ARRAY_P_DEFS_