#pragma once



#include <iostream>
#include <atomic>


using namespace std;


typedef struct BASIC_ELEMENT_HDR {
	uint32_t	_prev;
	uint32_t	_next;
} Basic_element;




template<class StackEl>
class AtomicStack {
	public:

		AtomicStack() {
		}

		virtual ~AtomicStack() {}

		uint32_t pop_number(uint8_t *start, StackEl *ctrl_free, uint32_t n, uint32_t *reserved_offsets) {
			//
			auto head = static_cast<atomic<uint32_t>*>(&(ctrl_free->_prev));
			uint32_t h_offset = head->load(std::memory_order_relaxed);
			if ( h_offset == UINT32_MAX ) {
				_status = false;
				_reason = "out of free memory: free count == 0";
				return(UINT32_MAX);
			}
			//
			// POP as many as needed
			//
			while ( n-- ) {  // consistently pop the free stack
				uint32_t next_offset = UINT32_MAX;
				uint32_t first_offset = UINT32_MAX;
				do {
					if ( h_offset == UINT32_MAX ) {
						_status = false;
						_reason = "out of free memory: free count == 0";
						return(UINT32_MAX);			/// failed memory allocation...
					}
					first_offset = h_offset;
					first = (StackEl *)(start + first_offset);
					next_offset = first->_next;
				} while( !(head->compare_exchange_weak(h_offset, next_offset)) );  // link ctrl->next to new first
				//
				if ( next_offset < UINT32_MAX ) {
					reserved_offsets[n] = first_offset;  // h_offset should have changed
				}
			}
			return 0;
		}

		// _atomic_stack_push
		void _atomic_stack_push(uint8_t *start, StackEl *ctrl_free, StackEl *el) {
			auto head = static_cast<atomic<uint32_t>*>(&(ctrl_free->_next));
			uint32_t el_offset = (uint32_t)(el - start);
			uint32_t h_offset = head->load(std::memory_order_relaxed);
			while(!head->compare_exchange_weak(h_offset, el_offset));
		}



		uint16_t setup_region_free_list(size_t record_size, uint8_t *start, size_t step, size_t region_size) {

			uint16_t free_count = 0;

			LRU_element *ctrl_hdr = (LRU_element *)start;
			ctrl_hdr->_prev = UINT32_MAX;
			ctrl_hdr->_next = step;
			ctrl_hdr->_hash = 0;
			ctrl_hdr->_when = 0;
			
			LRU_element *ctrl_tail = (LRU_element *)(start + step);
			ctrl_tail->_prev = 0;
			ctrl_tail->_next = UINT32_MAX;
			ctrl_tail->_hash = 0;
			ctrl_tail->_when = 0;

			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);
			ctrl_free->_prev = UINT32_MAX;
			ctrl_free->_next = 3*step;
			ctrl_free->_hash = 0;
			ctrl_free->_when = 0;

			//
			size_t curr = ctrl_free->_next;
			size_t next = 4*step;
			
			while ( curr < region_size ) {   // all the ends are in the first three elements ... the rest is either free or part of the LRU
				free_count++;
				LRU_element *next_free = (LRU_element *)(start + curr);
				next_free->_prev = UINT32_MAX;  // singly linked free list
				next_free->_next = next;
				if ( next >= region_size ) {
					next_free->_next = UINT32_MAX;
				}
				next_free->_hash = UINT64_MAX;
				next_free->_when = 0;
				//
				curr += step;
				next += step;
			}

			ctrl_free->_hash = 0;   // how many free elements avaibale
			ctrl_hdr->_hash = 0;

			return free_count;
		}

};

