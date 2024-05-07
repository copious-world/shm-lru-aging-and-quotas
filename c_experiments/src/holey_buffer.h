
#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <iostream>

#include <deque>
#include <list>
#include <ctime>

#include <thread>
#include <atomic>

#include <chrono>
#include <iostream>

#include <atomic>

using namespace std;



#include "updating_source.h"


// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------


// max_min

static inline pair<uint32_t,uint32_t> 
max_min(pair<uint32_t,uint32_t> *beg,pair<uint32_t,uint32_t> *end) {
	pair<uint32_t,uint32_t> p;
	p.first = UINT32_MAX;
	p.second = 0;
	//
	while ( beg < end ) {
		uint32_t val = beg->first;
		if ( p.second < val ) {
			p.second = val;
		}
		if ( p.first > val ) {
			p.first = val;
		}
		beg++;
	}

	return p;
}



// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------


// b_search
//
static inline pair<uint32_t,uint32_t> *
b_search(uint32_t key,pair<uint32_t,uint32_t> *key_val,uint32_t N) {
	pair<uint32_t,uint32_t> *beg = key_val;
	pair<uint32_t,uint32_t> *end = key_val + N;
	if ( beg->first == key ) return beg;
	beg++; N--;
	if ( (end-1)->first == key ) return (end-1);
	end--; N--;
	//
	while ( beg < end ) {
		N = N >> 1;
		if ( N == 0 ) {
			while ( beg < end ) {
				if ( beg->first == key ) return beg;
				beg++;
			}
			break;
		}
		pair<uint32_t,uint32_t> *mid = beg + N;
		if ( mid >= end ) break;
		//
		if ( key == mid->first ) return mid;
		if ( key > mid->first ) beg = mid;
		else end = mid;
	}
	return nullptr;
}


// u32b_search
//
static inline uint32_t
u32b_search(uint32_t key,uint32_t *buckets,uint32_t N) {

	uint32_t *beg = buckets;
	uint32_t *end = buckets + N;
	if ( (beg[0] <= key) && (key < beg[1]) ) return 0;
	beg++; N--;
	//
	if ( (end-1)[0] <= key ) return N;
	end--; N--;
	//
	while ( beg < end ) {
		N = N >> 1;
		if ( N == 0 ) {
			while ( beg < end ) {
				if ( (beg[0] <= key) && (key < beg[1]) ) return (uint32_t)(beg - buckets);
				beg++;
			}
			break;
		}
		uint32_t *mid = beg + N;
		if ( mid >= end ) break;
		//
		if ( (mid[0] <= key) && (key < mid[1]) ) return (uint32_t)(mid - buckets);
		if ( key >= mid[1] ) beg = mid;
		else end = mid;
	}
	return UINT32_MAX;
}




// bin_search_with_blackouts_increasing
//
static inline pair<uint32_t,uint32_t> *
bin_search_with_blackouts_increasing(uint32_t key,pair<uint32_t,uint32_t> *key_val,uint32_t N) {
    //
	if ( N == 0 ) return nullptr;
	if ( key_val == nullptr ) return nullptr;
	else {
		pair<uint32_t,uint32_t> *beg = key_val;
		pair<uint32_t,uint32_t> *end = key_val + N;
		//
		while ( ( beg < end) && (beg->first == UINT32_MAX) ) beg++;
		while ( (end > beg ) && ((end-1)->first == UINT32_MAX)) end--;
		//
		if ( beg == end ) return nullptr;
		//
		if ( key > (end-1)->first ) return nullptr;	// smallest to largest
		if ( key < beg->first ) return nullptr;   	//
		//
		while ( (end > beg) && (N > 0) ) {
			if ( key == beg->first ) return beg;
			if ( key == (end-1)->first ) return (end-1);
			N = N >> 1;
			pair<uint32_t,uint32_t> *mid_u = beg + N; // smaller values are in upper
			pair<uint32_t,uint32_t> *mid_l = mid_u;
			while ( (mid_u->first == UINT32_MAX) && (mid_u < end) ) mid_u++;
			while ( (mid_l->first == UINT32_MAX) && (mid_l > beg) ) mid_l--;
			//
			if ( mid_u < end ) {  
				uint32_t mky = mid_u->first;
				if ( key == mky ) return mid_u;
				if ( key > mky ) {   // smaller values are in upper
					beg = mid_u + 1;
					continue;
				}
			}
			if ( mid_l >= beg ) {
				uint32_t mky = mid_l->first;
				if ( key == mky ) return mid_l;
				if ( key < mky ) {  // larger values are in lower
					end = mid_l;
					continue;
				}
			}
			// at this point the key is between the upper and lower parts 
			// but blackouts are stored in these areas, go ahead and use one
			uint64_t distance = ((uint64_t)mid_u - (uint64_t)mid_l)/(sizeof(uint64_t)*2);
			if ( ( mid_u < end ) && ( mid_l >= beg ) && (distance > 0) ) {
				if ( distance == 1 ) {
					return mid_l;
				} else {
					return mid_l + distance/2;
				}
			} break;    // nothing found and no room to put it.
		}
		//
	}

	return nullptr;
}




// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// KeyValueManager
// 
/**
 * KeyValueManager
 * 
 * The KeyValueManager keeps track of two lists which are stored in the same region and which are often merged during 
 * their usage. The bottom of memory list is a sorted list of values, which may acquire holes over time. The top of memory list
 * grows by appendation and is expected to be nearly sorted as a result of key incrementation over time. In most cases, the key 
 * will be a timestamp. And, in the case of the time stamp, the key will almost surely be sorted, except that since more than one process 
 * may enter a key, there will be races that put the keys out of sort locally for a small bucket (basket) of values.
 * 
 * M measures number of elements in the lower memory list. And, N measuret the number of entries beyond it. 
 * When a new element is added, it may grow M if N is zero (N=0). If it N=0, it can be assumed that the lower list have been 
 * resorted and that no race condition has violated the order of addition. At some point, prior to a sorting, merging, and compression 
 * of the lists, the M will become fixed and new additions will be added to the end. 
 * 
 * Conditions that fix M can be entries set out of order by race conditions, by an update or by a deletion of an older entry.
 * 
 * 
 * 
*/

class KeyValueManager {

	public:

		KeyValueManager(pair<uint32_t,uint32_t> *primary_storage,
							uint32_t count_size, pair<uint32_t,uint32_t> *shared_queue, uint16_t expected_proc_max) 
			: _proc_queue(shared_queue,expected_proc_max) {
			_key_val = primary_storage;
			_end_key_val = _key_val + expected_proc_max;
			_MAX = count_size;
			_N = 0;
			_M = 0;
			_blackout_count = 0;
			_proc_count = 4;   // depends on processor and configuration
			//
			_nouveau_max = 0;
			_nouveau_min = UINT32_MAX;
			//
			_init_min_max_holes();
			_regions_option = false;
			_regions = nullptr;				// for future use
		}

	public:
		// 


		/**
		 * add_entry -- application facing method. 
		 * 
		 * Call `entry_add`, which returns an update of `_M`, the number of elements in the lower memory list.
		 * `_M` does not necessarily update, but should reflect the size of the surely sorted elements. 
		 * The upper and lower bounds of all the entries are updated as well. 
		 * 
		*/

		inline bool add_entry(uint32_t key,uint32_t value) {
			if ( _N + _M >= _MAX ) return false;
			uint32_t MCheck = entry_add(key,value,_key_val,_N,_M);
			if ( MCheck == UINT32_MAX ) {
				return false;
			}
			_nouveau_max = max(key,_nouveau_max);
			_nouveau_min = min(key,_nouveau_min);
			_M = MCheck;
			return true;
		}


		/**
		 * update_entry  -- application facing method. 
		 * 
		 * updates an entry by changing the key (timestamp) and moving it from the sorted region the 
		 * upper memory list. The two region sizes need enough space for a new element, since the 
		 * update will create a hole at the position where the entry presided and add the upate to the end 
		 * with a new increment (timestamp).
		 * 
		 * 
		*/
		inline bool update_entry(uint32_t key,uint32_t old_key,uint32_t maybe_value = UINT32_MAX) {
			if ( _N + _M >= (_MAX-1) ) return false;
			manage_update(old_key,key,maybe_value);
			return true;
		}


