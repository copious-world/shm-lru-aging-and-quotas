
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


#include "./holey_buffer.h"



// Experiment with atomics for completing hash table operations.


static_assert(sizeof(uint64_t) == sizeof(atomic<uint64_t>), 
    "atomic<T> isn't the same size as T");

static_assert(atomic<uint64_t>::is_always_lock_free,  // C++17
    "atomic<T> isn't lock-free, unusable on shared mem");

// 


// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------





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


// debugging methods

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


// end debugging methods

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

			//pair<uint32_t,uint32_t> *p = 
			bin_search_with_blackouts_increasing(kk,tester._key_val + epoch_offset, step_size);
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


void tryout_sort_small_mostly_increasing(KeyValueManager &tester) {

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

		auto start = high_resolution_clock::now();

		tester.sort_small_mostly_increasing(beg,end);

		auto stop = high_resolution_clock::now();
		auto duration = duration_cast<microseconds>(stop - start);

		total_time += (double)(duration.count())/(1000000.0);
	}

	cout << total_time << " seconds" << endl;

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
}



const uint32_t TEST_SIZE = 750000;

// 7500000 @ 44(+/-) sec. | 75000 @ 0.44 | 7500 @ 0.035 sec | 750000 @ 3.9 sec 
// no threads...


int main(int arcg, char **argv) {
	//
	uint32_t count_size = TEST_SIZE;
	pair<uint32_t,uint32_t> *primary_storage = new pair<uint32_t,uint32_t>[count_size];
	uint16_t expected_proc_max = 8;
	pair<uint32_t,uint32_t> shared_queue[expected_proc_max];

	KeyValueManager tester(primary_storage, count_size, shared_queue,  expected_proc_max);
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
	//

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

	double total_time = 0;
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


	// tester._N = (count_size-2);
	// tester._M = 0;
    // uniform_int_distribution<uint32_t> pdist(0,160);
	// //
	// uint32_t delta = pdist(gen_v);
	// //

	uniform_int_distribution<uint32_t> updt_dist(100,1024);


	tester._N = count_size/2;
	tester._M = 0;

	key = tester._key_val[(count_size-4)].first;
	for ( uint j = 0; j < 128; j++ ) {
		//
		uint32_t k = updt_dist(gen_v);
		uint32_t old_key = tester._key_val[k].first;
		//
		//uint32_t value = 20000 + j;
		//
		//cout << "OK 1" << endl;
		tester.update_entry((key + j), old_key);
		tester.add_entry(key + j + 128,20000 + j);
	}


	//

	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(stop - start);

	total_time += (double)(duration.count())/(1000000.0);
	
	// ---- 
	cout << total_time << " seconds" << endl;

	//cout << test_val << endl;  (see baseline test)

	//print_stored(primary_storage);

    return(0);
}

