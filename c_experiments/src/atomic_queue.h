#pragma once



#include <iostream>
#include <atomic>

#include "atomic_stack.h"

using namespace std;


const uint8_t NUM_SHARED_ATOMS = 4;


typedef struct BASIC_Q_ELEMENT_HDR {
	uint32_t	_info;
	uint32_t	_next;
	//
	uint32_t	_prev;

	void init([[maybe_unused]]int i = 0) {}

} Basic_q_element;


template<class QueueEl>
class AtomicQueue : public AtomicStack<QueueEl> {		// ----
	//
	public:

		AtomicQueue() {
		}

		virtual ~AtomicQueue() {}


		/**
		 * pop_queue
		 * 
		 * In this method, the process pops from the tail of the queue. (Elements are pushed onto the front a.k.a. the header.)
		*/
		uint32_t pop_queue(QueueEl &output) {
			//
			uint8_t *start = (uint8_t *)_q_head;
			auto tail_ref = _q_tail;
			uint32_t tail_offset = tail_ref->load(std::memory_order_relaxed);
			//
			if ( tail_offset == 0 ) { // empty, take no action
				return UINT32_MAX;
			}
			//
			incr_pop_count();
			wait_on_push_count();		// one semaphore just in case the queue is near empty during a push
			//
			std::atomic_thread_fence(std::memory_order_acquire);
			// ----
			//
			uint32_t prev_offset = 0;
			uint32_t t_offset = 0;
			do {
				//
				if ( tail_offset == 0 ) {  // tail_offset updates with the failed exchange_weak
					decr_pop_count();
					return(UINT32_MAX);			/// failed memory allocation...
				}
				//
				QueueEl *tail = (QueueEl *)(start + tail_offset); 	// ref next free object
				auto atom_fp = (atomic<uint32_t> *)(&(tail->_prev));
				prev_offset = atom_fp->load(std::memory_order_acquire);
				t_offset = tail_offset;
				//										// if fails, then the new tail will be in tail_offset
			} while( !(tail_ref->compare_exchange_weak(tail_offset, prev_offset)) );  // link ctrl->next to new first
			//
			if ( t_offset > 0 ) {
				QueueEl *tail = (QueueEl *)(start + t_offset);
				output = *tail;
				this->_atomic_stack_push((start + NUM_SHARED_ATOMS*sizeof(atomic<uint32_t>)), tail);
				decr_pop_count();
				return 0;
			}
			//
			decr_pop_count();
			return UINT32_MAX;
		}


		/**
		 * push_queue
		 * 
		 * In this method, the process pushes onto the front of the queue. (Elements are popped from the tail.)
		*/
		uint32_t push_queue(QueueEl &input) {
			uint32_t el_offset = 0;
			uint8_t *start = (uint8_t *)_q_head;
			uint32_t front_offset = 0;
			//
			// Get a free object from the object stack.
			uint32_t rslt = this->pop_number(start + NUM_SHARED_ATOMS*sizeof(atomic<uint32_t>), 1, &el_offset);
			if ( el_offset == 0 || rslt == UINT32_MAX ) {   // out of space (possibly wrong offset)
				return UINT32_MAX;
			}
			//
			incr_push_count();
			wait_on_pop_count();
			//
			std::atomic_thread_fence(std::memory_order_acquire);
			//
			el_offset += NUM_SHARED_ATOMS*sizeof(atomic<uint32_t>);  // adjust to queue frame
			QueueEl *el = (QueueEl *)(start + el_offset);
			*el = input;   // 
			auto atom_fp = (atomic<uint32_t> *)(&(el->_prev));
			atom_fp->store(0);
			//
			do {
				//
				auto head = _q_head;
				front_offset = head->load(std::memory_order_relaxed);
				//
				if ( front_offset == UINT32_MAX ) { // empty, take no action
					decr_push_count();
					return UINT32_MAX;
				}
				// ----
				if ( front_offset == 0 ) {  //  FIRST ELEMENT  ... otherwise the alternative breaks the loop and proceeds below.
					// no real offset is zero, so the front_offset being zero indicates an empty queue.
					// attempt to set the head offset to the first element.
					while ( !(head->compare_exchange_weak(front_offset,el_offset,std::memory_order_acq_rel)) && (front_offset == 0));
					// On sucess, the first offset remains zero.
					if ( front_offset == 0 ) {  // this is the proc that wrote the new header
						// store the tail offset... since this is the only element in the queue, the tail will refer to it.
						_q_tail->store(el_offset,std::memory_order_acq_rel);
						*el = input;						// retrieve value (this op is independent of setting head and tail)
						auto atom_fp = (atomic<uint32_t> *)(&(el->_prev));  // the first element as no previous
						atom_fp->store(0);
						decr_push_count();
						return 0;				// LEAVE ... do not do the operations below
					}
				} else break;
				// other contenders have to wait for the tail reference to be set before trying again.
				while ( _q_tail->load(std::memory_order_relaxed) == 0 ) tick();
				//
			} while (true);
			//
			//	Having gotten here, means that the element being installed is not the first element.
			//	`front_offset` is a reference to the current head that this processes gets to pop.
			// 	Other processes pop other elements 
			auto hdr_offset = front_offset;

			while ( hdr_offset == front_offset ) {  // We expect the header reference to change...
				//
				QueueEl *first = (QueueEl *)(start + front_offset); 	// ref the last inerted element, the header (don't forget: el is the free element)
				//
				auto atom_fp = (atomic<uint32_t> *)(&(first->_prev));	// get the previous value
				auto fp = atom_fp->load(std::memory_order_acquire);
				//
				if ( fp != 0 ) {		// if first is the true header, then it should have no previous, i.e. fp == 0
					while ( hdr_offset == front_offset ) {	// otherwise, try for another header
						front_offset = head->load(std::memory_order_acquire);
					}
					hdr_offset = front_offset;	// try again
					continue;
				}
				//
				auto fp_no_change = fp;
				// Now, it is worth trying to be the one to set the prev of the original header.
				// This narrows down the control over the header to just one proc/thread.
				while ( !(atom_fp->compare_exchange_weak(fp, el_offset, std::memory_order_acq_rel)) && (fp == fp_no_change) )
				;
				if ( fp != fp_no_change ) {
					// breakage of the weak exchange has been handled, 
					// and this proc/thread failed to set fp (prev of the original header)
					while ( hdr_offset == front_offset ) {		// get the header that scooped the backref
						front_offset = head->load(std::memory_order_acquire);  // A see B below
					}
					hdr_offset = front_offset;		// try again
					continue;
				}
				// This proc is the one that set the previous ref of the original header 
				// Other procs may be waiting on this change.
				// If more than one valid operation has gotten to this point 
				head->store(std::memory_order_release);
				decr_push_count();
				return rslt;
			}
			decr_push_count();
			return rslt;
		}