		/**
		 * remove_entry -- application facing method. 
		 * 
		 * 
		*/
		inline bool remove_entry(uint32_t key) {
			if ( _N ==0 && _M == 0 ) return false;
			if ( (_blackout_count == 0) && (_M > 0) ) {
				_N += _M;
				_M = 0;
			} else {
				pair<uint32_t,uint32_t> new_hole_offset{UINT32_MAX,UINT32_MAX};
				if ( key >= _max_hole_offset.first ) {
					pair<uint32_t,uint32_t> *redbuf = _key_val + _max_hole_offset.second;  // redbuf === reduced buffer
					uint32_t redN = (_N - _max_hole_offset.second) + 1;  // adding in the hole at the end to avoid the end copy...
					uint32_t update_count = entry_remove(key,redbuf,redN,_M,false,new_hole_offset);
					_N = redN + _max_hole_offset.second;
					if ( update_count < UINT32_MAX ) {
						if ( new_hole_offset.first != UINT32_MAX ) {
							new_hole_offset.first += _max_hole_offset.second;
							manage_new_hole(new_hole_offset);
						}
						if ( _M != update_count ) _M = update_count;
						else _blackout_count++;
						return true;
					}
					return false;  // no changes
				} else if ( _regions_option ) {
					//
					pair<uint32_t,uint32_t> new_hole_offset{UINT32_MAX,UINT32_MAX};
					pair<uint32_t,uint32_t> region_l;
					pair<uint32_t,uint32_t> region_u;
					//
					if ( find_region(key,region_l,region_u) ) {
						pair<uint32_t,uint32_t> *redbuf = _key_val + region_l.second;  // redbuf === reduced buffer
						uint32_t redN = region_u.second - region_l.second;
						uint32_t update_count = entry_remove(key,redbuf,redN,_M,false,new_hole_offset);
						_N = redN + _max_hole_offset.second;
						if ( update_count < UINT32_MAX ) {
							if ( new_hole_offset.first != UINT32_MAX ) {
								new_hole_offset.first += _max_hole_offset.second;
								manage_new_hole(new_hole_offset);
							}
							if ( _M != update_count ) _M = update_count;
							else _blackout_count++;
							return true;
						}
						return false;
					}
				}
				uint32_t update_count = entry_remove(key,_key_val,_N,_M,(_blackout_count != 0),new_hole_offset);
				if ( update_count < UINT32_MAX ) {
					if ( new_hole_offset.first != UINT32_MAX ) {
						manage_new_hole(new_hole_offset);
					}
					if ( _M != update_count ) _M = update_count;
					else _blackout_count++;
					return true;
				}
			}
			return false;  // no changes
		}


		/**
		 * rectify_blackout_count
		*/
		void rectify_blackout_count(uint32_t tolerance) {
			if ( tolerance >= _blackout_count ) {
				_N = merge_sort_with_blackouts_increasing(_key_val,_N,_M,_nouveau_min,_proc_queue);
			}
		}

		/**
		 * set_procs_participating
		*/
		void set_procs_participating(uint32_t P) {
			_proc_count = P;
		}

		//
		/**
		 * displace_lowest_value_threshold
		 * 
		*/
		void displace_lowest_value_threshold(list<uint32_t> &deposit, uint32_t min_max, uint32_t max_count) {
			//
			pair<uint32_t,uint32_t> *p = _key_val;
			pair<uint32_t,uint32_t> *end = _key_val + max_count;
			pair<uint32_t,uint32_t> *copy = end + 1;
			pair<uint32_t,uint32_t> *eo_everything = p + _N + _M;
			//
			while ( (p < end) && (p->first < min_max) ) {
				if ( p->first != UINT32_MAX ) {
					deposit.push_back(p->second);
					if ( copy < eo_everything ) {
						p->first = copy->first;
						p->second = copy->second;
						copy++;
						_N--;
					}
				}
				p++;
			}
			while ( copy < eo_everything ) {
				p->first = copy->first;
				p->second = copy->second;
				copy++; p++;
			}
			//
			rectify_blackout_count(UINT32_MAX);
		}


		/**
		 * least_time_key
		*/
		uint32_t least_time_key() {
			pair<uint32_t,uint32_t> *p = _key_val;
			pair<uint32_t,uint32_t> *eo_everything = p + _N + _M;

			while ( (p->first == UINT32_MAX) && (p < eo_everything) ) { p++; }

			return p->first;
		}


	//protected:
	public:

		void manage_new_hole(pair<uint32_t,uint32_t> &new_hole_offset) {
			if ( new_hole_offset.first > _max_hole_offset.first ) {
				_max_hole_offset.first = new_hole_offset.first;
			}
			if ( new_hole_offset.first < _min_hole_offset.first ) {
				_min_hole_offset.first = new_hole_offset.first;
			}
		}

