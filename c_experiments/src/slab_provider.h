#pragma once



#include <iostream>
#include <atomic>
#include <map>
#include <stdexcept>
#include <string>

using namespace std;

#include "tick.h"


// SPARSE SLAB OPTION
// a wait based operation option without interleaving (hence, excess memory)
// fairly usual hash table... 



// HH_element 128 bits or 16 bytes ;; for this implementation is given

const uint8_t hh_el_size = 16;  // in bytes

// SP_slab_types

typedef enum {
	SP_slab_t_4,
	SP_slab_t_8,
	SP_slab_t_16,
	SP_slab_t_32
} SP_slab_types;


static inline SP_slab_types next_slab_type(SP_slab_types st) {
	if ( SP_slab_t_4 == st ) return SP_slab_t_8;
	if ( SP_slab_t_8 == st ) return SP_slab_t_16;
	if ( SP_slab_t_16 == st ) return SP_slab_t_32;
	return SP_slab_t_4;
}

static inline SP_slab_types prev_slab_type(SP_slab_types st) {
	if ( SP_slab_t_8 == st ) return SP_slab_t_4;
	if ( SP_slab_t_16 == st ) return SP_slab_t_8;
	if ( SP_slab_t_32 == st ) return SP_slab_t_16;
	return SP_slab_t_4;
}

// The first level of table storage is bucket headers.

typedef struct SP_element {
	SP_slab_types		_slab_type;			// + 8
	uint8_t				_bucket_count;		// + 8		// counts all included space takers including deletes -- reduced by cropping
	uint16_t			_slab_index;		// + 16 = 32
	uint32_t			_slab_offset;		// + 16 = 64
	uint32_t			_stash_ops;			// same as cbits ... except that membership role is not in use
	uint32_t			_reader_ops;		// same as tbits ... except that memory allocation is not kept
} sp_element;  // 128 bits


typedef enum {
	SP_CELL_ADD,
	SP_CELL_REMOVE,
	SP_CELL_REQUEST
} sp_cell_op;


typedef struct SP_communication_cell {
	bool					_active{false};
	SP_slab_types 			_st;
	int						_res_id{-1};		// if < 0 by a lot (< -1 or < -10), then _offset is the start (non-mutable ref) of a region within the segment
	uint16_t				_slab_index;
	size_t					_el_count;
	uint32_t 				_offset{0};
} sp_communication_cell;


typedef struct SP_comm_events {
	atomic_flag				_table_change;
	atomic<uint16_t>		_readers;
	sp_cell_op				_op;
	uint8_t					_which_cell;
} sp_comm_events;


// note: control threads will get an index to put into _which_cell;

// SlabProvider
//

class SlabProvider {
	//
	public:
		//
		SlabProvider() {
		}
		virtual ~SlabProvider(void) {}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		uint16_t bytes_needed(SP_slab_types st) {
			uint8_t sz = 4;
			switch ( st ) {
				case SP_slab_t_4: return (16*sz*4);
				case SP_slab_t_8: return (16*sz*8);
				case SP_slab_t_16: return (16*sz*16);
				case SP_slab_t_32: return (16*sz*32);
				default:
					break;
			}
			return 0;
		}

		uint8_t max_els(SP_slab_types st) {
			switch ( st ) {
				case SP_slab_t_4: return (4);
				case SP_slab_t_8: return (8);
				case SP_slab_t_16: return (16);
				case SP_slab_t_32: return (32);
				default:
					break;
			}
			return 0;
		}


