
//
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <iostream>

#include <deque>
#include <map>

using namespace std;


// Experiment with atomics for completing hash table operations.


static_assert(sizeof(uint64_t) == sizeof(atomic<uint64_t>), 
    "atomic<T> isn't the same size as T");

static_assert(atomic<uint64_t>::is_always_lock_free,  // C++17
    "atomic<T> isn't lock-free, unusable on shared mem");

// 

// should be sorted from largest to smallest
//
inline pair<uint32_t,uint32_t> *bin_search_with_blackouts_increasing(uint32_t key,pair<uint32_t,uint32_t> *key_val,uint32_t N) {
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
		if ( key > (end-1)->first ) return nullptr;   // largest to smallest
		if ( key < beg->first ) return nullptr;   // largest to smallest
		//
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
					beg = mid_u;
					continue;
				}
			}
			if ( mid_l > beg ) {
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
			if ( ( mid_u < end ) && ( mid_l > beg ) && distance > 0 ) {
				if ( distance == 1 ) {
					return mid_l;
				} else {
					return mid_l + distance/2;
				}
			} else break;    // nothing found and no room to put it.
		}
		//
	}
	return nullptr;
}

inline pair<uint32_t,uint32_t> max_min(pair<uint32_t,uint32_t> *beg,pair<uint32_t,uint32_t> *end) {
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


inline void sort_small_mostly_increasing(pair<uint32_t,uint32_t> *beg,pair<uint32_t,uint32_t> *end) {
	pair<uint32_t,uint32_t> *start = beg;
	while ( beg < (end-1) ) {
		if ( beg->first > (beg+1)->first ) break;
		beg++;
	}
	if ( beg < end ) {
		sort(beg,end,[](pair<uint32_t,uint32_t> &a, pair<uint32_t,uint32_t> &b) {
			return a.first < b.first;
		});
		if ( (start < beg) && ((beg-1)->first > beg->first) ) {
			pair<uint32_t,uint32_t> *rscout = beg-1;
			while ( (rscout > start) && (rscout->first > beg->first) ) {
				rscout--;
			}
			pair<uint32_t,uint32_t> *fscout = beg+1;
			while ( (fscout < end) && (fscout->first < (beg-1)->first) ) {
				fscout++;
			}
			sort(rscout,fscout,[](pair<uint32_t,uint32_t> &a, pair<uint32_t,uint32_t> &b) {
				return a.first < b.first;
			});
		}
	}
}


class UpdateSource {
	public:

	UpdateSource(pair<uint32_t,uint32_t> *shared_queue,uint16_t expected_proc_max) 
									: _sorted_updates(shared_queue), _NProcs(expected_proc_max) {}

	public:

		inline pair<uint32_t,uint32_t> release_min() {
			pair<uint32_t,uint32_t> p = _sorted_updates[_min_index];
			_min_index = (_min_index + 1) % _NProcs;
			_min_value = _sorted_updates[_min_index].first;
			return p;
		}

	public:
		uint32_t 	_min_value;
		uint32_t 	_min_index;

		uint32_t	_max_value;
		uint32_t	_max_index;
		//
	private:
		pair<uint32_t,uint32_t> *_sorted_updates;
		uint16_t				_NProcs;
};



inline uint32_t merge_sort_with_blackouts_increasing(pair<uint32_t,uint32_t> *key_val,pair<uint32_t,uint32_t> *output,uint32_t N,uint32_t M,UpdateSource &us)  {
	//
	if ( N == 0 ) return UINT32_MAX;
	pair<uint32_t,uint32_t> *beg = key_val;
	pair<uint32_t,uint32_t> *end = key_val + N;
	pair<uint32_t,uint32_t> *new_vals = key_val + N;
	pair<uint32_t,uint32_t> *new_vals_end = key_val + N + M;
	//
	while ( ( beg < end) && (beg->first == UINT32_MAX) ) beg++;
	while ( ( end > beg ) && ((end-1)->first == UINT32_MAX)) end--;
	//
	sort_small_mostly_increasing(new_vals,new_vals_end);
	//
	if ( beg == end ) {  // just copy the new vals to the 
		memcpy(output,new_vals,M*sizeof(pair<uint32_t,uint32_t>));
		return M;
	} else {
		uint32_t late_arrivals = 0;
		//
		pair<uint32_t,uint32_t> *next = beg;
		//
		// three-way merge
		//
		while ( next < end ) {
			uint32_t val = next->first;
			while ( (val == UINT32_MAX) && (next < end) ) { // move ahead past the blackouts
				next++; val = next->first;
				N--;
			}
			if ( next < end ) {
				uint32_t val = next->first;
				if ( val > us._min_value ) {
					pair<uint32_t,uint32_t> p = us.release_min();  // might have to shift right to accommodate
					output->first = p.first;
					output->second = p.second;
					late_arrivals++;
				} else if ( (new_vals < new_vals_end) && (val > new_vals->first) ) {
					output->first = new_vals->first;
					output->second = new_vals->second;
					new_vals++;
				} else {
					output->first = next->first;
					output->second = next->second;
					next++;
				}
				output++;
			}
		}
		//
		while ( new_vals < new_vals_end ) {
			uint32_t val = new_vals->first;
			if ( val > us._min_value ) {
				pair<uint32_t,uint32_t> p = us.release_min();  // might have to shift right to accommodate
				output->first = p.first;
				output->second = p.second;
			} else {
				output->first = new_vals->first;
				output->second = new_vals->second;
				new_vals++;
			}
			output++;
		}
		//
		return M + N + late_arrivals;

	}

	return 0;
}



// makes a hole where the old entry is...
inline uint32_t entry_upate(uint32_t key,uint32_t key_update,pair<uint32_t,uint32_t> *key_val,uint32_t N,uint32_t M,uint32_t maybe_value = UINT32_MAX) {
	//
	pair<uint32_t,uint32_t> *p = bin_search_with_blackouts_increasing(key,key_val, N);
	if ( p == nullptr ) {
		p = key_val + N;
		pair<uint32_t,uint32_t> *end = p + M;
		while ( p < end ) {
			if ( p->first == key ) {
				p->first = key_update;
				if ( maybe_value != UINT32_MAX ) p->second = maybe_value;
				return M;
			}
		}
		if ( maybe_value != UINT32_MAX ) {
			p->first = key;
			p->second = maybe_value;
			return M + 1;
		}
	}
	//
	if ( p != nullptr ) {
		if ( p->first == key ) {
			uint32_t value = p->second;
			p->first = UINT32_MAX;
			p->second = UINT32_MAX;
			(key_val + N + M)->first = key_update;
			(key_val + N + M)->second = value;
			return M + 1;
		} else if ( p->first == UINT32_MAX ) {
			p->first = key_update;
			p->second = maybe_value;
			return M;
		}
	} else if ( maybe_value != UINT32_MAX ) {
		p = (key_val + N + M);
		p->first = key_update;
		p->second = maybe_value;
		return M + 1;
	}
	return 0;
}




// makes a hole where the old entry is...
inline uint32_t entry_remove(uint32_t key,pair<uint32_t,uint32_t> *key_val,uint32_t N,uint32_t M) {
	//
	pair<uint32_t,uint32_t> *p = bin_search_with_blackouts_increasing(key,key_val, N);
	if ( (p == nullptr) && (M > 0) ) {
		p = key_val + N;
		pair<uint32_t,uint32_t> *end = p + M;
		while ( p < end ) {
			if ( p->first == key ) {
				p->first = (end-1)->first;
				p->second = (end-1)->second;
				return M-1;
			}
		}
	}
	//
	if ( p != nullptr ) {
		if ( p->first == key ) {
			uint32_t value = p->second;
			p->first = UINT32_MAX;
			p->second = UINT32_MAX;
			return M;
		}
	}
	//
	return UINT32_MAX;
}



class KeyValueManager {

	public:

		KeyValueManager(pair<uint32_t,uint32_t> *primary_storage, pair<uint32_t,uint32_t> *secondary_storage,
							uint32_t count_size, pair<uint32_t,uint32_t> *shared_queue, uint16_t expected_proc_max) 
			: _proc_queue(shared_queue,expected_proc_max) {
			_key_val_A = _key_val = primary_storage;
			_key_val_B = _output = secondary_storage;
			_AB_switch = false;
			_MAX = count_size;
			_N = 0;
			_M = 0;
			_blackout_count = 0;
			_proc_count = 4;   // depends on processor and configuration
		}
	
	public:
		// 

		inline bool add_entry(uint32_t key,uint32_t value) {
			if ( _N + _M >= _MAX ) return false;
			manage_update(key,key,value);
			return true;
		}

		inline bool update_entry(uint32_t key,uint32_t old_key,uint32_t value) {
			if ( _N + _M >= _MAX ) return false;
			manage_update(old_key,key,value);
			return true;
		}

		inline bool remove_entry(uint32_t key) {
			uint32_t update_count = entry_remove(key,_key_val,_N,_M);
			if ( update_count < UINT32_MAX ) {
				if ( _M != update_count ) _M = update_count;
				else _blackout_count++;
				return true;
			}
			return false;  // no changes
		}

		inline void rectify_blackout_count(uint32_t tolerance) {
			if ( tolerance >= _blackout_count ) {
				_N = merge_sort_with_blackouts_increasing(_key_val,_output,_N,_M,_proc_queue);
				_M = 0;
				_blackout_count = 0;
				switch_buffers();
			}
		}


		void set_procs_participating(uint32_t P) {
			_proc_count = P;
		}

	protected:

		inline void manage_update(uint32_t key,uint32_t key_update,uint32_t maybe_value = UINT32_MAX) {
			_M = entry_upate(key,key_update,_key_val,_N,_M,maybe_value);
			if ( _proc_count <= _M ) {
				_N = merge_sort_with_blackouts_increasing(_key_val,_output,_N,_M,_proc_queue);
				_M = 0;
				_blackout_count = 0;
				switch_buffers();
			}
		}

		void switch_buffers() {
			if ( _AB_switch ) {
				_key_val = _key_val_A;
				_output = _key_val_B;
				_AB_switch = false;
			} else {
				_key_val = _key_val_B;
				_output = _key_val_A;
				_AB_switch = true;
			}
		}

	private:

		UpdateSource			_proc_queue;

		pair<uint32_t,uint32_t> *_key_val;
		pair<uint32_t,uint32_t> *_output;
		pair<uint32_t,uint32_t> *_key_val_A;
		pair<uint32_t,uint32_t> *_key_val_B;
		bool					_AB_switch;
		uint32_t				_MAX;
		uint32_t				_N;
		uint32_t				_M;
		uint32_t				_blackout_count;

		uint16_t				_proc_count;
};





// more like original
const uint64_t ui_53_1s = (0x0FFFFFFFFFFFFFFFF >> 11);
const double d_53_0s = (double)((uint64_t)1 << 53);

inline uint64_t rotl64 ( uint64_t x, int8_t r )
{
	return (x << r) | (x >> (64 - r));
}

template<uint64_t N>
void figure_some_probabilities(uint8_t ProcCount,double overlap_fraction,double subjective_reduction) {
	auto P = ProcCount;

	overlap_fraction = subjective_reduction*overlap_fraction;
	uint16_t OverLap = (overlap_fraction*N);

	cout << "# Procs: " << (int)P << " #Els: " << N << " # OVERLAP: " << OverLap << endl;
	cout << "overlap scalar: " << overlap_fraction << " v.s ProcCount " <<  ((double)(P)/(overlap_fraction*N)) << endl;
	//
	double order = OverLap + P*log2(P) + (OverLap + P)*log2(OverLap + P);
	cout << "O: " << order << endl;
}


void probabilities_likely_size_for_end_sorts(void) {
    //test_HD_separation();
    //gen_vector_set<2048*4,16,50>();
	for ( int i = 2; i <= 128; i *= 2 ) {
		for ( double sreduc = 0.125; sreduc < 0.75; sreduc += 0.125 ) {
			uint8_t Procs = i;
			cout << "PROCS: " << (int)Procs << " at reduction: " << sreduc << endl;
			double procs_to_els = (double)(Procs)/20000000.0;
			figure_some_probabilities<20000000L>(Procs,procs_to_els,sreduc);
			cout << ">>>>" << endl;
		}
		cout << "---------------------------" << endl;
	}
	//
	uint8_t Procs = 96;
	double sreduc = 20.0;
	cout << "PROCS: " << (int)Procs << " at reduction: " << sreduc << endl;
	double procs_to_els = (double)(Procs)/20000000.0;
	figure_some_probabilities<20000000L>(Procs,procs_to_els,sreduc);
	cout << ">>>>" << endl;
}



int main(int arcg, char **argv) {
	//
	uint32_t count_size = 200;
	pair<uint32_t,uint32_t> primary_storage[count_size];
	pair<uint32_t,uint32_t> secondary_storage[count_size];
	uint16_t expected_proc_max = 8;
	pair<uint32_t,uint32_t> shared_queue[expected_proc_max];

	KeyValueManager tester(primary_storage, secondary_storage, count_size, shared_queue,  expected_proc_max);
	//
    return(0);
}