		void _init_min_max_holes() {
			_min_hole_offset.first = UINT32_MAX;
			_min_hole_offset.second = UINT32_MAX;
			//
			_max_hole_offset.first = 0;
			_max_hole_offset.second = UINT32_MAX;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * manage_update
		*/
		void manage_update(uint32_t key,uint32_t key_update,uint32_t maybe_value = UINT32_MAX) {
			pair<uint32_t,uint32_t> new_hole_offset{UINT32_MAX,UINT32_MAX};
			uint32_t maybe_M = UINT32_MAX;
			//
			if ( key >= _max_hole_offset.first ) {
				pair<uint32_t,uint32_t> *redbuf = _key_val + _max_hole_offset.second;  // redbuf === reduced buffer
				uint32_t redN = (_N - _max_hole_offset.second);
				maybe_M = entry_key_upate(key,key_update,redbuf,redN,_M,new_hole_offset,maybe_value,false);
				if ( maybe_M != UINT32_MAX ) {
					_N = redN + _max_hole_offset.second;
				}
			} else if ( _regions_option ) {
				pair<uint32_t,uint32_t> region_l;
				pair<uint32_t,uint32_t> region_u;
				//
				if ( find_region(key,region_l,region_u) ) {
					pair<uint32_t,uint32_t> *redbuf = _key_val + region_l.second;  // redbuf === reduced buffer
					uint32_t redN = (region_u.second - region_l.second) + 1; // add in the hole to avoid the end copy
					maybe_M = entry_key_upate(key,key_update,redbuf,redN,_M,new_hole_offset,maybe_value,false);
					if ( maybe_M != UINT32_MAX ) {
						_N = redN + _max_hole_offset.second;
					}
				} else {
					maybe_M = entry_key_upate(key,key_update,_key_val,_N,_M,new_hole_offset,maybe_value,(_blackout_count != 0));	
				}
			} else {
				maybe_M = entry_key_upate(key,key_update,_key_val,_N,_M,new_hole_offset,maybe_value,(_blackout_count != 0));	
			}
			//
			if ( maybe_M == UINT32_MAX ) return;
			_M = maybe_M;
			//
			_nouveau_max = max(key_update,_nouveau_max);
			_nouveau_min = min(key_update,_nouveau_min);
			//
			if ( _proc_count <= _M ) {  // Try to keep this last sort very small, within the number of procs sharing this buffer.
				_N = merge_sort_with_blackouts_increasing(_key_val,_N,_M,_nouveau_min,_proc_queue);
			}
			if ( new_hole_offset.first != UINT32_MAX ) {
				manage_new_hole(new_hole_offset);
			}
		}


	// ----
		/**
		 * sort_small_mostly_increasing
		*/
		void sort_small_mostly_increasing(pair<uint32_t,uint32_t> *beg,pair<uint32_t,uint32_t> *end) {
			pair<uint32_t,uint32_t> *start = beg;
			while ( start < (end-1) ) {
				if ( start->first >= (start+1)->first ) break;
				start++;
			}
			if ( start < end ) {
				for ( int i = 0; (i < 3) && ( start > beg ); i++ ) start--;
				//
				pair<uint32_t,uint32_t> *min_stop = start+1; 
				pair<uint32_t,uint32_t> *stop = end; 
				while ( --stop > min_stop ) {
					if ( stop->first < (stop-1)->first ) break;
				}
				for ( int i = 0; (i < 4) && ( stop < end ); i++ ) stop++;
				//

				sort(start,stop,[](pair<uint32_t,uint32_t> &a, pair<uint32_t,uint32_t> &b) {
					return a.first < b.first;
				});

				if ( ((start >= beg) && ((start-1)->first > start->first))
						|| ((stop < end) && ((stop-1)->first > stop->first)) ) { // sorted the section, but it had problems globally
					// what is the least position the smallest element comes before?
					pair<uint32_t,uint32_t> *rscout = start-1;  // reverse scout
					auto start_key = start->first;
					while ( (rscout > beg) && (rscout->first > start_key) ) {
						rscout--;
					}
					// 
					pair<uint32_t,uint32_t> *fscout = stop-1;
					auto stop_key = fscout->first;
					fscout = stop;
					while ( (fscout < end) && (fscout->first < stop_key) ) {
						fscout++;
					}
					if ( fscout < end ) fscout++;
					sort(rscout,fscout+1,[](pair<uint32_t,uint32_t> &a, pair<uint32_t,uint32_t> &b) {
						return a.first < b.first;
					});
				}

			}
		}


		inline uint32_t _merge_sort_with_blackouts_increasing(pair<uint32_t,uint32_t> *key_val,uint32_t N,uint32_t M,uint32_t new_min,UpdateSource &us)  {
			//
			if ( N == 0 ) return UINT32_MAX;
			pair<uint32_t,uint32_t> *output = key_val;
			//
			pair<uint32_t,uint32_t> *beg = key_val;
			pair<uint32_t,uint32_t> *end = key_val + N;
			pair<uint32_t,uint32_t> *new_vals = key_val + N;
			pair<uint32_t,uint32_t> *new_vals_end = key_val + N + M;
			//
			while ( ( beg < end) && (beg->first == UINT32_MAX) ) beg++;
			while ( ( end > beg ) && ((end-1)->first == UINT32_MAX)) end--;
			//
			//
			if ( beg == end ) {  // just copy the new vals to the 
				sort_small_mostly_increasing(new_vals,new_vals_end);
				memcpy((void *)output,(void *)new_vals,M*sizeof(pair<uint32_t,uint32_t>));
				return M;
			} else {
				//
				if ( new_min < (end-1)->first ) {
					while ( (new_min < (end-1)->first) && ( end > beg ) ) {
						if ( (end-1)->first == UINT32_MAX ) {
							(end-1)->first = (new_vals_end - 1)->first;
							(end-1)->second = (new_vals_end - 1)->second;
							new_vals_end--;
						}
						end--;
					}
					sort_small_mostly_increasing(end,new_vals_end);
					// if there are any holes that remained, they will be at the end.
					while ( (new_vals_end-1)->first == UINT32_MAX ) new_vals_end--;
					if ( end == beg ) {
						// the whole thing got sorted and compressed. But, this should be unlikely with large numbers 
						// of entries. All the same, if this occurs, return with the values.
						return (uint32_t)(new_vals_end - beg);
					}
					//
				} else {
					sort_small_mostly_increasing(new_vals,new_vals_end);
				}
				// now the two regions should be contiguous and in sorted order, 
				// but there may still be holes below the sort
				//
				pair<uint32_t,uint32_t> *next = beg;
				//
				// three-way merge
				//
				while ( next < end ) {
					uint32_t val = next->first;
					// skip wholes
					while ( (val == UINT32_MAX) && (++next < end) ) { // move ahead past the blackouts
						val = next->first;
					}
					if ( next < end ) { // otherwise take care of the next value (just one)
						if ( next > output ) {
							output->first = next->first;
							output->second = next->second;
						}
						next++;
					}
					output++;
				}

				uint32_t late_key_min = UINT32_MAX;
				next = output;
				while ( us.has_values() ) {
					pair<uint32_t,uint32_t> p = us.release_min();
					if ( p.first < late_key_min ) late_key_min = p.first;
					next->first = p.first;
					next->second = p.second;
					next++;						// assuming this will not run passed the end of buffer due to restrictions on has_values
				}

				if ( late_key_min < (output-1)->first ) {
					while ( (late_key_min < (output-1)->first) && (output > key_val) ) {
						output--;
					}
					sort(output,next,[](pair<uint32_t,uint32_t> &a, pair<uint32_t,uint32_t> &b) {
						return a.first < b.first;
					});
					output = next;
				}

				//
				return (uint32_t)(output - key_val);
			}

			return 0;
		}


		/**
		 * merge_sort_with_blackouts_increasing
		*/

		uint32_t merge_sort_with_blackouts_increasing(pair<uint32_t,uint32_t> *key_val,uint32_t N,uint32_t M,uint32_t new_min,UpdateSource &us)  {
			//
			uint32_t result = _merge_sort_with_blackouts_increasing(key_val, N, M, new_min, us);
			_M = 0;
			_nouveau_max = 0;
			_nouveau_min = UINT32_MAX;
			_blackout_count = 0;
			_init_min_max_holes();
			//
			return result;
		}


		/**
		 * entry_add
		*/
		inline uint32_t entry_add(uint32_t key,uint32_t value,pair<uint32_t,uint32_t> *key_val,uint32_t N,uint32_t M) {
			if ( N == 0 ) {   // append
				pair<uint32_t,uint32_t> *p = key_val + M;
				p->first = key;
				p->second = value;
				return (M+1);
			} else {
				pair<uint32_t,uint32_t> *check = key_val + N - 1;  // look at the last stored element
				if ( key < check->first ) {  // require the new elements to have a greater time stamp than the fixed lower region largest stamp.
					return UINT32_MAX;
				}
				pair<uint32_t,uint32_t> *p = key_val + N + M;  // end of everything (M lower and N upper)
				p->first = key;			// set key and value
				p->second = value;
				return (M+1);  // one more
			}
		}


		// makes a hole where the old entry is...
		/**
		 * entry_key_upate
		 * 
		 * binary search to find the item that will be updated...
		*/
		inline uint32_t entry_key_upate(uint32_t key,uint32_t key_update,pair<uint32_t,uint32_t> *key_val,uint32_t &N,uint32_t M,pair<uint32_t,uint32_t> &new_hole_offset,uint32_t maybe_value = UINT32_MAX,bool has_holes = true) {
			if ( key_update < key ) return UINT32_MAX; // it is assumed the keys will arrive in monotonic increasing order
			//
			pair<uint32_t,uint32_t> *p = has_holes ? bin_search_with_blackouts_increasing(key,key_val, N) : 
											b_search(key,key_val,N);
			//
			if ( p == nullptr ) {	// not found in the sorted older end (with possible holes)
				p = key_val + N;
				pair<uint32_t,uint32_t> *end = p + M;		// so look for it in the new addition, unlikely sorted.
				while ( p < end ) {
					if ( p->first == key ) {
						p->first = key_update; // the position of the unsorted element will not change
						if ( maybe_value != UINT32_MAX ) p->second = maybe_value;     // changing the value?
						return M;
					}
					p++;
				}
				// -- this would be an implicit add with update... not going that route
				// if ( maybe_value != UINT32_MAX ) {  // add a new value and key... 
				// 	p->first = key;
				// 	p->second = maybe_value;
				// 	return M + 1;
				// }
				return UINT32_MAX;
			}
			//
			if ( p != nullptr ) {		// this was found in the older region...
				pair<uint32_t,uint32_t> *end = (key_val + N);
				if ( (end-1) == p ) {
					p->first = key_update;	// new key 
					p->second = ( maybe_value == UINT32_MAX ) ? p->second : maybe_value;
					return M;
				} else {
					new_hole_offset.first = p->first;
					p->first = UINT32_MAX;
					(end + M)->first = key_update;	// new key 
					(end + M)->second = ( maybe_value == UINT32_MAX ) ? p->second : maybe_value;
					p->second = UINT32_MAX;  // clear out the old one
					new_hole_offset.second = (p - key_val);
					return M + 1;
				}
			}
			// -- this would be an implicit add with update... not going that route
			//	else if ( maybe_value != UINT32_MAX ) {
			// 	p = (key_val + N + M);
			// 	p->first = key_update;
			// 	p->second = maybe_value;
			// 	return M + 1;
			// }
			return UINT32_MAX;
		}


		// entry_remove
		// makes a hole where the old entry is...
		//	remove an element by making a hole.
		//
		inline uint32_t entry_remove(uint32_t key,pair<uint32_t,uint32_t> *key_val,uint32_t &N,uint32_t M,bool has_holes,pair<uint32_t,uint32_t> &new_hole_offset) {
			if ( N == 0 && M == 0 ) return UINT32_MAX;
			//
			pair<uint32_t,uint32_t> *p = has_holes ? bin_search_with_blackouts_increasing(key,key_val, N) : 
											b_search(key,key_val,N);

			if ( (p == nullptr) && (M > 0) ) {  // If it was not found in the lower buffer, then linearly scan the small upper buffer.
				p = key_val + N;
				pair<uint32_t,uint32_t> *end = p + M;
				while ( p < end ) {
					if ( p->first == key ) {		// going on the assumption that the 'late' added elements are not sorted, generally.
						p->first = (end-1)->first;  // shorten the upper buffer copy the previously appended element into the element leaving.
						p->second = (end-1)->second;
						return M-1;  // the list got shorter by one
					}
				}
			}
			//
			if ( p != nullptr ) {  // a position, p, has been located in the lower buffer
				pair<uint32_t,uint32_t> *end = (key_val + N);
				if ( p == (end - 1) ) {  // handle this end case that keeps a hole out of the last position.
					if ( M > 0 ) {  // there are elements that have been appended
						pair<uint32_t,uint32_t> *q = (end + M - 1);
						p->first = q->first;    // shorten the top by moving it to replace the end of the lower region.
						p->second = q->second;
						N--; 					// make the lower region smaller (the upper now stars at the old lower end of list)
						return M;				//  M has not decreased in size...  
					} else {
						N--;		// no elements were appended ... just shrink the lower bufer
						return 0;
					}
				}
				if ( p->first == key ) { 	// this is a double check... p < (end - 1)
					p->first = UINT32_MAX;			// make a hole where the element was.
					p->second = UINT32_MAX;
					new_hole_offset.first = key;	// update the keeper of the list of holes...
					new_hole_offset.second = (p - key_val);
					return M;
				}
			}
			//
			return UINT32_MAX;
		}


		bool find_region([[maybe_unused]] uint32_t key, [[maybe_unused]] pair<uint32_t,uint32_t> &lb_result, [[maybe_unused]] pair<uint32_t,uint32_t> &ub_result) {
			return false;
		}

	public:

		UpdateSource			_proc_queue;
		//
		pair<uint32_t,uint32_t> *_key_val;
		pair<uint32_t,uint32_t> *_end_key_val;
		//
		uint32_t				_MAX;
		uint32_t				_N;
		uint32_t				_M;
		uint32_t				_blackout_count;
		//
		uint32_t				_nouveau_max;
		uint32_t				_nouveau_min;
		//
		pair<uint32_t,uint32_t> _min_hole_offset;
		pair<uint32_t,uint32_t> _max_hole_offset;
		//

		//
		uint16_t				_proc_count;
		bool					_regions_option;
		pair<uint32_t,uint32_t> *_regions;
};






class SpinlockNonReaders {
public:
  SpinlockNonReaders(): flag{ATOMIC_FLAG_INIT} {}

