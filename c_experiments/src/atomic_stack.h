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

		AtomicStack() : _status(true) {
		}

		virtual ~AtomicStack() {}

		uint32_t pop_number(uint8_t *start, StackEl *ctrl_free, uint32_t n, uint32_t *reserved_offsets) {
			//
			auto head = (atomic<uint32_t>*)(&(ctrl_free->_prev));
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
					StackEl *first = (StackEl *)(start + first_offset);
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
			auto head = (atomic<uint32_t>*)(&(ctrl_free->_next));
			uint32_t el_offset = (uint32_t)(((uint8_t *)el) - start);
			uint32_t h_offset = head->load(std::memory_order_relaxed);
			while(!head->compare_exchange_weak(h_offset, el_offset));
		}


		uint16_t setup_region_free_list(uint8_t *start, size_t step, size_t region_size) {

			uint16_t free_count = 0;

			StackEl *ctrl_hdr = (StackEl *)start;
			ctrl_hdr->_prev = UINT32_MAX;
			ctrl_hdr->_next = step;
			ctrl_hdr->_hash = 0;
			ctrl_hdr->_when = 0;
			
			StackEl *ctrl_tail = (StackEl *)(start + step);
			ctrl_tail->_prev = 0;
			ctrl_tail->_next = UINT32_MAX;
			ctrl_tail->_hash = 0;
			ctrl_tail->_when = 0;

			StackEl *ctrl_free = (StackEl *)(start + 2*step);
			ctrl_free->_prev = UINT32_MAX;
			ctrl_free->_next = 3*step;
			ctrl_free->_hash = 0;
			ctrl_free->_when = 0;

			//
			size_t curr = ctrl_free->_next;
			size_t next = 4*step;
			
			while ( curr < region_size ) {   // all the ends are in the first three elements ... the rest is either free or part of the LRU
				free_count++;
				StackEl *next_free = (StackEl *)(start + curr);
				if ( !check_end((uint8_t *)next_free) ) {
					throw "test_lru_creation_and_initialization: run past end of reion";
				}
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

		// ok ----

		bool ok(void) {
			return(this->_status);
		}

	protected: 

		virtual bool check_end([[maybe_unused]] uint8_t *ref,[[maybe_unused]] bool expect_end = false) { return true; }

	public:

		bool							_status;
		const char 						*_reason;


};

