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


template<class Entry, uint16_t const ExpectedMax = 100>
class SharedQueue_SRSW {    // single reader, single writer
	//
	public:

		SharedQueue_SRSW() {
			_beg = &_entries[0];
			_end = _beg + ExpectedMax;
			_r.store(_beg);
			_w.store(_beg);
		}
		virtual ~SharedQueue_SRSW() {
		}

	public:

		bool 		pop(Entry &entry) {
			//
			Entry *rr = nullptr;
			if ( emptiness(&rr) ) {
				memset(&entry,0,sizeof(Entry));
				return false;
			}

			entry = *rr++;  // can only get here if the read pointer does not meet the write pointer

			if ( rr == _end ) {		// read aprises the writer of its update
				_r.store(_beg,std::memory_order_release);
			} else {
				_r.store(rr,std::memory_order_release);
			}

			//
			return true;
		}


		bool		emptiness(Entry **rr) {   // read obtains the snapshot of the write location (saves for future comparison)
			auto rr0 = _r.load(std::memory_order_relaxed);
			if ( rr0 == _w_cached ) {  // current idea of cache is that it is maxed out
				_w_cached = _w.load(std::memory_order_acquire);   // did it move since we looked last?
				if ( rr0 == _w_cached ) { // no it didn't
					return true;
				}
			}
			*rr = rr0;
			return false;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		bool		push(Entry &entry) {
			Entry *wrt = nullptr;
			Entry *wrt_update;
			if ( full(&wrt,&wrt_update) ) {   // the update will be no less than one behind the writer
				return false;
			}
			*wrt = entry;  // the last write position receives the value...
			_w.store(wrt_update,std::memory_order_release);  // the write aprises the reader of the write position

			return true;
		}


		bool		full(Entry **wrt_ref,Entry **wrt_update) {
			//
			auto wrt = _w.load(std::memory_order_relaxed);
			auto next_w = (wrt + 1);
			if ( next_w == _end ) next_w = _beg;
			//
			auto rr = _r_cached;
			if ( rr < next_w ) {
				const auto max_span = ExpectedMax - 1;
				// initial state until the first wrap around
				// (either at opposite ends or trailing by one at first wrap around)
				if ( ((next_w - rr) == max_span) || ((rr == _beg) && (next_w == (_end-1)))  ) {
					rr = _r_cached = _r.load(std::memory_order_acquire);
					if ( ((next_w - rr) == max_span) || ((rr == _beg) && (next_w == (_end-1)))  ) {
						return true;
					}
				}
			} else {  // after wrap around
				if ( (rr - next_w) == 1 ) {  // full if it trails by one
					rr = _r_cached = _r.load(std::memory_order_acquire);
					if ( (rr - next_w) == 1 ) {
						return true;
					}
				}
			}
			//
			*wrt_ref = wrt;
			*wrt_update = next_w;
			return false;
		}


		bool 		empty(void) {
			return ( _r.load() == _w.load() );
		}


	public:

		Entry	_entries[ExpectedMax];
		Entry	*_beg;
		Entry	*_end;

		atomic<Entry *>		_r;
		Entry		 		*_r_cached;
		atomic<Entry *>		_w;
		Entry		 		*_w_cached;

};




#endif // _H_QUEUE_ENTRY_HOLDER_