  virtual ~SpinlockNonReaders(void) { unlock(); }

  void lock(bool reader){
    while( flag.test_and_set() && !reader ) __libcpp_thread_yield();
  }

  void unlock(){
    flag.clear();
  }

 private:
   std::atomic_flag flag;

};


class SpinlockWriters {
public:
  SpinlockWriters(): flag{ATOMIC_FLAG_INIT} {}

  virtual ~SpinlockWriters(void) { unlock(); }

  void lock(void){
    while( flag.test_and_set() ) __libcpp_thread_yield();
  }

  void unlock(){
    flag.clear();
  }

 private:
   std::atomic_flag flag;

};



class Shared_KeyValueManager : public KeyValueManager {

	public:

		Shared_KeyValueManager(pair<uint32_t,uint32_t> *primary_storage,
									uint32_t count_size, pair<uint32_t,uint32_t> *shared_queue, uint16_t expected_proc_max)
											: KeyValueManager(primary_storage,count_size,shared_queue,expected_proc_max)
								 {
		}

		virtual ~Shared_KeyValueManager() {
		}



	protected:

		void enter_shared_mode_ops() {
			_N = _share_N->load();
			_M = _share_M->load();
			_blackout_count = _share_blackout_count->load();
			_nouveau_max = _share_nouveau_max->load();
			_nouveau_min = _share_nouveau_min->load();
			_min_hole_offset.first = _share_min_hole_offset_first->load();
			_min_hole_offset.second = _share_min_hole_offset_second->load();
			_max_hole_offset.first = _share_max_hole_offset_first->load();
			_max_hole_offset.second = _share_max_hole_offset_first->load();
		}

