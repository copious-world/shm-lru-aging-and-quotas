
//
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
using namespace std::chrono;


// Experiment with atomics for completing hash table operations.


static_assert(sizeof(uint64_t) == sizeof(atomic<uint64_t>), 
    "atomic<T> isn't the same size as T");

static_assert(atomic<uint64_t>::is_always_lock_free,  // C++17
    "atomic<T> isn't lock-free, unusable on shared mem");

// 


// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------


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

// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------




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






#include <bitset>
#include <random>

template<uint32_t N>
unsigned int hamming(bitset<N> &a,bitset<N> &b) {
    bitset<N> diff = a ^ b;
    return diff.count();
}

random_device rdv;  // a seed source for the random number engine
mt19937 gen_v(rdv()); // mersenne_twister_engine seeded with rd()



// ---- ---- ---- ---- ----
//
template<const uint32_t MAX_ENTRIES>
void print_stored(pair<uint32_t,uint32_t> *primary_storage,uint32_t print_max = INT32_MAX) {
	//
	auto N = min(MAX_ENTRIES,print_max);
	for ( uint32_t i = 0; i < N; i++ ) {
		cout << primary_storage[i].first << "\t" << primary_storage[i].second << endl;
	}
	//
}




// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------


pair<uint32_t,uint32_t> *
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



uint32_t
u32b_search(uint32_t key,uint32_t *buckets,uint32_t N) {

	uint32_t *beg = buckets;
	uint32_t *end = buckets + N;
	if ( (beg[0] <= key) && (key < beg[1]) ) return 0;
	beg++; N--;
	//
	if ( (end-1)[0] <= key ) return N-1;
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



pair<uint32_t,uint32_t> *buffer_seek(uint32_t key,pair<uint32_t,uint32_t> *key_val,uint32_t N) {
	for ( uint32_t i = 0;  i < N; i++ ) {
		pair<uint32_t,uint32_t> *p = key_val + i;
		if ( p->first == key ) return p;
	}
	return nullptr;
}


inline pair<uint32_t,uint32_t>  * return_point(int i) {
	cout << "RT: " << i << endl;
	return nullptr;
}

//
pair<uint32_t,uint32_t> *bin_search_with_blackouts_increasing(uint32_t key,pair<uint32_t,uint32_t> *key_val,uint32_t N) {
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
	//cout << beg->first << "," << (beg+1)->first << "," << (beg+2)->first << " (" << (end - beg) << ") "  << N << endl;
	}

	return nullptr;
}


