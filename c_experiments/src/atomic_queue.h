#pragma once



#include <iostream>
#include <atomic>

#include "atomic_stack.h"

using namespace std;


typedef struct BASIC_Q_ELEMENT_HDR {
	uint32_t	_info;
	uint32_t	_next;
	//
	uint32_t	_prev;

} Basic_q_element;


template<class QueueEl>
class AtomicQueue : public AtomicStack<QueueEl> {		// ----
	//
	public:

		AtomicQueue() {
		}

		virtual ~AtomicQueue() {}


		/**
		 * pop_number
		*/
		uint32_t pop_queue(QueueEl &output) {
			//
			uint8_t *start = (uint8_t *)_q_head;
			auto tail = _q_tail;
			uint32_t tail_prev_offset = tail->load(std::memory_order_relaxed);
			//
			if ( tail_prev_offset == 0 ) { // empty, take no action
				return UINT32_MAX;
			}
			//
			std::atomic_thread_fence(std::memory_order_acquire);
			// ----
			//
			uint32_t prev_offset = 0;
			uint32_t t_p_offset = 0;
			do {
				if ( t_p_offset == 0 ) {
					return(0);			/// failed memory allocation...
				}
				t_p_offset = tail_prev_offset;
				//
				QueueEl *prev = (QueueEl *)(start + t_p_offset); 	// ref next free object
				prev_offset = prev->_prev;								// next of next free
				//
			} while( !(tail->compare_exchange_weak(tail_prev_offset, prev_offset)) );  // link ctrl->next to new first
			//
			if ( t_p_offset > 0 ) {
				QueueEl *prev = (QueueEl *)(start + t_p_offset);
				output = *prev;
				this->_atomic_stack_push((start + 2*sizeof(atomic<uint32_t>)), prev);
				return 0;
			}
			//
			return UINT32_MAX;
		}



		uint32_t push_queue(QueueEl &input) {
			uint32_t el_offset = 0;
			uint8_t *start = (uint8_t *)_q_head;
			uint32_t rslt = this->pop_number(start + 2*sizeof(QueueEl), 1, &el_offset);
			if ( rslt < UINT32_MAX ) {
				QueueEl *el = (QueueEl *)(start + 2*sizeof(QueueEl) + el_offset);
				*el = input;
				{
					el->_prev = 0;
					//
					auto head = _q_head;
					uint32_t first_offset = head->load(std::memory_order_relaxed);
					//
					if ( first_offset == UINT32_MAX ) { // empty, take no action
						return UINT32_MAX;
					}
					//
					std::atomic_thread_fence(std::memory_order_acquire);
					// ----
					//
					auto hdr_offset = first_offset;
					while ( hdr_offset == first_offset ) {
						QueueEl *first = (QueueEl *)(start + first_offset); 	// ref next free object
						auto atom_fp = (atomic<uint32_t> *)(&(first->_prev));
						auto fp = atom_fp->load(std::memory_order_acquire);
						if ( fp != 0 ) {
							while ( hdr_offset == first_offset ) {
								first_offset = head->load(std::memory_order_acquire);
							}
							hdr_offset = first_offset;
							continue;
						}
						if ( !(atom_fp->compare_exchange_weak(fp, el_offset)) ) {
							fp = atom_fp->load(std::memory_order_acquire);
							if ( fp != el_offset ) {
								while ( hdr_offset == first_offset ) {
									first_offset = head->load(std::memory_order_acquire);
								}
								hdr_offset = first_offset;
								continue;
							}
						}
						while ( !(head->compare_exchange_weak(first_offset,el_offset,std::memory_order_acq_rel)) ) {
							auto cur_prev = (atomic<uint32_t> *)(&(el->_prev));
							if ( cur_prev->load(std::memory_order_relaxed) != 0 ) break;
							first_offset = head->load(std::memory_order_acquire);
						}
					}
				}
			}
			return rslt;
		}


		bool empty(void) {
			auto cur_tail = _q_tail->load();
			return (cur_tail == 0);
		}


		uint16_t setup_queue_region(uint8_t *start, size_t step, size_t region_size) {
			_q_head = (atomic<uint32_t> *)start;
			_q_tail = _q_head + 1;
			_q_head->store(1);
			_q_tail->store(0);
			auto sz = region_size - 2*sizeof(atomic<uint32_t>);
			return this->setup_region_free_list((start + 2*sizeof(atomic<uint32_t>)),step,sz);
		}


		void attach_queue_region(uint8_t *start, size_t region_size) {
			_q_head = (atomic<uint32_t> *)start;
			_q_tail = _q_head + 1;
			auto sz = region_size - 2*sizeof(atomic<uint32_t>);
			this->attach_region_free_list((start+ 2*sizeof(atomic<uint32_t>)), sz);
		}

	public:

		atomic<uint32_t> 				*_q_head;
		atomic<uint32_t> 				*_q_tail;
		//

};