		void exit_shared_mode_ops() {
			_share_N->store(_N);
			_share_M->store(_M);
			_share_blackout_count->store(_blackout_count);
			_share_nouveau_max->store(_nouveau_max);
			_share_nouveau_min->store(_nouveau_min);
			//
			_share_min_hole_offset_first->store(_min_hole_offset.first);
			_share_min_hole_offset_second->store(_min_hole_offset.second);
			_share_max_hole_offset_first->store(_max_hole_offset.first);
			_share_max_hole_offset_first->store(_max_hole_offset.second);
		}



		/**
		 * lock_position --
		 * The offset value being stored should never reach UINT32_MAX.
		 * By design the number of elements being stored times size is less than 4 billion.
		*/
		bool lock_position(pair<uint32_t,uint32_t> *p) {
			auto *p_val = (atomic<uint32_t> *)(&(p->second));
			auto pval = p_val->exchange(UINT32_MAX);
			if ( pval == UINT32_MAX ) {
				return false;
			}
			return true;
		}

		/**
		 * unlock_position --
		 * 
		*/
		void unlock_position(pair<uint32_t,uint32_t> *p,uint32_t value) {
			auto *p_val = (atomic<uint32_t> *)(&(p->second));
			p_val->store(value,std::memory_order_release);
		}


