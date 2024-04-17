#ifndef _H_QUEUE_ENTRY_HOLDER_
#define _H_QUEUE_ENTRY_HOLDER_

#pragma once

#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>
#include <thread>
#include <ctime>
#include <atomic>
#include <thread>
#include <mutex>

#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <deque>


// USING
using namespace std;
// 


template<uint16_t const ExpectedMax = 100, class Entry>
class SharedQueue {
	//
	public:

		SharedQueue() {
			_r = 0;
			_w = 0;
		}
		virtual ~SharedQueue() {
		}

	public:

		bool 		pop(Entry &entry) {
			//
			uint16_t rr;
			if ( emptiness(rr) ) {
				memset(&entry,0,sizeof(Entry));
				return false;
			}

			entry = _entries[rr++];

			if ( rr == ExpectedMax ) {
				_r.store(0,std::memory_order_release);
			} else {
				_r.store(rr,std::memory_order_release);
			}
			//
			return true;
		}


		bool		emptiness(uint16_t &rr) {
			rr = _r.load(1,std::memory_order_relaxed);
			if (rr == _w_cached) {  // current idea of cache is that it is maxed out
				_w_cached = _w.load(std::memory_order_acquire);   // did it move since we looked last?
				if (rr == _w_cached) { // no it didn't
					return true;
				}
			}
			return false;
		}



		bool		push(Entry &entry) {
			uint16_t wrt;
			uint16_t wrt_update;
			if ( full(wrt,wrt_update) ) {
				return false;
			}
			_entries[wrt] = entry;
			_w.store(wrt_update,std::memory_order_release);
			return true;
		}


		bool full(uint16_t &wrt,uint16_t &wrt_update) {
			wrt = _w.load(std::memory_order_relaxed);
			auto next_w = (wrt + 1);
			if ( next_w == ExpectedMax ) next_w = 0;
			//
			auto rr = _r_cached;
			if ( rr < next_w ) {
				if ( ((next_w - rr) == ExpectedMax) || ((rr == 0) && (next_w == (ExpectedMax-1)))  ) {
					rr = _r_cached = _r.load(std::memory_order_acquire);
					if ( ((next_w - rr) == ExpectedMax) || ((rr == 0) && (next_w == (ExpectedMax-1)))  ) {
						return true;
					}
				}
			} else {
				if ( (rr-next_w) == 1 ) {
					rr = _r_cached = _r.load(std::memory_order_acquire);
					if ( (next_w-rr) == ExpectedMax ) {
						return true;
					}
				}
			}
			wrt_update = next_w;
			return false;
		}


		bool empty(void) {
			return ( _r.load() == _w.load() );
		}


	public:

		Entry _entries[ExpectedMax];
		atomic<uint16_t> _r;
		uint16_t		 _r_cached;
		atomic<uint16_t> _w;
		uint16_t		 _w_cached;

};




#endif // _H_QUEUE_ENTRY_HOLDER_