#pragma once



#include <iostream>
#include <atomic>

#include "atomic_stack.h"

using namespace std;


const uint8_t NUM_SHARED_ATOMS_Q = 5;


typedef struct BASIC_Q_ELEMENT_HDR {
	uint32_t	_info;
	uint32_t	_next;
	//
	uint32_t	_prev;
	uint8_t		_proc_id;

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
			add_popper();
			//
			uint8_t *start = (uint8_t *)_q_head;
			auto tail_ref = _q_tail;
			uint32_t tail_offset = tail_ref->load(std::memory_order_relaxed);
			//
			if ( tail_offset == 0 ) { // empty, take no action
				remove_popper();
				return UINT32_MAX;
			}
			//
			std::atomic_thread_fence(std::memory_order_acquire);
			// ----
			//
			uint32_t prev_offset = 0;
			uint32_t t_offset = 0;
			do {
				//
				if ( tail_offset == 0 ) {  // tail_offset updates with the failed exchange_weak
					auto head = _q_head;
					head->store(0,std::memory_order_release);
					remove_popper();
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
				this->_atomic_stack_push((start + NUM_SHARED_ATOMS_Q*sizeof(atomic<uint32_t>)), tail);
				if ( prev_offset == 0 ) {
					auto head = _q_head;
					head->store(0,std::memory_order_release);
				}
				remove_popper();
				return 0;
			} else {
				_q_head->store(0,std::memory_order_release);
			}
			//
			remove_popper();
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
			uint32_t tail_offset = 0;
			//
			// Get a free object from the object stack.
			uint32_t rslt = this->pop_number(start + NUM_SHARED_ATOMS_Q*sizeof(atomic<uint32_t>), 1, &el_offset);
			if ( el_offset == 0 || rslt == UINT32_MAX ) {   // out of space (possibly wrong offset)
				return UINT32_MAX;
			}
			//
			add_pusher();
			//
			std::atomic_thread_fence(std::memory_order_acquire);
			//
			el_offset += NUM_SHARED_ATOMS_Q*sizeof(atomic<uint32_t>);  // adjust to queue frame
			QueueEl *el = (QueueEl *)(start + el_offset);
			*el = input;   // 
			auto atom_fp = (atomic<uint32_t> *)(&(el->_prev));
			atom_fp->store(0);
			//
			auto head = _q_head;
			auto tail = _q_tail;

			//
			do {
				//
				front_offset = head->load(std::memory_order_relaxed);
				tail_offset = tail->load(std::memory_order_relaxed);
				//
				if ( front_offset == UINT32_MAX ) { // empty, take no action
					remove_pusher();
					return UINT32_MAX;
				}
				// ----
				if ( front_offset == 0 ) {  //  FIRST ELEMENT  ... otherwise the alternative breaks the loop and proceeds below.
					// no real offset is zero, so the front_offset being zero indicates an empty queue.
					// attempt to set the head offset to the first element.
uint32_t check1 = 0;
					while ( !(head->compare_exchange_weak(front_offset,el_offset,std::memory_order_acq_rel)) && (front_offset == 0) )
					{ check1++; if ( !(check1%10) ) { cout << "check1::" << check1 << endl;} };
					// On sucess, the first offset remains zero.
					if ( front_offset == 0 ) {  // this is the proc that wrote the new header
						// store the tail offset... since this is the only element in the queue, the tail will refer to it.
uint32_t check2 = 0;
						while ( !(_q_tail->compare_exchange_weak(tail_offset,el_offset,std::memory_order_acq_rel) ) )
						{ check2++; if ( !(check2%10) ) { cout << "check2::" << check2 << endl; } };
						//_q_tail->store(el_offset,std::memory_order_release);
						*el = input;						// retrieve value (this op is independent of setting head and tail)
						auto atom_fp = (atomic<uint32_t> *)(&(el->_prev));  // the first element as no previous
						atom_fp->store(0);
						remove_pusher();
						return 0;				// LEAVE ... do not do the operations below
					}
				} else break;
				// other contenders have to wait for the tail reference to be set before trying again.
uint32_t check3 = 0;
				while ( _q_tail->load(std::memory_order_relaxed) == 0 ) 
				{ tick(); check3++; if ( !(check3%10) ) { cout << "check3::" << check3 << endl;} };
				//
			} while (true);
			//
			//	Having gotten here, means that the element being installed is not the first element.
			//	`front_offset` is a reference to the current head that this processes gets to pop.
			// 	Other processes pop other elements 
			auto hdr_offset = front_offset;

uint32_t check4 = 0;
			while ( hdr_offset == front_offset ) {  // We expect the header reference to change...
				//
				QueueEl *first = (QueueEl *)(start + front_offset); 	// ref the last inerted element, the header (don't forget: el is the free element)
				//
				auto atom_fp = (atomic<uint32_t> *)(&(first->_prev));	// get the previous value
				auto fp = atom_fp->load(std::memory_order_acquire);
				//
				if ( fp != 0 ) {		// if first is the true header, then it should have no previous, i.e. fp == 0
uint32_t check5 = 0;
					while ( hdr_offset == front_offset ) {	// otherwise, try for another header
						front_offset = head->load(std::memory_order_acquire);
						//
						{ check5++; if ( !(check5%10) ) { cout << "check5::" << check5 << endl;} }
					}
					hdr_offset = front_offset;	// try again
					{ check4++; if ( !(check4%10) ) { cout << "check4::" << check4 << endl;} }
					continue;
				}
				//
				auto fp_no_change = fp;
				// Now, it is worth trying to be the one to set the prev of the original header.
				// This narrows down the control over the header to just one proc/thread.
				// --- old first, now gets the new first in its prev
uint32_t check6 = 0;
				while ( !(atom_fp->compare_exchange_weak(fp, el_offset, std::memory_order_acq_rel)) && (fp == fp_no_change) )
				{ check6++; if ( !(check6%10) ) { cout << "check6::" << check6 << endl;} };
				if ( fp != fp_no_change ) {
					// breakage of the weak exchange has been handled, 
					// and this proc/thread failed to set fp (prev of the original header)
uint32_t check7 = 0;
					while ( hdr_offset == front_offset ) {		// get the header that scooped the backref
						front_offset = head->load(std::memory_order_acquire);  // A see B below
{ check7++; if ( !(check7%10) ) { cout << "check7::" << check7 << endl;} }
					}
					hdr_offset = front_offset;		// try again
					continue;
				}
				// This proc is the one that set the previous ref of the original header 
				// Other procs may be waiting on this change.
				// If more than one valid operation has gotten to this point 
				head->store(el_offset,std::memory_order_release);
				break;
			}
			remove_pusher();
			return rslt;
		}


		bool empty(void) {
			auto cur_tail = _q_tail->load();
			return (cur_tail == 0);
		}

		bool full(void) {
			return this->free_mem_empty();
		}


/*
    {
        auto oldValue = s_.load();
        while ( (oldValue == 0) || !(s_.compare_exchange_strong(oldValue, oldValue - 1)) ) {
           oldValue = s_.load();
		}
    }

	// ----
	auto cur_count = _q_shared_count->fetch_add(1,std::memory_order_acquire);
	if ( cur_count == 0 ) {
		auto cnt = _q_pop_count->load(std::memory_order_acquire);
		if ( cnt == 0 ) {
			_popper->clear();
			return;
		}
		_q_shared_count->store(0,std::memory_order_release);
		while ( cnt > 0 ) {
			cnt = _q_pop_count->load(std::memory_order_acquire);
			tick();
		}
		_q_shared_count->store(1,std::memory_order_release);
	}

*/
		// void incr_push_count(void) {  // _popper --
		// 	while ( !(_popper->test_and_set()) )
		// 	;
		// 	_q_shared_count->fetch_add(1,std::memory_order_acquire);
		// 	_popper->clear();
		// }

		// void incr_pop_count(void) {
		// 	while ( !(_popper->test_and_set()) )
		// 	;
		// 	_q_pop_count->fetch_add(1,std::memory_order_acquire);
		// 	_popper->clear();
		// }

		// // ----
		// void decr_push_count(void) {
		// 	auto check = _q_shared_count->fetch_sub(1,std::memory_order_release);
		// 	if ( check == 0 ) {
		// 		_q_shared_count->store(0,std::memory_order_release);
		// 	}
		// }

		// // ----
		// void decr_pop_count(void) {
		// 	auto check = _q_pop_count->fetch_sub(1,std::memory_order_release);
		// 	if ( check == 0 ) {
		// 		_q_pop_count->store(0,std::memory_order_release);
		// 	}
		// }


		// 

		void add_operator(uint32_t incr,uint32_t mask) {
			while ( true ) {
				//
				auto check = _q_shared_count->load(std::memory_order_acquire);
				while ( check & mask ) {
					tick();
					check = _q_shared_count->load(std::memory_order_acquire);
				}
				auto old_check = check;
				auto new_check = (check+incr);
				while ( !(_q_shared_count->compare_exchange_strong(check, new_check,std::memory_order_acq_rel)) ) {
					if ( old_check != check ) {
						if ( check & mask ) break;
						new_check = (check+incr);
						old_check = check;
					}
				}
				if ( check & mask ) continue;
				break;
				//
			}
		}


		void remove_operator(uint32_t incr,uint32_t mask,uint32_t alter_mask) {
			//
			auto check = _q_shared_count->load(std::memory_order_acquire);
			if ( (alter_mask & check) == 0 ) return;
			//
			auto old_check = check;
			auto new_check = (check-incr);
			while ( !(_q_shared_count->compare_exchange_strong(check, new_check, std::memory_order_acq_rel)) ) {
				if ( old_check != check ) {
					if ( (alter_mask & check) == 0 ) return;
					new_check = (check-incr);
					old_check = check;
				}
			}
			//
		}


		void add_popper(void) {
			uint32_t incr = 0x00010000;
			uint32_t mask = 0x0000FFFF;
			//uint32_t alter_mask = 0xFFFF0000;		
			add_operator(incr, mask);
		}

		void add_pusher(void) {
			uint32_t incr = 0x1;
			uint32_t mask = 0xFFFF0000;
			//uint32_t alter_mask = 0x0000FFFF;		
			add_operator(incr, mask);
		}

		void remove_popper(void) {
			uint32_t incr = 0x00010000;
			uint32_t mask = 0x0000FFFF;
			uint32_t alter_mask = 0xFFFF0000;		
			remove_operator(incr, mask, alter_mask);
		}
		void remove_pusher(void) {
			uint32_t incr = 0x1;
			uint32_t mask = 0xFFFF0000;
			uint32_t alter_mask = 0x0000FFFF;		
			remove_operator(incr, mask, alter_mask);
		}


		/**
		 * wait_on_push_count
		 * 
		 * This method waits on push operation only when there is just one element in the queue.
		 * The aim is to prevent removing the head when it is being used in order to anchor a new element.
		 * Other pop operations can proceed without intefering with push operations, as they affect following elements.
		 */

		// void wait_on_push_count(void) {
		// 	auto cnt = _q_shared_count->load(std::memory_order_acquire);
		// 	while ( cnt > 0 ) {
		// 		tick();
		// 		cnt = _q_shared_count->load(std::memory_order_acquire);
		// 	}
		// }

		/**
		 * wait_on_pop_count
		 * 
		 */

		// void wait_on_pop_count(void) {
		// 	//
		// 	auto cnt = _q_pop_count->load(std::memory_order_acquire);
		// 	while ( cnt > 0 ) {
		// 		tick();
		// 		cnt = _q_pop_count->load(std::memory_order_acquire);
		// 	}
		// }


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		uint16_t setup_queue_region(uint8_t *start, size_t step, size_t region_size) {
			_q_head = (atomic<uint32_t> *)start;
			_q_tail = _q_head + 1;
			_q_head->store(0);
			_q_tail->store(0);
			_q_shared_count = _q_tail + 1;
			_q_shared_count->store(0);
			_q_pop_count = _q_shared_count + 1;
			_q_pop_count->store(0);
			_popper = (atomic_flag *)(_q_pop_count + 1);
			_popper->clear();
			auto sz = region_size - NUM_SHARED_ATOMS_Q*sizeof(atomic<uint32_t>);
			// setup
			return this->setup_region_free_list((start + NUM_SHARED_ATOMS_Q*sizeof(atomic<uint32_t>)),step,sz);
		}


		void attach_queue_region(uint8_t *start, size_t region_size) {
			_q_head = (atomic<uint32_t> *)start;
			_q_tail = _q_head + 1;
			_q_shared_count = _q_tail + 1;
			_q_pop_count = _q_shared_count + 1;
			_popper = (atomic_flag *)(_q_pop_count + 1);
			auto sz = region_size - NUM_SHARED_ATOMS_Q*sizeof(atomic<uint32_t>);
			// attach
			this->attach_region_free_list((start+ NUM_SHARED_ATOMS_Q*sizeof(atomic<uint32_t>)), sz);
		}

		static size_t check_region_size(uint32_t el_count) {
			size_t total_size = NUM_SHARED_ATOMS_Q*sizeof(atomic<uint32_t>) + AtomicStack<QueueEl>::check_region_size(el_count);
			return total_size;
		}

		static uint8_t atomics_count(void) {
			return NUM_SHARED_ATOMS_Q + AtomicStack<QueueEl>::atomics_count();
		}

	public:

		atomic<uint32_t> 				*_q_head;
		atomic<uint32_t> 				*_q_tail;
		atomic<uint32_t> 				*_q_shared_count;
		atomic<uint32_t> 				*_q_pop_count;
		atomic_flag						*_popper;
		//

};
