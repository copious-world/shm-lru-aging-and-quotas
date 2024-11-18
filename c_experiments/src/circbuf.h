#pragma once



#include <iostream>
#include <atomic>
#include <stdexcept>

using namespace std;


const uint8_t NUM_SHARED_ATOMS_C = 5;


typedef struct BASIC_C_ELEMENT_HDR {
	uint32_t	_info;
	uint8_t		_proc_id;

	void init([[maybe_unused]]int i = 0) {}

} Basic_c_element;


template<class QueueEl>
class CircBuff {		// ----
	//
	public:

		CircBuff() {
		}

		virtual ~CircBuff() {}


		/**
		 * pop_queue
		 * 
		 * In this method, the process pops from the tail of the queue. (Elements are pushed onto the front a.k.a. the header.)
		*/
		uint32_t pop_queue(QueueEl &output) {
			uint32_t rslt = UINT32_MAX;
			auto shared_atoms_section_size = NUM_SHARED_ATOMS_C*sizeof(atomic<uint32_t>);

			uint8_t *start = (uint8_t *)_q_head;
			start +=shared_atoms_section_size;
			//
			if ( !(this->empty()) ) {
				auto cur_tail = _q_tail->load();
				auto cur_head = _q_head->load();
				auto count = _q_shared_count->load();

				start += cur_tail*sizeof(QueueEl);
				QueueEl *el = (QueueEl *)start;
				output = *el;
				cur_tail++;
				if ( cur_tail >= count ) {
					cur_tail = 0;
				}
				_q_tail->store(cur_tail);
				rslt = 0;
			}
			//
			return UINT32_MAX;
		}


		/**
		 * push_queue
		 * 
		 * In this method, the process pushes onto the front of the queue. (Elements are popped from the tail.)
		*/
		uint32_t push_queue(QueueEl &input) {
			uint32_t rslt = UINT32_MAX;
			auto shared_atoms_section_size = NUM_SHARED_ATOMS_C*sizeof(atomic<uint32_t>);

			uint8_t *start = (uint8_t *)_q_head;
			start += shared_atoms_section_size;
			if ( !(this->full()) ) {
				//
				auto cur_tail = _q_tail->load();
				auto cur_head = _q_head->load();
				auto count = _q_shared_count->load();
				auto pos = cur_head;
				cur_head++;
				if ( cur_head >= count ) {
					cur_head = 0;
				}
				start += pos*sizeof(QueueEl);
				QueueEl *el = (QueueEl *)start;
				*el = input;
				_q_head->store(cur_head);
				//
				rslt = 0;
			}
			return rslt;
		}


		bool empty(void) {
			auto cur_tail = _q_tail->load();
			auto cur_head = _q_head->load();
			return (cur_tail == cur_head);
		}

		bool full(void) {
			auto cur_tail = _q_tail->load();
			auto cur_head = _q_head->load();
			auto count = _q_shared_count->load();
			if ( (cur_head + 1)%count == cur_tail ) return true;
			return false;
		}



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		static size_t check_region_size(uint32_t el_count) {
			size_t total_size = NUM_SHARED_ATOMS_C*sizeof(atomic<uint32_t>) + el_count*sizeof(QueueEl);
			return total_size;
		}

		static uint8_t atomics_count(void) {
			return NUM_SHARED_ATOMS_C;
		}


		uint16_t setup_queue_region(uint8_t *start, size_t step, size_t region_size) {
			_q_head = (atomic<uint32_t> *)start;
			_q_tail = _q_head + 1;
			_q_shared_count = _q_tail + 1;
			_q_pop_count = _q_shared_count + 1;
			_popper = (atomic_flag *)(_q_pop_count + 1);

			_q_head->store(0);
			_q_tail->store(0);
			_q_shared_count->store(0);
			_q_pop_count->store(0);
			_popper->clear();

			auto shared_atoms_section_size = NUM_SHARED_ATOMS_C*sizeof(atomic<uint32_t>);
			auto sz = region_size - shared_atoms_section_size;
//
			uint8_t *el = start + shared_atoms_section_size;
			uint8_t *end = el + sz;

			//
			uint16_t count = 0;
			while ( el < end ) {
				QueueEl *qel = (QueueEl *)el;
				qel->init();
				el += sizeof(QueueEl);
				count++;
			}

			_q_shared_count->store(count);
			return count;
		}