		/**
		 * entry_add
		 * 
		 * Allow some cooperation among readers of the data structure (also, these are writers of data).
		 * But, other operations which alter the sorting of the buffers, can lock. Locking may be further reduced 
		 * at some later date. For now, given some time constraints a larger region of operation can be locked.
		 * So, update and remove will be given exclusive access to the data structure. Readers (those adding) will be held out 
		 * while update and remove operate. Threads adding will lock out the other operations except for other adders 
		 * and some no-lock style of buffer access will be allowed. 
		 * 
		*/
		inline uint32_t entry_add(uint32_t key,uint32_t value,pair<uint32_t,uint32_t> *key_val,uint32_t N,uint32_t M) {
			if ( N == 0 ) {   // append
				pair<uint32_t,uint32_t> *p = key_val + M;
				uint32_t r = 1;
				while ( !(lock_position(p)) ) {
					r++;
					p++;
					if ( p >= _end_key_val ) {
						return UINT32_MAX;
					}
				}
				p->first = key;
				unlock_position(p,value);
				return (M+r);
			} else {
				pair<uint32_t,uint32_t> *check = key_val + N - 1;  // look at the last stored element
				if ( key < check->first ) {  // require the new elements to have a greater time stamp than the fixed lower region largest stamp.
					return UINT32_MAX;
				}
				pair<uint32_t,uint32_t> *p = key_val + N + M;  // end of everything (M lower and N upper)
				uint32_t r = 1;
				while ( !(lock_position(p)) ) {
					r++;
					p++;
					if ( p >= _end_key_val ) {
						return UINT32_MAX;
					}
				}
				p->first = key;			// set key and value
				unlock_position(p,value);
				return (M+r);  // one more
			}
		}


