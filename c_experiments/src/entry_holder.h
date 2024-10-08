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

// This queue shares in process... 


/**
 * The Shared Queue -- in process task queue being used between threads in a single process.
 * 
*/

template<class Entry, uint16_t const ExpectedMax = 100>
class SharedQueue_SRSW {    // single reader, single writer
	//
	public:

		SharedQueue_SRSW() {
			reset();
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

			entry = *rr++;  // (instead of memset) can only get here if the read pointer does not meet the write pointer

			if ( rr >= _end ) {		// read aprises the writer of its update
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
			if ( _r_cached == next_w ) {
				auto rr = _r.load(std::memory_order_acquire);
				_r_cached = rr;
				if ( _r_cached == next_w ) {
					return true;
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


		void		reset(void) {
			//
			_beg = &_entries[0];
			_end = _beg + ExpectedMax;
			_r_cached = _beg;
			_w_cached = _beg;
			_r.store(_beg);
			_w.store(_beg);
			//
			memset(_entries,0,sizeof(Entry)*ExpectedMax);
		}


		void		search(uint32_t key,uint32_t &value) {
			auto wrt = _w.load(std::memory_order_relaxed);
			auto rd = _r.load(std::memory_order_relaxed);
			value = UINT32_MAX;
			if ( wrt == _end ) wrt = _beg;
			while ( rd != wrt ) {
				if ( rd == _end ) rd = _beg;
				if ( compare_key(key,_beg + rd,value) ) {
					return;
				}
				rd++;
			}
		}

		bool		compare_key([[maybe_unused]] uint32_t key,[[maybe_unused]] Entry *el,[[maybe_unused]] uint32_t &value) {
			return false;
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