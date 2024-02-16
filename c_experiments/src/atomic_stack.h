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
};