	public:

		/**
		 * add_entries - add a number of entries at once...
		*/
		inline bool add_entries(uint32_t *lru_element_offsets,uint32_t *entry_times,uint32_t ready_msg_count) {
			//
			_writer_lock.lock();
			_reader_lock.lock(true);

			_N = _share_N->load();
			_M = _share_M->load();
			// 
			bool any_ok = false;
			for ( uint32_t i = 0; i < ready_msg_count; i++ ) {
				uint32_t value = lru_element_offsets[i];
				uint32_t key = entry_times[i];
				any_ok |= this->add_entry(key, value);
			}
			//
			if ( any_ok ) {
				_share_N->store(_N);
				_share_N->store(_M);
				_share_nouveau_max->store(_nouveau_max);
				_share_nouveau_min->store(_nouveau_min);
			}
			_reader_lock.unlock();
			_writer_lock.unlock();
			//
			return any_ok;
		}

		/**
		 * update_entry
		 * 
		*/
		inline bool update_entry(uint32_t key,uint32_t old_key,uint32_t maybe_value = UINT32_MAX) {
			//
			_reader_lock.lock(false);
			_writer_lock.lock();
			//
			enter_shared_mode_ops();

			if ( _N + _M >= (_MAX-1) ) {
				_writer_lock.unlock();
				_reader_lock.unlock();
				return false;
			}

			manage_update(old_key,key,maybe_value);

			exit_shared_mode_ops();

			_writer_lock.unlock();
			_reader_lock.unlock();
			//
			return true;
		}



		inline bool update_entries(uint32_t *old_times,uint32_t *new_times,uint8_t count_updates) {
			_reader_lock.lock(false);
			_writer_lock.lock();
			//
			enter_shared_mode_ops();

			for ( uint8_t i = 0; i < count_updates; i++ ) {
				manage_update(old_times[i],new_times[i]);  // retain the current value...
			}
			
			exit_shared_mode_ops();

			_writer_lock.unlock();
			_reader_lock.unlock();
			//
			return true;
		}


		/**
		 * remove_entry -- application facing method. 
		 * 
		 * 
		*/
		inline bool remove_entry(uint32_t key) {
			//
			_reader_lock.lock(false);
			_writer_lock.lock();
			//
			enter_shared_mode_ops();
			//
			KeyValueManager *vkm = (KeyValueManager *)this;
			bool status = vkm->remove_entry(key);

			exit_shared_mode_ops();

			_writer_lock.unlock();
			_reader_lock.unlock();

			return status;
		}



	public:

		atomic<uint32_t>		*_share_N;
		atomic<uint32_t>		*_share_M;
		atomic<uint32_t>		*_share_blackout_count;
		atomic<uint32_t>		*_share_nouveau_max;
		atomic<uint32_t>		*_share_nouveau_min;
		//
		atomic<uint32_t>		*_share_min_hole_offset_first;
		atomic<uint32_t>		*_share_min_hole_offset_second;
		atomic<uint32_t>		*_share_max_hole_offset_first;
		atomic<uint32_t>		*_share_max_hole_offset_second;
		//
		SpinlockNonReaders		_reader_lock;
		SpinlockWriters			_writer_lock;
		//
};