//
// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
/*
// a test after the first sort in 
auto test = start;
auto base = start->first;
uint16_t test_i = 0;
while ( test++ < (stop+10) ) {
	cout << ++test_i << ">> " << (test->first - base) << " @: " << (test->second) << endl;
}

// cout << "start: " << (start-1)->first << " < " << start->first << " is " << ((start-1)->first < start->first) << endl;
// cout << "stop: "  << (stop-1)->first << " < " << (stop)->first << " is " << ((stop-1)->first < (stop)->first) << endl;
// cout << "true/false rscout < fscout: " << (rscout < fscout) << endl;

// while ( rscout < fscout ) {
// 	rscout++;
// 	cout << (rscout->first - (rscout-1)->first) << endl;
// }


*/


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
			uint32_t MCheck = entry_add(key,value,_key_val,_N,_M);
			if ( MCheck == UINT32_MAX ) {
				return false;
			}
			_M = MCheck;
			return true;
		}

		inline bool update_entry(uint32_t key,uint32_t old_key,uint32_t value) {
			if ( _N + _M >= _MAX ) return false;
			manage_update(old_key,key,value);
			return true;
		}

		inline bool remove_entry(uint32_t key) {
			if ( (_blackout_count == 0) && (_M > 0) ) {
				_N += _M;
				_M = 0;
			}
			uint32_t update_count = entry_remove(key,_key_val,_N,_M,(_blackout_count != 0));
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

	//protected:
	public:

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


	// ----
		inline void sort_small_mostly_increasing(pair<uint32_t,uint32_t> *beg,pair<uint32_t,uint32_t> *end) {
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

		inline uint32_t entry_add(uint32_t key,uint32_t value,pair<uint32_t,uint32_t> *key_val,uint32_t N,uint32_t M) {
			if ( N == 0 ) {
				pair<uint32_t,uint32_t> *p = key_val + M;
				p->first = key;
				p->second = value;
				return (M+1);
			} else {
				pair<uint32_t,uint32_t> *check = key_val + N - 1;
				if ( key < check->first ) {
					return UINT32_MAX;
				}
				pair<uint32_t,uint32_t> *p = key_val + N + M;
				p->first = key;
				p->second = value;
				return (M+1);
			}
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
		inline uint32_t entry_remove(uint32_t key,pair<uint32_t,uint32_t> *key_val,uint32_t N,uint32_t M,bool has_holes) {
			//
			pair<uint32_t,uint32_t> *p = has_holes ? bin_search_with_blackouts_increasing(key,key_val, N) : 
											b_search(key,key_val,N);
			if ( (p == nullptr) && (M > 0) ) {
				p = key_val + N;
				pair<uint32_t,uint32_t> *end = p + M;
				while ( p < end ) {
					if ( p->first == key ) {		// going on the assumption that the 'late' added elements are not sorted, generally.
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



	public:

		UpdateSource			_proc_queue;
		//
		pair<uint32_t,uint32_t> *_key_val;
		pair<uint32_t,uint32_t> *_output;
		pair<uint32_t,uint32_t> *_key_val_A;
		pair<uint32_t,uint32_t> *_key_val_B;
		bool					_AB_switch;
		uint32_t				_MAX;
		uint32_t				_N;
		uint32_t				_M;
		uint32_t				_blackout_count;
		//
		uint16_t				_proc_count;
};





void try_searching(time_t key, KeyValueManager &tester) {
	cout << "SEARCH" << endl; //
    uniform_int_distribution<uint32_t> pdist(0,160);

	tester._N += tester._M; tester._M = 0;

	cout << tester._N << endl; 
	/*
	for ( uint8_t i = 0; i < 100; i++ ) {
		uint32_t k = pdist(gen_v) % tester._N;
		cout << k << " :: "; cout.flush();
		pair<uint32_t,uint32_t> *p = b_search((key + k),tester._key_val,tester._N);
		if ( p != nullptr ) {
			cout << "k: " << k << " " << p->second << endl;
		} else {
			cout << "could not find: " << k << endl;
		}
	}
	//
	for ( int i = 0; i < 198; i++ ) {
		cout << primary_storage[i].first << "\t" << primary_storage[i].second << " k: " << i << endl;
		pair<uint32_t,uint32_t> *p = b_search((key + i),tester._key_val,tester._N);
		if ( p != nullptr ) {
			cout << p->first << "\t" << p->second << " k: " << i << endl;
		} else {
			cout << "could not find: " << i << endl;
		}
	}
	*/

	cout << "---------try_searching--------(3)---" << endl;
	cout << tester._N << endl; 
	for ( uint8_t i = 0; i < 100; i++ ) {
		uint8_t jj = (i+3);
		uint32_t k = pdist(gen_v) % jj;
		cout << k << " :: "; cout.flush();
		pair<uint32_t,uint32_t> *p = b_search((key + k),tester._key_val,(i+3));
		if ( p != nullptr ) {
			cout << "k: " << k << " " << p->second << endl;
		} else {
			cout << "could not find: " << k << endl;
		}
	}

}


template<const uint32_t MAX_ENTRIES>
void try_removing(time_t key, KeyValueManager &tester) {
	//
	auto start = high_resolution_clock::now();
	//
	tester.remove_entry(key + 20);
	tester.remove_entry(key + 15);
	tester.remove_entry(key + 120);
	tester.remove_entry(key + 120);
	tester.remove_entry(key + 120);
	tester.remove_entry(key + 16);
	for ( uint32_t i = 0; i < MAX_ENTRIES; i++ ) {
		tester.remove_entry(key + i);
	}
	//
	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(stop - start);
	cout << "REMOVE" << (double)(duration.count())/(1000000.0) << " seconds" << endl;

}



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

/// ---------------


void test_buckets() {

	uint32_t buckets[21];
	//
	uint32_t delta = 20;
	for ( int i = 0; i < 21; i++ ) {
		buckets[i] = delta*i;
	}

	for ( int i = 0; i < 21; i++ ) {
		cout << buckets[i] << ", ";
	}

	cout << endl;

	//
	auto last = buckets[20];
	for ( uint32_t key = 0; key < last; key++ ) {
		auto index = u32b_search( key, buckets, 21);
		cout << " .. " << key << " bucket # " << index << " " << (( index < 21 ) ? buckets[index] : -1 ) << endl;
	}
	//

}



uint32_t search_epoch(uint32_t key, uint32_t *epoch_list, uint32_t step_size, uint32_t N) {
	auto index = u32b_search( key, epoch_list, N);
	return index*step_size;
}


void tryout_bin_search(KeyValueManager &tester,uint32_t key,uint32_t *epoch_list,uint32_t count_size,uint32_t step_size,uint32_t EN) {
	//
    uniform_int_distribution<uint32_t> pdist(0,160);
	uint32_t delta = pdist(gen_v);
	//
	//
	for ( uint32_t j = 0; j < 1000; j++ ) {
		for ( uint32_t i = 0; i < (count_size-10); i++  ) {
			//
			auto k = (i + delta) % (count_size-100);
			auto kk = (key + k);

			//auto top_epoch_offset = search_epoch(kk,top_epoch_list,epic_step,top_EN);
	// + top_epoch_offset
			auto epoch_offset = search_epoch(kk,epoch_list,step_size,EN); //epic_step);
			//epoch_offset += top_epoch_offset*step_size;

			// if ( epoch_offset < (EN/2) ) {
			// 	uint8_t can_write = 0;
			// 	while ( !lock_values[0].compare_exchange_weak(can_write,0) && (can_write != 0) );
			// 	thread_KS[0]= kk;
			// 	epoch_offsets[0] = epoch_offset;
			// 	lock_values[0].store(2);
			// } else {
			// 	uint8_t can_write = 0;
			// 	while ( !lock_values[0].compare_exchange_weak(can_write,0) && (can_write != 0) );
			// 	thread_KS[1]= kk;
			// 	epoch_offsets[1] = epoch_offset;
			// 	lock_values[1].store(2);
			// }

			//pair<uint32_t,uint32_t> *p = b_search(kk,tester._key_val + epoch_offset, step_size);

			pair<uint32_t,uint32_t> *p = bin_search_with_blackouts_increasing(kk,tester._key_val + epoch_offset, step_size);
			//pair<uint32_t,uint32_t> *p = bin_search_with_blackouts_increasing(kk,tester._key_val, tester._N);
		
			// if ( (p != nullptr) && (i == (j%190)) ) {
			// 	cout << i << ") " << (key + k) << " = " << p->first << " " << p->second << endl;
			// } else if ( i == (j%190) ){
			// 	p = buffer_seek((key + k),tester._key_val,198);
			// 	cout << i << ") " << "not found: " << (key + k) << " -=- " << k << " " << p->first << " :: " << p->second << endl;
			// }
		
			//
		}
	}
}



const uint32_t TEST_SIZE = 750000;

// 7500000 @ 44(+/-) sec. | 75000 @ 0.44 | 7500 @ 0.035 sec | 750000 @ 3.9 sec 
// no threads...



int main(int arcg, char **argv) {
	//
	uint32_t count_size = TEST_SIZE;
	pair<uint32_t,uint32_t> *primary_storage = new pair<uint32_t,uint32_t>[count_size];
	pair<uint32_t,uint32_t> *secondary_storage = new pair<uint32_t,uint32_t>[count_size];
	uint16_t expected_proc_max = 8;
	pair<uint32_t,uint32_t> shared_queue[expected_proc_max];

	KeyValueManager tester(primary_storage, secondary_storage, count_size, shared_queue,  expected_proc_max);
	//
	time_t key = time(nullptr);
	cout << key << endl;
	cout << "------------------------------" << endl;
	uint32_t value = 1;
	//
	tester.add_entry(key,value);
	cout << primary_storage[0].first << endl;
	cout << "----------------------------->>-" << endl;

	for ( int i = 1; i < 10; i++ ) {
		tester.add_entry(key + i,i+1);
	}
	//
	tester._N = 10;
	tester._M = 0;
	//
	for ( uint32_t i = 10; i < (count_size-2); i++ ) {
		tester.add_entry(key + i,i+1);
	}

	//print_stored(primary_storage);
	//try_searching(key,tester,primary_storage);

	/*

	tester._N = (count_size-2);
	tester._M = 0;
	try_removing<TEST_SIZE>(key,tester);

	*/

	tester._N = (count_size-2);
	tester._M = 0;
    uniform_int_distribution<uint32_t> pdist(0,160);
	//
	uint32_t delta = pdist(gen_v);
	//

	uint32_t step_size = 750;
	uint32_t EN = count_size/step_size;

	//
	cout << "count_size: " << count_size << " step_size: " << step_size << "(count_size/step_size) == EN: " << EN << endl;

	uint32_t *epoch_list = new uint32_t[EN + 1];
	for ( uint32_t i = 0; i < EN; i++ ) {
		pair<uint32_t,uint32_t> *p = tester._key_val + i*step_size;
		epoch_list[i] = p->first;
	}

	auto tail_count = count_size - EN*step_size;
	cout << "tail_count: " << tail_count << endl;



	uint32_t top_EN = 100;
	auto epic_step = (EN/top_EN);
	uint32_t top_epoch_list[top_EN + 1];


	cout << "top_EN: " << top_EN << " epic_step : " << epic_step << endl;

	for ( uint32_t i = 0; i < top_EN; i++ ) {
		top_epoch_list[i] = epoch_list[(i*epic_step)];
	}

/*
	for ( uint32_t i = 0; i < top_EN; i++ ) {
		cout << "(" << i << "): "<< top_epoch_list[i] << endl;
	}
*/

	cout << "TEST_SIZE: "  << TEST_SIZE << " log2(TEST_SIZE) :: " << log2(TEST_SIZE) << endl;
	cout << "TEST_SIZE: "  << TEST_SIZE << " step_size :: " << step_size << " epic_step :: " << epic_step << endl;
	cout << "TEST_SIZE: "  << TEST_SIZE << " epics N :: " << EN << " top_EN :: " << top_EN << endl;
	cout << "lg2(step_size): " << log2(step_size) << " lg2(epic_step): " << log2(epic_step) << " all: " << (log2(step_size) + log2(epic_step) + log2(top_EN)) << endl;
	//

	cout << "NlogN:: " << (double)TEST_SIZE*log2(TEST_SIZE) << " >= " << (step_size*log2(step_size) + epic_step*log2(epic_step) + top_EN*log2(top_EN)) << endl;

	//

	bool running = true;
	atomic<uint8_t>		lock_values[2];
	uint32_t			thread_KS[2];
	uint32_t			epoch_offsets[2];

	lock_values[0].store(0);
	lock_values[1].store(0);
/*
	thread search_1([&](){
		uint32_t epoch_offset = UINT32_MAX;
		while ( running ) {
			uint8_t can_read = 0;
			while ( !lock_values[0].compare_exchange_weak(can_read,2) );
			uint32_t kk = thread_KS[0];
			epoch_offset = epoch_offsets[0];
			lock_values[0].store(0);
			pair<uint32_t,uint32_t> *p = bin_search_with_blackouts_increasing(kk,tester._key_val + epoch_offset, step_size);
		}
	});
	thread search_2([&](){
		uint32_t epoch_offset = UINT32_MAX;
		while ( running ) {
			uint8_t can_read = 0;
			while ( !lock_values[0].compare_exchange_weak(can_read,2) );
			uint32_t kk = thread_KS[1];
			epoch_offset = epoch_offsets[1];
			lock_values[0].store(1);
			pair<uint32_t,uint32_t> *p = bin_search_with_blackouts_increasing(kk,tester._key_val + epoch_offset, step_size);
		}
	});
*/

	uint32_t r_start = 226;
	uint32_t r_stop = 253;

	pair<uint32_t,uint32_t> *beg = tester._key_val;
	pair<uint32_t,uint32_t> *end = beg + 256;   // maybe 64 procs allowed to run unchecked about 4 times.
	double total_time = 0;


	for ( uint32_t i = 0; i < 1000000L; i++ ) {

		auto t_min = beg[r_start - 10].first;
		uint32_t loc_top = min((uint32_t)(r_stop + 10),(uint32_t)254);
		auto t_max = beg[loc_top].first;
		auto rn = r_stop/2;
		for ( uint32_t i = r_start; i < rn; i++ ) {
			beg[i].first = t_min + (i-r_start)*2;
			beg[r_stop-i].first = t_max - (i-r_start)*2;
		}

/*
    uniform_int_distribution<uint32_t> pdist2(r_start,r_stop);
	for ( uint32_t i = r_start; i < r_stop; i++ ) {
		uint32_t swapper = pdist2(gen_v);
		if ( i != swapper ) {
			swap(beg[i],beg[swapper]);
		}
	}
*/


	// for ( uint32_t i = r_start; i < 256; i++ ) {
	// 	cout << (beg[i].first - t_min) << endl;
	// }

		auto start = high_resolution_clock::now();

	// baseline test
	//
	// 0.008859 seconds 
	// uint64_t test_val = 1;
	// for ( uint64_t i = 1; i < 10000000L; i++ ) {
	// 	test_val += i % 771;
	// }



		tester.sort_small_mostly_increasing(beg,end);
	//running = false;

	// search_1.join();
	// search_2.join();

	//cout << endl;
	//
		auto stop = high_resolution_clock::now();
		auto duration = duration_cast<microseconds>(stop - start);

		total_time += (double)(duration.count())/(1000000.0);
	}


	cout << total_time << " seconds" << endl;

	// for ( uint32_t i = r_start; i < rn; i++ ) {
	// 	cout << beg[i].second << ", "; cout.flush();
	// 	if ( ( i < 256 ) &&  (beg[i].first > beg[i+1].first) ) {
	// 		cout << endl;
	// 		for ( uint32_t j = i - 4; j < i+4; j++ ) {
	// 			cout << "unsorted: " << j << " :: " << beg[j].second << endl;
	// 		}
	// 	}
	// }

	// cout << endl;
	cout << "--------------------------------"  << endl;

	for ( uint32_t i = 0; i < 256; i++ ) {
		cout << beg[i].second << ", "; cout.flush();
		if ( ( i < 256 ) &&  (beg[i].first > beg[i+1].first) ) {
			cout << endl;
			for ( uint32_t j = i - 4; j < i+4; j++ ) {
				cout << "unsorted: " << j << " :: " << beg[j].first << endl;
			}
		}
	}
	cout << endl;

	//cout << test_val << endl;  (see baseline test)

	//print_stored(primary_storage);

    return(0);
}

