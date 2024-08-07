#pragma once


// SPARSE SLAB OPTION
//
// a wait based operation option without interleaving (hence, excess memory)
// fairly usual hash table... 


#include <iostream>
#include <atomic>
#include <map>
#include <stdexcept>
#include <string>

using namespace std;

#include "tick.h"
#include "shm_shared_segs.h"
#include <sys/ipc.h>


#include "simple_stack.h"
#include "tok_gen.h"


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
	return SP_slab_t_32;
}

static inline SP_slab_types prev_slab_type(SP_slab_types st) {
	if ( SP_slab_t_8 == st ) return SP_slab_t_4;
	if ( SP_slab_t_16 == st ) return SP_slab_t_8;
	if ( SP_slab_t_32 == st ) return SP_slab_t_16;
	return SP_slab_t_4;
}

// The first level of table storage is bucket headers.


/**
 * SP_element
 * 
 * For the basic bucket hash table, this data structure is the contents of the bucket that refers to a segment and an offset
 * to a samll array of elements. By assiging a type to the array, the segment containing its elements and free storage 
 * can be established.
 * 
 * This data structure contains operation bits, `_stash_ops` and `_reader_ops`. 
 */
typedef struct SP_element {
	SP_slab_types		_slab_type;			// + 8
	uint8_t				_bucket_count;		// + 8		// counts all included space takers including deletes -- reduced by cropping
	key_t				_slab_index;		// + 32 = 48
	uint16_t			_slab_offset;		// + 16 = 64
	uint32_t			_stash_ops;			// same as cbits ... except that membership role is not in use
	uint32_t			_reader_ops;		// same as tbits ... except that memory allocation is not kept
} sp_element;  // 128 bits


typedef enum {
	SP_CELL_ADD,
	SP_CELL_REMOVE,
	SP_CELL_REQUEST
} sp_cell_op;


typedef struct SP_communication_cell {
	atomic_flag				_active;
	SP_slab_types 			_st;
	int						_res_id{-1};		// if < 0 by a lot (< -1 or < -10), then _offset is the start (non-mutable ref) of a region within the segment
	key_t					_slab_index;
	size_t					_el_count;
	uint32_t 				_offset{0};
} sp_communication_cell;


typedef struct SP_comm_events {
	atomic_flag				_table_change;
	atomic<uint16_t>		_readers;
	sp_cell_op				_op;
	uint8_t					_which_cell;
} sp_comm_events;


// handle_receive_slab_event exists within a thread

typedef struct SLAB_parameters {
	void				*slab;
	SP_slab_types		st;
	uint16_t			el_count;
	uint32_t			slab_index;
} slab_parameters;


/**
 * SlabProvider
 * 
 * 
 * public:
 * 
 * 		bytes_needed(btype);
 * 		max_els(base->_slab_type)
 * 		uint32_t slab_size_bytes(SP_slab_types st,uint16_t el_count)
 * 
 * 		load_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed)
 * 		unload_bytes(btype,base->_slab_index,base->_slab_offset, elements_buffer, bytes_needed)
 * 		expand(st,si,so,( _max_n/(1 << (st+1)) ));
 * 		contract(st,si,so,( _max_n/(1 << (st+1)) ))
 * 	
 * 		void set_slabs_for_startup(slab_parameters *initial_slabs, uint8_t slab_count)
 * 		void set_slab_communicator(void *com_area,uint16_t threads_procs_count,uint8_t assigned_cell)
 * 		void slab_thread_runner([[maybe_unused]] int i)
 * 
 */
