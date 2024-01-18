#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <iostream>

#include <deque>
#include <map>
#include <ctime>

#include <thread>
#include <atomic>

#include <chrono>
#include <iostream>

using namespace std;

// UpdateSource

// Applications should override UpdateSource
// Either the UpdateSource will prove to be useless or it will serve
// as the entry point for new values arriving from processors (cores)
// while the timer vector is being updated by one selected process.
//
class UpdateSource {

	public:

	UpdateSource(pair<uint32_t,uint32_t> *shared_queue,uint16_t expected_proc_max) 
									: _sorted_updates(shared_queue), _NProcs(expected_proc_max) {
		_min_value = UINT32_MAX;
		_min_index = UINT32_MAX;
		_max_value = 0;
		_max_index = 0;
	}

	public:

		inline pair<uint32_t,uint32_t> release_min() {
			pair<uint32_t,uint32_t> p = _sorted_updates[_min_index];
			_min_index = (_min_index + 1) % _NProcs;
			_min_value = _sorted_updates[_min_index].first;
			return p;
		}

		bool has_values() {
			return _min_value < UINT32_MAX;
		}

	public:
		uint32_t 	_min_value;
		uint32_t 	_min_index;

		uint32_t	_max_value;
		uint32_t	_max_index;
		//
	protected:
		pair<uint32_t,uint32_t> *_sorted_updates;
		uint16_t				_NProcs;
};



/*

Updating source has to be figured out. Atomicity is needed around 'has_values'.

* Code from node_shm.cc will be moved into classes. 

* There is one processes that will compress the timer buffer. 
* The 'put' method will call upon the time buffer when adding a new entry.
* The 'set' in index.js calls the 'put' method.  (currently 'set' calls shm.set)

* Maybe a separate update method since this will not hit the hash table per se.
    - but 'set with hash' type of method will search the hash table for the element offset.

* 'set with hash' and 'get' will adjust the timer of an element. Offload timer table update... 
    - if an element is already moved to the unsorted section (it is not moved again unless compression happens first)
* Evictions refer to the time table -- mass remove.

* is the timer table a circular buffer?  (No... this is not. The tradeoff is instantaneous evictions for 
all search and sorting references being masked and calculated.)
But, the buffer can have dense areas and sparse areas with the idea that the most active traffic will be in a dense area,
while the least active areas will remain dense to economize on space. Medium traffic areas can have some sparseness (holes)
which may slow its searches, but updates will move these to the new area which should be a packed short array.

The algorithm can no the lowest and highest hole numbers.  It can opportunistically compact the buffer 
in order to remove holes. 

* The rest is that we work on tiers...

*/
