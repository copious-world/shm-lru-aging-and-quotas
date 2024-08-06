#pragma once



#include <iostream>
#include <atomic>
#include <map>
#include <stdexcept>
#include <string>

using namespace std;

#include "tick.h"
#include <sys/ipc.h>


// SPARSE SLAB OPTION
// a wait based operation option without interleaving (hence, excess memory)
// fairly usual hash table... 
// (tested)

typedef struct STACK_el_header {
	uint16_t				_next;
} stack_el_header;


typedef struct STACK_el_stack__header {
	uint16_t				_next;
	uint16_t				_count;
} stack_stack_header;

/**
 * Stack_simple -- this is just a simple stack using offset values.
 * 
 */
class Stack_simple {
	public:

		Stack_simple() {}
		virtual ~Stack_simple(void) {}

		void set_region(uint8_t *beg, uint8_t *end,uint16_t el_size = 0,bool initialize = false) {
			if ( (beg + sizeof(stack_stack_header) + sizeof(stack_el_header)) >= end ) {
				string  msg = "(Stack_simple::set_region) stack region is too small for operation.";
				throw std::runtime_error(msg);
			}
			_region = beg;
			_end_region = end;
			if ( initialize ) {
				stack_stack_header *stack_header = (stack_stack_header *)beg;
				//
				stack_header->_next = sizeof(stack_stack_header);
				stack_header->_count = 0;
				//
				stack_el_header *seh = (stack_el_header *)stack_header;
				uint8_t *nxt = beg + stack_header->_next;
				while ( nxt < end ) {
					stack_el_header *n_seh = (stack_el_header *)nxt;
					n_seh->_next = seh->_next + sizeof(stack_el_header) + el_size;
					nxt = beg + n_seh->_next;
					seh = n_seh;
					stack_header->_count++;
				}
				if ( seh != nullptr ) {
					seh->_next = UINT16_MAX;
				}
			}
		}


		static uint32_t check_expected_stack_region_size(uint32_t el_size, uint32_t num_els) {
			auto stack_head = sizeof(stack_stack_header);
			auto el_head = sizeof(stack_el_header);
			auto el_reagion_size = (el_head + el_size)*num_els;
			return (el_reagion_size + stack_head);
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		uint8_t *pop(void) {
			//
			stack_stack_header *stack_header = (stack_stack_header *)_region;
			auto top_off = stack_header->_next;
			if ( (top_off == UINT16_MAX) || (stack_header->_count == 0)) {
				return nullptr;
			}
			stack_header->_count--;
			stack_el_header *top = (stack_el_header *)(_region + top_off);
			stack_header->_next = top->_next;
			uint8_t *el = (uint8_t *)(top + 1);
			return el;
		}

		void push(uint8_t *el) {
			stack_el_header *ret = (stack_el_header *)(el - sizeof(stack_el_header));
			stack_stack_header *stack_header = (stack_stack_header *)_region;
			ret->_next = stack_header->_next;
			cout << "push " << ret->_next;
			stack_header->_next = (el - _region) - sizeof(stack_el_header);
			cout << "  " << stack_header->_next << endl;
			stack_header->_count++;
		}

		bool empty(void) {
			stack_stack_header *stack_header = (stack_stack_header *)_region;
			return (stack_header->_count == 0);
		}

		uint16_t count(void) {
			stack_stack_header *stack_header = (stack_stack_header *)_region;
			return (stack_header->_count);
		}

		uint8_t 				*_region;
		uint8_t					*_end_region;
	
};




class Stack_simple_test : public Stack_simple {
	public:

		Stack_simple_test() {}
		virtual ~Stack_simple_test(void) {}

		//
		void set_region_and_walk(uint8_t *beg, uint8_t *end) {
			if ( (beg + sizeof(stack_stack_header) + sizeof(stack_el_header)) >= end ) {
				string  msg = "(Stack_simple::set_region) stack region is too small for operation.";
				throw std::runtime_error(msg);
			}
			_region = beg;
			_end_region = end;
			//
			stack_stack_header *stack_header = (stack_stack_header *)beg;
			//
			cout << "set_region_and_walk: " << stack_header->_next << endl;
			//
			uint8_t *nxt = beg + stack_header->_next;
			while ( nxt < end ) {
				stack_el_header *n_seh = (stack_el_header *)nxt;
				cout << "set_region_and_walk: " << n_seh->_next << endl;
				nxt = beg + n_seh->_next;
				auto total_el = (int)(nxt - (uint8_t *)n_seh);
				cout << total_el << " " << sizeof(stack_el_header) << " " << (total_el - sizeof(stack_el_header)) << endl;
			}
		}

		void walk_stack(void) {
			if ( (_region == nullptr) ||  ( _end_region <= _region) ) {
				cout << "bad region" << endl;
				return;
			}
			stack_stack_header *stack_header = (stack_stack_header *)_region;
			if ( stack_header->_next == UINT16_MAX ) {
				cout << "empty list: " << stack_header->_count << endl;
				return;
			}
			uint8_t *nxt = _region + stack_header->_next;
			stack_el_header *n_seh = (stack_el_header *)nxt;
			while ( n_seh->_next != UINT16_MAX ) {
				cout << "walk_stack: " << n_seh->_next << endl;
				n_seh = (stack_el_header *)(_region + n_seh->_next);
			}
			cout << "walk_stack(final): " << n_seh->_next << endl;
		}
};