		uint16_t attach_queue_region(uint8_t *start, size_t region_size) {
			_q_head = (atomic<uint32_t> *)start;
			_q_tail = _q_head + 1;
			_q_shared_count = _q_tail + 1;
			_q_pop_count = _q_shared_count + 1;
			_popper = (atomic_flag *)(_q_pop_count + 1);
			//auto sz = region_size - NUM_SHARED_ATOMS_C*sizeof(atomic<uint32_t>);
			// attach
			uint16_t count = _q_shared_count->load();
			return count;
		}


	public:

		atomic<uint32_t> 				*_q_head;
		atomic<uint32_t> 				*_q_tail;
		atomic<uint32_t> 				*_q_shared_count;
		atomic<uint32_t> 				*_q_pop_count;
		atomic_flag						*_popper;
		//

};




static const uint8_t C_CTRL_ATOMIC_FLAG_COUNT = 2;


typedef struct CPROC_COM {
  uint32_t    	_hash;
  uint32_t    	_value;			// 64
  uint8_t     	_proc_id;
  atomic_flag 	_reader;
  atomic_flag 	_writer;		// 24
  //uint8_t	_filler[5];			// + 40
} c_proc_com_cell;


typedef struct CREQUEST {
	uint8_t		_proc_id;
	uint32_t	_hash;
	// uint32_t	_next;
	// uint32_t	_prev;
	//uint8_t	_filler[3];

	void		init(uint32_t hash_init = UINT32_MAX) {
		_hash = hash_init;
	}
} c_request_cell;


typedef struct CPUT {
	uint8_t		_proc_id;
	uint32_t	_hash;
	// uint32_t	_next;
	// uint32_t	_prev;
	uint32_t	_value;
	//uint8_t	_filler[3];

	void		init(uint32_t hash_init = UINT32_MAX) {
		_hash = hash_init;
	}
} c_put_cell;

/**
 * RequestEntries uses request_cell in SharedQueue_SRSW<request_cell,ExpectedMax>
 * 
 * 
*/

template<class CELL_TYPE>
class AppCircBuff : public CircBuff<CELL_TYPE> {

	public:

		size_t setup_queue(uint8_t *start, size_t q_entry_count, bool am_initializer) {
			size_t step = sizeof(CELL_TYPE);
			size_t region_size = AppCircBuff<CELL_TYPE>::check_expected_queue_region_size(q_entry_count);
			if ( am_initializer ) {
				this->setup_queue_region(start,step,region_size);
			} else {
				this->attach_queue_region(start,region_size);
			}
			return region_size;
		}

		static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
			return CircBuff<CELL_TYPE>::check_region_size(q_entry_count);
		}

};


class CRequestEntries : public AppCircBuff<c_request_cell> {

	public:

		static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
			return AppCircBuff<c_request_cell>::check_region_size(q_entry_count);
		}

		size_t setup_queue(uint8_t *start, size_t q_entry_count, bool am_initializer) {
			return AppCircBuff<c_request_cell>::setup_queue(start, q_entry_count, am_initializer);
		}

};


class CPutEntries : public  AppCircBuff<c_put_cell> {

	public:

		static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
			return AppCircBuff<c_put_cell>::check_region_size(q_entry_count);
		}

		size_t setup_queue(uint8_t *start, size_t q_entry_count, bool am_initializer) {
			return AppCircBuff<c_put_cell>::setup_queue(start, q_entry_count, am_initializer);
		}
};


typedef struct CPUT_QUEUE_MANAGER {
	atomic_flag								*_write_awake;
	atomic_flag								*_client_privilege;
	CPutEntries								_put_queue;

	size_t setup_queue(uint8_t *start, size_t q_entry_count, bool am_initializer) {
		_write_awake = (atomic_flag *)start;
		start += sizeof(atomic_flag);
		_client_privilege = (atomic_flag *)start;
		start += sizeof(atomic_flag);
		return _put_queue.setup_queue(start,q_entry_count,am_initializer);
	}
	
	static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
		return CPutEntries::check_expected_queue_region_size(q_entry_count);
	}
	
} c_put_queue_manager;