		//
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void await_table_change(sp_comm_events *slab_ev) {
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		void set_allocator_role(bool may_allocate) {
			_allocator = may_allocate;
		}

		void set_slab_communicator(void *com_area,uint16_t threads_procs_count,uint8_t assigned_cell) {
			_slab_events = (sp_comm_events *)com_area;
			_slab_com = (sp_communication_cell *)(_slab_events);
			_end_slab_com = _slab_com + threads_procs_count;
			_slab_cell = assigned_cell;
			_particpating_thread_count = threads_procs_count;
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		
		void add_slab_entry(uint16_t slab_index,void *slab,SP_slab_types st,uint16_t el_count) {
			uint8_t *start = (uint8_t *)slab;
			uint8_t *end = start + el_count*hh_el_size;
			_slab_lookup[st][slab_index] = start;
			_slab_ender_lookup[st][slab_index] = end;
		}

		void remove_slab_entry(uint16_t slab_index,SP_slab_types st) {
			delete _slab_lookup[st][slab_index];
			delete _slab_ender_lookup[st][slab_index];
		}
		

		uint16_t gen_slab_index() {
			return 0;
		}

		/**
		 * slab_shm_attacher
		 */

		// TODO

		int create_resource(size_t bytes) {
			return -1;
		}

		uint8_t *bytes_available(SP_slab_types st,uint16_t el_count,uint32_t &offset) {
			offset = 0;
			return nullptr;
		}

		void *slab_shm_attacher(int res_id) {
			if ( res_id < 0 ) return nullptr;
			return nullptr;
		}

		/**
		 * slab_shm_detacher
		 */
		void slab_shm_detacher(int res_id) {
			if ( res_id > 0 ) {
				// TODO
			}
		}

		void *create_slab(SP_slab_types st, uint16_t el_count, int &res_id, uint32_t &offset) {
			uint8_t *bytes = bytes_available(st,el_count,offset);
			//
			if ( bytes == nullptr ) {
				res_id = create_resource(bytes_needed(st)*el_count);
				if ( res_id > 0 ) {
					offset = 0;
					return slab_shm_attacher(res_id);
				}
			} else {
				res_id = -res_id;
			}
			//
			return (void *)bytes;
		}


		/**
		 * attach_slab
		 */
		void *attach_slab(int res_id,uint32_t offset) {
			//
			void *slab = nullptr;
			//
			if ( res_id < 0 ) {
				slab = slab_shm_attacher(-res_id);
			} else {
				slab = slab_shm_attacher(-res_id);
			}
			//
			if ( (offset > 0) && (res_id < 0) ) {
				uint8_t *tmp = ((uint8_t *)slab) + offset;
				return (void *)tmp;
			}
			return slab;
		}


		/**
		 * slab_adder
		 */

		void *slab_adder(uint16_t slab_index,int res_id,uint32_t offset,SP_slab_types st,uint16_t el_count) {
			void *slab = attach_slab(res_id,offset);
			if ( slab == nullptr ) {
				string  msg = "bad call with unknown slab in slab handler";
				throw std::runtime_error(msg);
			}

			add_slab_entry(slab_index, slab, st, el_count);
		}


		/**
		 * slab_remover
		 */

		void slab_remover(uint16_t slab_index,int res_id,SP_slab_types st) {
			if ( res_id > 0 ) {
				slab_shm_detacher(res_id);
			}
			remove_slab_entry(slab_index, st);
		}


		/**
		 * add_slab_and_broadcast
		 */
		void add_slab_and_broadcast(uint16_t slab_index,int res_id,uint32_t offset,SP_slab_types st,uint16_t el_count) {
			//
			if ( _slab_events == nullptr ) {
				string  msg = "Uninitialized com buffer in slab handler";
				throw std::runtime_error(msg);
			}
			//
			void *slab = slab_adder(slab_index,res_id,offset,st,el_count);

			await_table_change(_slab_events);

			sp_communication_cell *own_cell = _slab_com + _slab_cell;
			if ( own_cell >= _end_slab_com ) {
				string  msg = "out of bounds slab com in slab handler";
				throw std::runtime_error(msg);
			}
			//
			own_cell->_st = st;
			own_cell->_el_count = el_count;
			own_cell->_slab_index = slab_index;
			own_cell->_res_id = res_id;
			own_cell->_active = true;

			_slab_events->_op = SP_CELL_ADD;
			_slab_events->_readers = _particpating_thread_count - 1;
			_slab_events->_which_cell = _slab_cell;

			while ( _slab_events->_readers.load(std::memory_order_acquire) > 0 ) {
				tick();
			}
			//
			own_cell->_active = false;
		}


		/**
		 * unload_msg_and_add_slab
		 */
		void unload_msg_and_add_slab(sp_communication_cell *cell) {
			uint16_t slab_index = cell->_slab_index;
			int res_id = cell->_res_id;
			uint32_t offset = cell->_offset;
			SP_slab_types st = cell->_st;
			uint16_t el_count = cell->_el_count;
			slab_adder(slab_index,res_id,offset,st,el_count);
		}


		/**
		 * unload_msg_and_remove_slab
		 * 
		 * note: only the caller detaches
		 */
		void unload_msg_and_remove_slab(sp_communication_cell *cell) {
			uint16_t slab_index = cell->_slab_index;
			int res_id = 0;
			uint32_t offset = 0;
			SP_slab_types st = cell->_st;
			uint16_t el_count = cell->_el_count;
			slab_remover(slab_index, res_id, st);
		}


		uint16_t create_slab_and_broadcast(SP_slab_types st,uint16_t el_count) {
			int res_id = 0;
			uint32_t offset = 0;
			void *new_slab = create_slab(st,el_count,res_id,offset);
			auto slab_index = gen_slab_index();
			add_slab_entry(slab_index, new_slab, st, el_count);
			_slab_events->_readers.store(0);
			_slab_events->_table_change.clear();
			add_slab_and_broadcast(slab_index, res_id, offset, st, el_count);
			return slab_index;
		}

		void unload_msg_and_create_slab(sp_communication_cell *cell) {
			if ( _allocator ) {
				SP_slab_types st = cell->_st;
				uint16_t el_count = cell->_el_count;
				create_slab_and_broadcast(st,el_count);
			}
		}


		/**
		 * add_slab_and_broadcast
		 */
		void request_slab_and_broadcast(SP_slab_types st,uint16_t el_count) {
			//
			if ( _slab_events == nullptr ) {
				string  msg = "Uninitialized com buffer in slab handler";
				throw std::runtime_error(msg);
			}
			//
			await_table_change(_slab_events);

			sp_communication_cell *own_cell = _slab_com + _slab_cell;
			if ( own_cell >= _end_slab_com ) {
				string  msg = "out of bounds slab com in slab handler";
				throw std::runtime_error(msg);
			}
			//
			own_cell->_st = st;
			own_cell->_el_count = el_count;
			own_cell->_slab_index = 0;
			own_cell->_res_id = 0;
			own_cell->_active = true;

			_slab_events->_op = SP_CELL_REQUEST;
			_slab_events->_readers = _particpating_thread_count - 1;
			_slab_events->_which_cell = _slab_cell;

			while ( _slab_events->_readers.load(std::memory_order_acquire) > 0 ) {
				tick();
			}
			//
			own_cell->_active = false;
		}


		void handle_receive_slab_event() {
			//
			auto op_cell = _slab_events->_which_cell;
			//
			sp_communication_cell *cell = _slab_com;
			if ( cell >= _end_slab_com ) {
				string  msg = "slab hanlder received message with out of bounds com cell";
				throw std::runtime_error(msg);
			}
			if ( !(cell->_active) ) return;
			//
			switch ( _slab_events->_op ) {
				case SP_CELL_ADD: {
					unload_msg_and_add_slab(cell);
					break;
				}
				case SP_CELL_REMOVE: {
					unload_msg_and_remove_slab(cell);
					break;
				}
				case SP_CELL_REQUEST: {
					if ( _allocator ) {
						unload_msg_and_create_slab(cell);
					}
					break;
				}
				default: {
					break;
				}
			}

			if ( _slab_events->_readers.load(std::memory_order_acquire) > 0 ) {
				auto remaining =_slab_events->_readers.fetch_sub(1,std::memory_order_acq_rel);
				if ( remaining == 0 ) {
					_slab_events->_table_change.clear();
				}
			}

		}

		//
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void _to_free(uint8_t *slab) {
		}

		uint8_t *_from_free(uint8_t *data) {

		}

		bool check_free(uint8_t *data) {
			if ( data == nullptr ) return false;
			//
			return false;
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void load_bytes(SP_slab_types st,uint16_t slab_index,uint32_t slab_offset, uint8_t *buffer, uint16_t sz) {
			uint8_t *slab = _slab_lookup[st][slab_index];
			if ( slab == nullptr ) return;
			uint8_t *el = slab + slab_offset;
			if ( el >= _slab_ender_lookup[st][slab_index] ) return;
			memcpy(buffer,el,sz);
		}


		void unload_bytes(SP_slab_types st,uint16_t slab_index,uint32_t slab_offset, uint8_t *buffer, uint16_t sz) {
			uint8_t *slab = _slab_lookup[st][slab_index];
			if ( slab == nullptr ) return;
			uint8_t *el = slab + slab_offset;
			if ( el >= _slab_ender_lookup[st][slab_index] ) return;
			memcpy(el,buffer,sz);
		}


		void expand(SP_slab_types st,uint16_t &slab_index,uint32_t &slab_offset,uint16_t el_count) {
			//
			uint16_t bytes_needed = this->bytes_needed(st);
			uint8_t buffer[bytes_needed];
			//
			load_bytes(st, slab_index, slab_offset, buffer, bytes_needed);
			uint8_t *slab = _slab_lookup[st][slab_index];
			uint8_t *el = slab + slab_offset;
			_to_free(el);
			st = next_slab_type(st);
			for ( auto p : _slab_lookup[st] ) {
				if ( check_free(p.second) ) {
					uint8_t *section = _from_free(p.second);
					if ( section != nullptr ) {
						slab_offset = (section - p.second);
						slab_index = p.first;
						return;
					}
				}
			}
			//
			if ( _allocator ) {
				slab_index = create_slab_and_broadcast(st,el_count);
				uint8_t *slab = _slab_lookup[st][slab_index];
				uint8_t *section = _from_free(slab);
				if ( section != nullptr ) {
					slab_offset = (section - slab);
					return;
				}
			} else {
				request_slab_and_broadcast(st,el_count);
				await_table_change(_slab_events);
				expand(st,slab_index,slab_offset,el_count);
			}
		}

		sp_comm_events									*_slab_events;
		sp_communication_cell							*_slab_com;
		sp_communication_cell							*_end_slab_com;
		uint8_t											_slab_cell;
		uint16_t										_particpating_thread_count;
		bool											_allocator{false};

		map<SP_slab_types,map<uint16_t,uint8_t *>> 		_slab_lookup;    // must preload all process
		map<SP_slab_types,map<uint16_t,uint8_t *>> 		_slab_ender_lookup;    // must preload all process
};