		bool empty(void) {
			auto cur_tail = _q_tail->load();
			return (cur_tail == 0);
		}

		bool full(void) {
			return this->free_mem_empty();
		}

		void incr_push_count(void) {
			_q_push_count->fetch_add(1,std::memory_order_acquire);
		}

		void decr_push_count(void) {
			auto check = _q_push_count->fetch_sub(1,std::memory_order_release);
			if ( check == 0 ) {
				_q_push_count->fetch_add(1,std::memory_order_acquire);
			}
		}

		void incr_pop_count(void) {
			_q_pop_count->fetch_add(1,std::memory_order_acquire);
		}

		void decr_pop_count(void) {
			auto check = _q_pop_count->fetch_sub(1,std::memory_order_release);
			if ( check == 0 ) {
				_q_pop_count->fetch_add(1,std::memory_order_acquire);
			}
		}
		/**
		 * wait_on_push_count
		 * 
		 * This method waits on push operation only when there is just one element in the queue.
		 * The aim is to prevent removing the head when it is being used in order to anchor a new element.
		 * Other pop operations can proceed without intefering with push operations, as they affect following elements.
		 */

		void wait_on_push_count(void) {
			if ( _q_head->load(std::memory_order_acquire) == _q_tail->load(std::memory_order_acquire) ) {
				while ( _q_push_count->load(std::memory_order_acquire) > 0 ) {
					tick();
					while ( _q_head->load(std::memory_order_acquire) == _q_tail->load(std::memory_order_acquire) ) {
						tick();
					}
				}
			}
		}

		/**
		 * wait_on_pop_count
		 * 
		 */

		void wait_on_pop_count(void) {
			if ( _q_head->load(std::memory_order_acquire) == _q_tail->load(std::memory_order_acquire) ) {
				while ( _q_pop_count->load(std::memory_order_acquire) > 0 ) {
					tick();
					while ( !(this->empty()) ) tick();
				}
			}
		}

		uint16_t setup_queue_region(uint8_t *start, size_t step, size_t region_size) {
			_q_head = (atomic<uint32_t> *)start;
			_q_tail = _q_head + 1;
			_q_head->store(0);
			_q_tail->store(0);
			_q_push_count->store(0);
			_q_pop_count->store(0);
			auto sz = region_size - NUM_SHARED_ATOMS*sizeof(atomic<uint32_t>);
			return this->setup_region_free_list((start + NUM_SHARED_ATOMS*sizeof(atomic<uint32_t>)),step,sz);
		}


		void attach_queue_region(uint8_t *start, size_t region_size) {
			_q_head = (atomic<uint32_t> *)start;
			_q_tail = _q_head + 1;
			auto sz = region_size - NUM_SHARED_ATOMS*sizeof(atomic<uint32_t>);
			this->attach_region_free_list((start+ NUM_SHARED_ATOMS*sizeof(atomic<uint32_t>)), sz);
		}

		static size_t check_region_size(uint32_t el_count) {
			size_t total_size = NUM_SHARED_ATOMS*sizeof(atomic<uint32_t>) + AtomicStack<QueueEl>::check_region_size(el_count);
			return total_size;
		}

	public:

		atomic<uint32_t> 				*_q_head;
		atomic<uint32_t> 				*_q_tail;
		atomic<uint32_t> 				*_q_push_count;
		atomic<uint32_t> 				*_q_pop_count;
		//

};