typedef struct CGET_QUEUE_MANAGER {
	atomic_flag								*_get_awake;
	atomic_flag								*_client_privilege;
	CRequestEntries							_get_queue;


	size_t setup_queue(uint8_t *start, size_t q_entry_count, bool am_initializer) {
		_get_awake = (atomic_flag *)start;
		start += sizeof(atomic_flag);
		_client_privilege = (atomic_flag *)start;
		start += sizeof(atomic_flag);
		return _get_queue.setup_queue(start,q_entry_count,am_initializer);
	}


	static uint32_t check_expected_queue_region_size(size_t q_entry_count) {
		return CRequestEntries::check_expected_queue_region_size(q_entry_count);
	}

} c_get_queue_manager;



template<uint8_t max_req_procs = 16,uint8_t max_service_threads = 16>
struct CTAB_PROC_DESCR {
	//
	c_proc_com_cell							*_outputs;
	c_put_queue_manager 					_put_com[max_service_threads];
	c_get_queue_manager 					_get_com[max_service_threads];
	//
	uint8_t									_num_client_p{255};			// <= max_req_procs
	uint8_t									_num_service_threads{255};	// <= max_service_threads
	uint8_t									_proc_id{255};

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	void set_region(void *data,size_t q_entry_count,uint32_t reg_sz,bool am_initializer = false) {
		_outputs = (c_proc_com_cell *)data;
		//
		uint8_t *start = (uint8_t *)data;
		auto m_procs = min(max_req_procs,_num_client_p);
		auto proc_area_size = sizeof(c_proc_com_cell)*m_procs;
		auto q_area_size = (reg_sz -  proc_area_size);

		start += proc_area_size;
		//
		c_proc_com_cell *out_cell = _outputs;
		while ( out_cell < (c_proc_com_cell *)start ) {
			out_cell->_proc_id = 255;
			out_cell->_reader.clear();
			out_cell->_writer.clear();
			out_cell++;
		}
		//
		setup_all_queues(start, q_entry_count, q_area_size, am_initializer);
	}

	void setup_all_queues(uint8_t *start, size_t q_entry_count, uint32_t reg_sz, bool am_initializer = false) {
		//
		auto num_t = min(max_service_threads,_num_service_threads);

		auto end_region = start + reg_sz;
		for ( uint8_t t = 0; t < num_t; t++ ) {
			start += _put_com[t].setup_queue(start, q_entry_count, am_initializer);
			start += _get_com[t].setup_queue(start, q_entry_count, am_initializer);
		}

		if ( end_region < start ) {
			cout << " from _outputs: " << (start - (uint8_t *)_outputs) << endl;
			cout << "setup_all_queues: start is ahead of end by " << (start - end_region) << " ... queues overrun region" << endl;
			throw std::runtime_error(std::string("Initialization overruns buffer"));
		}

		if ( start == end_region ) {
			cout << "queue region ready... " << endl;
		}
		//
	}


	static uint32_t check_expected_region_size(size_t q_entry_count) {
		uint32_t sz = 0;
		sz += c_put_queue_manager::check_expected_queue_region_size(q_entry_count);
		sz += c_get_queue_manager::check_expected_queue_region_size(q_entry_count);
		sz *= max_service_threads;		// on queue per service thread
		sz += sizeof(c_proc_com_cell)*max_req_procs;
		//sz += sizeof(struct CTAB_PROC_DESCR<max_req_procs,max_service_threads>);
		return sz;
	}



	static uint32_t check_expected_region_size(size_t q_entry_count,uint8_t max_rq_procs,uint8_t max_srvce_threads) {
		uint32_t sz = 0;
		sz += c_put_queue_manager::check_expected_queue_region_size(q_entry_count);
		sz += c_get_queue_manager::check_expected_queue_region_size(q_entry_count);
		sz *= max_srvce_threads;		// on queue per service thread
		sz += sizeof(c_proc_com_cell)*max_rq_procs;
		//sz += sizeof(struct CTAB_PROC_DESCR<max_req_procs,max_service_threads>);
		return sz;
	}

};


typedef struct CTAB_PROC_DESCR<> c_table_proc_com;