class SlabProvider : public SharedSegments, public TokGenerator {
	//
	public:
		//
		SlabProvider() {
		}
		virtual ~SlabProvider(void) {}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		uint16_t bytes_needed(SP_slab_types st) {
			switch ( st ) {
				case SP_slab_t_4: return (hh_el_size*4);
				case SP_slab_t_8: return (hh_el_size*8);
				case SP_slab_t_16: return (hh_el_size*16);
				case SP_slab_t_32: return (hh_el_size*32);
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


		uint32_t slab_size_bytes(SP_slab_types st,uint16_t el_count) {
			uint32_t sz =((bytes_needed(st) + sizeof(stack_el_header))*el_count) + sizeof(stack_stack_header);
			return sz;
		}


		/**
		 * bytes_available
		 * 
		 * 		The idea of bytes available would be that a large storage region is set up once again as free stack.
		 * 		But, the elements are large regions which will contain free stacks for specific element sizes. 
		 *		The reason to do this, would be to reduce the number of shared memory regions taking up file descriptors, etc.
		 *		For now, this method is not implemented.
		 */

		uint8_t *bytes_available([[maybe_unused]] SP_slab_types st,[[maybe_unused]] uint16_t el_count,uint32_t &offset) {
			offset = 0;
			return nullptr;
		}


		//
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		// The cell even is for one process/(main thread) and it segment sharing message handler thread
		void clear_cell_event(void) {
			while ( _own_cell->_active.test(std::memory_order_acquire) ) {		// clear it if not already clear
				_own_cell->_active.clear(std::memory_order_release);
			}
		}

		void await_cell_event_set(void) {
			while ( !(_own_cell->_active.test(std::memory_order_acquire)) ) { tick(); }	// wait for it to set
			_own_cell->_active.clear(std::memory_order_release);						// no clear it and move on
		}

		void signal_cell_event_set(void) {
			_own_cell->_active.test_and_set(std::memory_order_acq_rel);
#ifndef __APPLE__
			_own_cell->_active.notify_one();
#endif
		}

		// ---
		void await_event_set(void) {
			while ( !(_slab_events->_table_change.test()) ) {
				tick();
			};
		}

		// ---
		void await_event_clear(void) {
			while ( _slab_events->_table_change.test() ) {   // make sure it clears
				tick();
			};
		}


		void set_event() {
			while ( _slab_events->_table_change.test_and_set(std::memory_order_acq_rel) ) { tick(); }
#ifndef __APPLE__
			_slab_events->_table_change.notify_all();
#endif
		}


		void  clear_event(void) {
			_slab_events->_table_change.clear();
#ifndef __APPLE__
			_slab_events->_table_change.notify_all();
#endif
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		// SET shared regions

		/**
		 * set_slabs_for_startup
		 * 
		 * The calling method passes a set of slabs that are already allocated or attached to.
		 * 
		 * parameters: 
		 * 		initial_slabs -- a list of slab parameters carried in slab_parameters structures
		 * 		slab_count	-- not moret than 255 slabs
		 */
		void set_slabs_for_startup(slab_parameters *initial_slabs, uint8_t slab_count) {
			slab_parameters *sp = initial_slabs;
			slab_parameters *end_slabs = sp + slab_count;
			while ( sp <  end_slabs) {
				//
				void *slab = sp->slab;
				SP_slab_types st = sp->st;
				uint16_t el_count = sp->el_count;
				key_t slab_index = sp->slab_index;
				//
				add_slab_entry(slab_index, slab, st, el_count);
				auto slab_end = _slab_ender_lookup[st][slab_index];
				_stack_ops.set_region((uint8_t *)slab,(uint8_t *)slab_end,bytes_needed(st),true);  // new slab, set up free stack
				sp++;
			}
		}


		void set_allocator_role(bool may_allocate) {
			_allocator = may_allocate;
		}

		void set_slab_communicator(void *com_area,uint16_t threads_procs_count,uint8_t assigned_cell) {
			_slab_events = (sp_comm_events *)com_area;
			_slab_com = (sp_communication_cell *)(_slab_events + 1);
			_end_slab_com = _slab_com + threads_procs_count;
			_slab_cell = assigned_cell;
			sp_communication_cell *own_cell = _slab_com + _slab_cell;
			if ( own_cell >= _end_slab_com ) {
				string  msg = "out of bounds slab com in slab handler";
				throw std::runtime_error(msg);
			}
			_own_cell = own_cell;
			own_cell->_active.clear();
			_particpating_thread_count = threads_procs_count;
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		
		void add_slab_entry(key_t slab_index,void *slab,SP_slab_types st,uint16_t el_count) {
			uint8_t *start = (uint8_t *)slab;
			uint8_t *end = start + slab_size_bytes(st,el_count);   // has to include the stack implementation as well.
			_slab_lookup[st][slab_index] = start;
			_slab_ender_lookup[st][slab_index] = end;
		}

		void remove_slab_entry(key_t slab_index,SP_slab_types st) {
			delete _slab_lookup[st][slab_index];
			delete _slab_ender_lookup[st][slab_index];
		}
		


		/**
		 * create_resource
		 */

		// TODO

		void *create_resource(key_t key,size_t bytes) {
			if ( _shm_creator(key,bytes) == 0 ) {
				return get_addr(key);
			}
			return nullptr;
		}





		pair<void *,void *> create_slab(SP_slab_types st, uint16_t el_count, uint16_t slab_id, uint32_t &offset) {

			size_t mem_size = slab_size_bytes(st,el_count);

			uint8_t *bytes = bytes_available(st,mem_size,offset);  // this won't be used right now
			uint8_t *end_bytes = nullptr;
			//
			//
			if ( bytes == nullptr ) {		// don't have a special shared resource to mimic allocation
				void *vbytes = create_resource(slab_id,mem_size);
				end_bytes = ((uint8_t *)vbytes) + mem_size;
				void *vend_bytes = end_bytes;
				//
				pair<void *,void *> p(vbytes,vend_bytes);
				return p;
			} else {
				end_bytes = bytes + mem_size;
			}
			//
			pair<void *,void *> p((void *)bytes,(void *)end_bytes);
			return p;
		}



		/**
		 * slab_adder
		 */

		void *slab_adder(key_t slab_index,SP_slab_types st,uint16_t el_count) {
			//
			if ( _shm_attacher(slab_index,0) == 0 ) {
				void *slab = get_addr(slab_index);
				add_slab_entry(slab_index, slab, st, el_count);
			}
			return nullptr;
			//
		}


		/**
		 * slab_remover
		 */

		void slab_remover(key_t slab_index,SP_slab_types st) {
			key_t key = slab_index;
			detach(key,this->_allocator);
			remove_slab_entry(slab_index, st); 
		}


		/**
		 * broadcast_slab
		 */
		void broadcast_slab(key_t slab_index,[[maybe_unused]] uint32_t offset,SP_slab_types st,uint16_t el_count) {
			//
			if ( _slab_events == nullptr ) {
				string  msg = "Uninitialized com buffer in slab handler";
				throw std::runtime_error(msg);
			}
			//

			await_event_clear();
			set_event();
			_slab_events->_readers.store(0);
			//
			_own_cell->_st = st;
			_own_cell->_el_count = el_count;
			_own_cell->_slab_index = slab_index;
			_own_cell->_res_id = 0;
			//
			_slab_events->_op = SP_CELL_ADD;
			_slab_events->_which_cell = _slab_cell;
			_slab_events->_readers.store(_particpating_thread_count - 1);
			//
			_slab_events->_readers.store(_particpating_thread_count - 1);
			while ( _slab_events->_readers.load(std::memory_order_acquire) > 0 ) {
				tick();
			}
			//
		}



		/**
		 * request_slab_and_broadcast
		 */
		void request_slab_and_broadcast(SP_slab_types st,uint16_t el_count) {
			//
			if ( _slab_events == nullptr ) {
				string  msg = "Uninitialized com buffer in slab handler";
				throw std::runtime_error(msg);
			}
			//
			await_event_clear();
			set_event();
			_slab_events->_readers.store(0);
			//
			_own_cell->_st = st;
			_own_cell->_el_count = el_count;
			_own_cell->_slab_index = 0;
			_own_cell->_res_id = 0;
			//
			_slab_events->_op = SP_CELL_REQUEST;
			_slab_events->_which_cell = _slab_cell;
			_slab_events->_readers.store(_particpating_thread_count - 1);
			//
			//
			while ( _slab_events->_readers.load(std::memory_order_acquire) > 0 ) {
				tick();
			}
			//
		}



		/**
		 * unload_msg_and_add_slab
		 */
		void unload_msg_and_add_slab(sp_communication_cell *cell) {
			key_t slab_index = cell->_slab_index;
			//uint32_t offset = cell->_offset;
			SP_slab_types st = cell->_st;
			uint16_t el_count = cell->_el_count;
			slab_adder(slab_index,st,el_count);
		}


		/**
		 * unload_msg_and_remove_slab
		 * 
		 * note: only the caller detaches
		 */
		void unload_msg_and_remove_slab(sp_communication_cell *cell) {
			key_t slab_index = cell->_slab_index;
			SP_slab_types st = cell->_st;
			slab_remover(slab_index, st);
		}


		uint16_t create_slab_and_broadcast(SP_slab_types st,uint16_t el_count) {
			uint32_t offset = 0;
			auto slab_index = gen_slab_index();
			pair<void *,void *> beg_end = create_slab(st,el_count,slab_index,offset);
			void *new_slab = beg_end.first;
			void *end_slab = beg_end.second;
			_stack_ops.set_region((uint8_t *)new_slab,(uint8_t *)end_slab,bytes_needed(st),true);  // new slab, set up free stack
			//
			add_slab_entry(slab_index, new_slab, st, el_count);			// specifically, local tables
			// communicate to consumers
			_slab_events->_readers.store(0);
			_slab_events->_table_change.clear();
			broadcast_slab(slab_index, offset, st, el_count);
			return slab_index;
		}


		//
		void unload_msg_and_create_slab(sp_communication_cell *cell) {
			if ( _allocator ) {
				SP_slab_types st = cell->_st;
				uint16_t el_count = cell->_el_count;
				create_slab_and_broadcast(st,el_count);
			}
		}


		/**
		 * slab_thread_runner
		 */
		void slab_thread_runner([[maybe_unused]] int i) {
			await_event_set();
			while ( _slab_events->_readers.load(std::memory_order_acquire) == 0 );
			handle_receive_slab_event();
		}



		void handle_receive_slab_event(void) {
			//
			sp_communication_cell *cell = _own_cell;
			//
			switch ( _slab_events->_op ) {
				case SP_CELL_ADD: {
					unload_msg_and_add_slab(cell);
					signal_cell_event_set();
					break;
				}
				case SP_CELL_REMOVE: {
					unload_msg_and_remove_slab(cell);
					signal_cell_event_set();
					break;
				}
				case SP_CELL_REQUEST: {
					if ( _allocator ) {			// must have the allocator role
						unload_msg_and_create_slab(cell);
					}
					break;
				}
				default: {
					break;
				}
			}
			//
			if ( _slab_events->_readers.load(std::memory_order_acquire) > 0 ) {
				auto remaining =_slab_events->_readers.fetch_sub(1,std::memory_order_acq_rel);
				if ( remaining == 0 ) {
					clear_event();
				}
			}

		}

		//
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void _to_free(uint8_t *el,uint8_t *slab,uint8_t *end_slab) {
			_stack_ops.set_region(slab,end_slab);
			_stack_ops.push(el);
		}

		uint8_t *_from_free(uint8_t *data,uint8_t *end_data) {
			_stack_ops.set_region(data,end_data);
			return _stack_ops.pop();
		}

		uint16_t init_from_free(key_t slab_index) {
			auto st = SP_slab_t_4;
			uint8_t *slab = _slab_lookup[st][slab_index];
			uint8_t *slab_end =_slab_ender_lookup[st][slab_index];
			uint8_t *section = _from_free(slab,slab_end);
			uint16_t slab_offset = (section - slab);
			return slab_offset;
		}

		bool check_free(uint8_t *data,uint8_t *end_data) {
			_stack_ops.set_region(data,end_data);
			return !_stack_ops.empty();
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void load_bytes(SP_slab_types st, key_t slab_index, uint32_t slab_offset, uint8_t *buffer, uint16_t sz) {
			uint8_t *slab = _slab_lookup[st][slab_index];
			if ( slab == nullptr ) return;
			uint8_t *el = slab + slab_offset;
			if ( el >= _slab_ender_lookup[st][slab_index] ) return;
			memcpy(buffer,el,sz);
		}


		void unload_bytes(SP_slab_types st, key_t slab_index, uint32_t slab_offset, uint8_t *buffer, uint16_t sz) {
			uint8_t *slab = _slab_lookup[st][slab_index];
			if ( slab == nullptr ) return;
			uint8_t *el = slab + slab_offset;
			if ( el >= _slab_ender_lookup[st][slab_index] ) return;
			memcpy(el,buffer,sz);
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void expand(SP_slab_types &st,key_t &slab_index,uint16_t &slab_offset,uint16_t el_count) {
			//
			uint16_t bytes_needed = this->bytes_needed(st);
			uint8_t buffer[bytes_needed];
			//
			// fetch the element from the smaller slab as salvage
			load_bytes(st, slab_index, slab_offset, buffer, bytes_needed);
			uint8_t *slab = _slab_lookup[st][slab_index];
			uint8_t *slab_end =_slab_ender_lookup[st][slab_index];
			uint8_t *el = slab + slab_offset;
			_to_free(el,slab,slab_end);  // release the element
			//
			st = next_slab_type(st);     // the bigger slab
			// try to find a slab with larger elements
			// that is already allocated and has free space
			for ( auto p : _slab_lookup[st] ) {
				uint8_t *buffer = p.second;
				uint8_t *end_buffer = _slab_ender_lookup[st][p.first];
				if ( check_free(buffer,end_buffer) ) {
					uint8_t *section = _from_free(buffer,end_buffer);
					if ( section != nullptr ) {
						slab_offset = (section - buffer);
						slab_index = p.first;
						return;
					}
				}
			}
			//  did not return so, a need another next sized slab
			if ( _allocator ) {						// if the role of allocator is given to this thread/proc
				//
				slab_index = create_slab_and_broadcast(st,el_count);
				//
				uint8_t *slab = _slab_lookup[st][slab_index];
				uint8_t *slab_end = _slab_ender_lookup[st][slab_index];
				//
				uint8_t *section = _from_free(slab,slab_end);
				if ( section != nullptr ) {
					slab_offset = (section - slab);
					return;
				}
				//
			} else {
				clear_cell_event();
				request_slab_and_broadcast(st,el_count);
				await_cell_event_set();
				expand(st,slab_index,slab_offset,el_count);
			}
		}



		void contract(SP_slab_types &st,key_t &slab_index,uint16_t &slab_offset,uint16_t el_count) {
			uint16_t bytes_needed = this->bytes_needed(st);
			uint8_t buffer[bytes_needed];
			//
			// fetch the element from the smaller slab as salvage
			load_bytes(st, slab_index, slab_offset, buffer, bytes_needed);
			uint8_t *slab = _slab_lookup[st][slab_index];
			uint8_t *slab_end =_slab_ender_lookup[st][slab_index];
			uint8_t *el = slab + slab_offset;
			_to_free(el,slab,slab_end);  // release the element
			//
			st = prev_slab_type(st);     // the bigger slab
			// try to find a slab with larger elements
			// that is already allocated and has free space
			for ( auto p : _slab_lookup[st] ) {
				uint8_t *buffer = p.second;
				uint8_t *end_buffer = _slab_ender_lookup[st][p.first];
				if ( check_free(buffer,end_buffer) ) {
					uint8_t *section = _from_free(buffer,end_buffer);
					if ( section != nullptr ) {
						slab_offset = (section - buffer);
						slab_index = p.first;
						return;
					}
				}
			}
			//  did not return so, a need another prev sized slab
			if ( _allocator ) {						// if the role of allocator is given to this thread/proc
				//
				slab_index = create_slab_and_broadcast(st,el_count);
				//
				uint8_t *slab = _slab_lookup[st][slab_index];
				uint8_t *slab_end = _slab_ender_lookup[st][slab_index];
				//
				uint8_t *section = _from_free(slab,slab_end);
				if ( section != nullptr ) {
					slab_offset = (section - slab);
					return;
				}
				//
			} else {
				clear_cell_event();
				request_slab_and_broadcast(st,el_count);
				await_cell_event_set();
				contract(st,slab_index,slab_offset,el_count);
			}
		}



		sp_comm_events									*_slab_events;
		sp_communication_cell							*_slab_com;
		sp_communication_cell							*_end_slab_com;
		uint8_t											_slab_cell;
		sp_communication_cell							*_own_cell;
		uint16_t										_particpating_thread_count;
		bool											_allocator{false};

		Stack_simple									_stack_ops;
		map<SP_slab_types,map<key_t,uint8_t *>> 		_slab_lookup;    // must preload all process
		map<SP_slab_types,map<key_t,uint8_t *>> 		_slab_ender_lookup;    // must preload all process
};


