#pragma once



#include <iostream>
#include <atomic>


using namespace std;


typedef struct BASIC_ELEMENT_HDR {
	uint32_t	_info;
	uint32_t	_next;
} Basic_element;


template<class StackEl>
class AtomicStack {		// ----
	//
	public:

		AtomicStack() : _status(true) {
		}

		virtual ~AtomicStack() {}


		/**
		 * pop_number
		*/
		uint32_t pop_number(uint8_t *start, uint32_t n, uint32_t *reserved_offsets) {
			//
			auto head = (atomic<uint32_t>*)(&(_ctrl_free->_next));
			uint32_t hdr_offset = head->load(std::memory_order_relaxed);
			//
			if ( hdr_offset == UINT32_MAX ) {
				_status = false;
				_reason = "out of free memory: free count == 0";
				return(UINT32_MAX);
			}
			//
			// POP as many as needed
			//
			auto fc = _count_free->load(std::memory_order_acquire);

			if ( (fc < n) && _backout_overflow ) {
				_status = false;
				_reason = "potential free memory overflow: free count == 0";
				return(UINT32_MAX);			/// failed memory allocation...
			}

			reduce_free_count(fc,n);
			//
			std::atomic_thread_fence(std::memory_order_acquire);
			
			// ----

			uint32_t *tmp_p = reserved_offsets;
			//
			while ( n-- ) {  // consistently pop the free stack
				uint32_t next_offset = UINT32_MAX;
				uint32_t first_offset = UINT32_MAX;
				do {
					if ( hdr_offset == UINT32_MAX ) {
						_status = false;
						_reason = "out of free memory: free count == 0";
						return(UINT32_MAX);			/// failed memory allocation...
					}

					first_offset = hdr_offset;
					StackEl *first = (StackEl *)(start + first_offset); 	// ref next free object
					next_offset = first->_next;								// next of next free
				} while( !(head->compare_exchange_weak(hdr_offset, next_offset)) );  // link ctrl->next to new first
				//
				if ( first_offset < UINT32_MAX ) {
					*tmp_p++ = first_offset;  // hdr_offset should have changed
				}
			}

			return 0;
		}


		/**
		 * _atomic_stack_push
		*/
		void _atomic_stack_push(uint8_t *start, StackEl *el) {
			if ( !full() ) {
				auto head = (atomic<uint32_t>*)(&(_ctrl_free->_next));		// whereever this is pointing now (may be UINT32_MAX)
				uint32_t el_offset = (uint32_t)(((uint8_t *)el) - start);	// 
				uint32_t hdr_offset = head->load(std::memory_order_relaxed);
				el->_next = hdr_offset;
				while(!head->compare_exchange_weak(hdr_offset, el_offset));
				increment_free_count();
			}
		}

		void _atomic_stack_push(uint8_t *start, uint32_t el_offset) {
			StackEl *el = (StackEl *)(start + el_offset);
			_atomic_stack_push(start, el);
		}


		/**
		 * increment_free_count
		*/

		void increment_free_count(void) {
			auto count = _count_free->fetch_add(1,std::memory_order_acquire);
			if ( count >= _max_free_local ) {
				count++;
				while ( !(_count_free->compare_exchange_weak(count,_max_free_local,std::memory_order_acq_rel)) && (count > _max_free_local) );
				if ( count < _max_free_local ) {
					_count_free->fetch_sub(1,std::memory_order_acq_rel);  // remove the unseen extra
				}
			}
		}


		/**
		 * reduce_free_count
		*/

		void reduce_free_count(uint32_t fc,uint32_t n) {
			auto store_this = fc;
			do {
				if (fc == 0) break;
				if ( fc < n ) {
					store_this = 0;
				} else {
					store_this = (fc - n);
				}
				while ( !(_count_free->compare_exchange_weak(fc, store_this, std::memory_order_relaxed)) && (store_this != fc) );
			} while ( store_this != fc );
		}


		/**
		 * full
		*/
		bool full(void) {
			bool status = false;
			if ( _max_free_local == _count_free->load() ) status = true;
			return status;
		}

		/**
		 * 	step -- step is the size of the list object header plus the object data allowed length..
		*/
		uint16_t setup_region_free_list(uint8_t *start, size_t step, size_t region_size) {

			uint16_t free_count = 0;

			_stack_region_end = start + region_size;

			_count_free = (atomic<uint32_t>*)(start);		// whereever this is pointing now (may be UINT32_MAX)
			_max_free = (atomic<uint32_t>*)(start + sizeof(atomic<uint32_t>*));		// whereever this is pointing now (may be UINT32_MAX)

			//
			_ctrl_free = (StackEl *)(start + 2*sizeof(atomic<uint32_t>*));
			_ctrl_free->init(0);
			_ctrl_free->_next = (step + 2*sizeof(atomic<uint32_t>*));	// step in bytes

			//
			size_t curr = _ctrl_free->_next;
			size_t next = curr + step;
			
			StackEl *last_free = nullptr;
			while ( curr < region_size ) {   // all the ends are in the first three elements ... the rest is either free or part of the LRU
				free_count++;
				StackEl *next_free = (StackEl *)(start + curr);
				last_free = next_free;
				if ( !check_end((uint8_t *)next_free) ) {
					throw "test_lru_creation_and_initialization: run past end of reion";
				}
				_ctrl_free->init();
				next_free->_next = next;
				if ( next >= region_size ) {
					next_free->_next = UINT32_MAX;
				}
				//
				curr += step;
				next += step;
			}

			if ( last_free ) {
				last_free->_next = UINT32_MAX;
			}

			_count_free->store(free_count);
			_max_free->store(free_count);
			_max_free_local = free_count;

			return free_count;
		}


		void attach_region_free_list(uint8_t *start, size_t region_size) {
			_stack_region_end = start + region_size;
			_count_free = (atomic<uint32_t>*)(start);		// whereever this is pointing now (may be UINT32_MAX)
			_max_free = (atomic<uint32_t>*)(start + sizeof(atomic<uint32_t>*));		// whereever this is pointing now (may be UINT32_MAX)
			_ctrl_free = (StackEl *)(start + 2*sizeof(atomic<uint32_t>*));
			_max_free_local = _max_free->load();
		}

		// ok ----

		bool ok(void) {
			return(this->_status);
		}

	protected: 

		virtual bool check_end([[maybe_unused]] uint8_t *ref,[[maybe_unused]] bool expect_end = false) { 
			if ( _stack_region_end <= ref ) {
				return false;
			}
			return true; 
		}

	public:


		bool							_status;
		bool							_backout_overflow {false};
		const char 						*_reason {""};
		uint8_t							*_stack_region_end;
		StackEl 						*_ctrl_free;
		//
		atomic<uint32_t>				*_count_free;
		atomic<uint32_t>				*_max_free;  // so other threads can read it
		uint32_t						_max_free_local;

};
