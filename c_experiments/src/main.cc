
//
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <iostream>
#include <cstring>

#include <deque>
#include <map>
#include <ctime>

#include <thread>
#include <atomic>

#include <csignal>

#include <chrono>
#include <vector>

#include <future>
 
#include <bitset>
#include <bit>
#include <cstdint>
#include <random>
#include <bit>


//#include <linux/futex.h>

#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ipc.h>


static constexpr bool noisy_test = true;
[[maybe_unused]] static constexpr uint8_t THREAD_COUNT = 64;

using namespace std;
using namespace chrono;
using namespace literals;


// ---- ---- ---- ---- ---- ---- ---- ----
//


// Experiment with atomics for completing hash table operations.


static_assert(sizeof(uint64_t) == sizeof(atomic<uint64_t>), 
    "atomic<T> isn't the same size as T");

static_assert(atomic<uint64_t>::is_always_lock_free,  // C++17
    "atomic<T> isn't lock-free, unusable on shared mem");

// 

#include <type_traits>


// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------



//#include "node_shm_LRU.h"

#include "time_bucket.h"
#include "random_selector.h"
#include "shm_seg_manager.h"

#include "node_shm_tiers_and_procs.h"

[[maybe_unused]] static TierAndProcManager<4> *g_tiers_procs = nullptr;

using namespace node_shm;



// #include "node_shm_HH_for_test.h"

// ---- ---- ---- ---- ---- ---- ---- ---



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

SharedSegmentsManager *g_ssm_catostrophy_handler = nullptr;

/*
SharedSegmentsTForm<HH_map_test<>> *g_ssm_catostrophy_handler_custom = nullptr;
*/

volatile std::sig_atomic_t gSignalStatus;

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



// 7500000 @ 44(+/-) sec. | 75000 @ 0.44 | 7500 @ 0.035 sec | 750000 @ 3.9 sec 
// no threads...

bool dataReady{false};

mutex mutex_;
condition_variable condVar1;          // (1)
condition_variable condVar2;          // (2)

atomic<uint32_t> counter{};
//uint32_t counter;
constexpr uint32_t countlimit = 10000000; // 1'000'000; // 10000000;   70000000;

void ping() {

    while( counter <= countlimit ) {
        {
            unique_lock<mutex> lck(mutex_);
            condVar1.wait(lck, []{return dataReady == false;});
            dataReady = true;
        }
        ++counter; 
        condVar2.notify_one();              // (3)
  }
}

void pong() {

    while( counter < countlimit ) {
        {
            unique_lock<mutex> lck(mutex_);
            condVar2.wait(lck, []{return dataReady == true;});
            dataReady = false;
        }
        condVar1.notify_one();            // (3)
  }

}



atomic_flag condAtomicFlag{};
atomic_flag g_ping_lock = ATOMIC_FLAG_INIT;


constexpr bool noisy_prints = false; 

void f_hit(bool ab_caller,uint32_t count) {
  //
  if ( noisy_prints ) {
    if ( ab_caller ) {
      cout << "P1-a: " << count << " is diff: " << (countlimit - count) << endl;
    } else {
      cout << "P1-b: " << count << " is diff: " << (countlimit - count) << endl;
    }
  }
}

void a_ping_1() {
  //
#ifndef __APPLE__

  while ( counter < countlimit ) {
      while ( g_ping_lock.test(memory_order_relaxed) ) ;
      //g_ping_lock.wait(true);
      ++counter;
      f_hit(true,counter);
      g_ping_lock.test_and_set();   // set the flag to true
      g_ping_lock.notify_one();
  }
  g_ping_lock.test_and_set();
  g_ping_lock.notify_one();
#endif

  cout << "P1: " << counter << " is diff: " << (countlimit - counter) << endl;
  //
}

void a_pong_1() {
  //
#ifndef __APPLE__
  while ( counter <= countlimit ) {
      while ( !(g_ping_lock.test(memory_order_relaxed)) ) g_ping_lock.wait(false);
      uint32_t old_counter = counter;
      f_hit(false,counter);
      g_ping_lock.clear(memory_order_release);
      g_ping_lock.notify_one();
      if ( counter == countlimit ) {
        if ( old_counter < counter ) {
          while ( !(g_ping_lock.test_and_set()) ) usleep(1);
          f_hit(false,counter);
        }
        break;
      }
  }
  //
  cout << "P1-b: " << counter << " is diff: " << (countlimit - counter) << endl;
#endif
  //
}



atomic_flag g_lock = ATOMIC_FLAG_INIT;


void f(int n)
{
    for (int cnt = 0; cnt < 40; ++cnt)
    {
        while ( g_lock.test_and_set(memory_order_acquire) ) // acquire lock
        {
            // Since C++20, it is possible to update atomic_flag's
            // value only when there is a chance to acquire the lock.
            // See also: https://stackoverflow.com/questions/62318642
        #if defined(__cpp_lib_atomic_flag_test)
            while (g_lock.test(memory_order_relaxed)) // test lock
        #endif
                ; // spin
        }
        static int out{};
        cout << n << ((++out % 40) == 0 ? '\n' : ' ');
        g_lock.clear(memory_order_release); // release lock
    }
}


void time_bucket_test() {
  // ----
  uint32_t timestamp = 100;
  uint32_t N = 32; // 300000;
  Tier_time_bucket timer_table[N]; // ----

  atomic<uint32_t> atom_ints[N*2];

  for ( uint32_t i = 0; i < N; i++ ) {
    timer_table[i]._lb_time = &atom_ints[i*2];
    timer_table[i]._ub_time = &atom_ints[i*2 + 1];
    timer_table[i]._lb_time->store(i*5);
    timer_table[i]._ub_time->store((i+1)*5);
  }

  timer_table[N-1]._ub_time->store((UINT32_MAX - 2));

  // for ( uint32_t i = 0; i < N; i++ ) {
  //     auto lb = timer_table[i]._lb_time->load();
  //     auto ub = timer_table[i]._ub_time->load();
  //     cout << i << ". (lb,ub) = (" << lb << "," << ub << ") ..";
  //     cout.flush();
  // }
  // cout << endl;

  auto NN = N*5;
  uint32_t found_1 = 0;
  uint32_t found_3 = 0;
  uint32_t nowish = 0; 

  const auto right_now = std::chrono::system_clock::now();
  nowish = std::chrono::system_clock::to_time_t(right_now);

  for ( uint32_t i = 0; i < NN; i++ ) {

    // found_1 = time_interval_b_search(i,timer_table,N);
    // if ( found_1 == UINT32_MAX ) {
    //   cout << i << " broken at " << endl;
    // }


    found_3 = time_interval_b_search(nowish,timer_table,N);

  }

  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;


  // test 2
	auto start = chrono::system_clock::now();  

  for ( uint32_t i = 0; i < NN; i++ ) {
    //
    found_1 = time_interval_b_search(i,timer_table,N);
    //
  }

  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  found_1 = time_interval_b_search(timestamp,timer_table,N);
  uint32_t found_2 = time_interval_b_search(0,timer_table,N);


  //
  cout << "found: " << found_2 << endl;
  cout << "found: " << found_1 << endl;
  cout << "found: " << found_3 << endl;
  //
  cout << "found 3: (" << nowish << ") " << timer_table[found_3]._lb_time->load() << "," << timer_table[found_3]._ub_time->load() << endl;
  //
  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;

}


void ping_pong_test() {

  uint32_t nowish = 0; 
  const auto right_now = std::chrono::system_clock::now();
  nowish = std::chrono::system_clock::to_time_t(right_now);

  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;

  // test 2
	auto start = chrono::system_clock::now();
  //

  cout << "starting a test" << endl;
#ifndef __APPLE__

  g_ping_lock.clear();
  
#endif

  thread t1(a_ping_1);
  thread t2(a_pong_1);
  //
  start = chrono::system_clock::now();

#ifndef __APPLE__
  g_ping_lock.notify_all();
#endif

  
  t1.join();
  t2.join();

  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;
}


void mutex_ping_pong() {
		thread t1(ping);
		thread t2(pong);
		//
		t1.join();
		t2.join();
}

void capability_test() {
  #if defined(__cpp_lib_atomic_flag_test)
      cout << "THERE REALLY ARE ATOMIC FLAGS" << endl;
  #endif

  #if defined(_GLIBCXX_HAVE_LINUX_FUTEX)
        cout << "There really is a platform wait" << endl;
  #endif

  cout << "size of unsigned long: " << sizeof(unsigned long) << endl;
}


void random_bits_test(Random_bits_generator<> &bs) {
  for ( uint32_t i = 0; i < 200000; i++ ) {
    bs.pop_bit();
//    cout << (bs->pop_bit() ? '1' : '0');
//    cout.flush();
  }
  //cout << endl;
}


// chrono::system_clock::time_point shared_random_bits_test() {

//   auto bs = new Random_bits_generator<65000,8>();
//   //random_bits_test(*bs);

//   for ( int i = 0; i < 8; i++ ) {
//     uint32_t *bits_for_test = new uint32_t[bs->_bits.size()+ 4*sizeof(uint32_t)];
//     bs->set_region(bits_for_test,i);
//     bs->regenerate_shared(i);
//   }
//   //

// 	auto start = chrono::system_clock::now();

//   for ( uint32_t j = 0; j < 1000; j++ ) {
//     for ( uint32_t i = 0; i < 65000; i++ ) {
//       bs->pop_shared_bit();
//     }
//     bs->swap_prepped_bit_regions();
//   }

//   return start;
// }

const char *paths[4] = {
  "/Users/richardalbertleddy/Documents/GitHub/universal-content/shm-lru-aging-and-quotas/c_experiments/data/bits_buffer.txt",
  "/Users/richardalbertleddy/Documents/GitHub/universal-content/shm-lru-aging-and-quotas/c_experiments/data/com_buffer.txt",
  "/Users/richardalbertleddy/Documents/GitHub/universal-content/shm-lru-aging-and-quotas/c_experiments/data/lru_buffer.txt",
  "/Users/richardalbertleddy/Documents/GitHub/universal-content/shm-lru-aging-and-quotas/c_experiments/data/hh_buffer.txt"
};



void shared_mem_test_initialization_components() {

  int status = 0;
  SharedSegmentsManager *ssm = new SharedSegmentsManager();


  uint32_t max_obj_size = 128;
  uint32_t num_procs = 4;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;


  key_t com_key = ftok(paths[0],0);
  status = ssm->initialize_com_shm(com_key, true, num_procs, num_tiers);
  cout << status << endl;

  cout << "Com Buf size: " << ssm->get_seg_size(com_key) << endl;


  key_t key = ftok(paths[1],0);
  status = ssm->initialize_randoms_shm(key,true);
  cout << status << endl;

  //
  bool yes = false;
  cin >> yes;
  cout << yes << endl;


  cout << "Randoms size: " << ssm->get_seg_size(key) << endl;
  cout << "Total size: " << ssm->total_mem_allocated() << endl;

  // 

  list<uint32_t> lru_keys;
  list<uint32_t> hh_keys;

  for ( uint8_t i = 0; i < num_tiers; i++ ) {
    key_t t_key = ftok(paths[2],i);
    key_t h_key = ftok(paths[3],i);
    lru_keys.push_back(t_key);
    hh_keys.push_back(h_key);
  }

  cout << lru_keys.size() << ":: " << hh_keys.size() << endl;
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  status = ssm->tier_segments_initializers(true,lru_keys,hh_keys,max_obj_size,num_procs,els_per_tier);

  for ( auto p : ssm->_ids_to_seg_sizes ) {
    cout << "ID TO SEG SIZE: " << p.first << ", " << p.second << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}




void shared_mem_test_initialization_one_call() {

  int status = 0;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  uint32_t max_obj_size = 128;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint32_t num_procs = 4;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  key_t com_key = ftok(paths[0],0);
  key_t randoms_key = ftok(paths[1],0);

  list<uint32_t> lru_keys;
  list<uint32_t> hh_keys;

  for ( uint8_t i = 0; i < num_tiers; i++ ) {
    key_t t_key = ftok(paths[2],i);
    key_t h_key = ftok(paths[3],i);
    lru_keys.push_back(t_key);
    hh_keys.push_back(h_key);
  }

  status = ssm->region_intialization_ops(lru_keys, hh_keys, true,
                                  num_procs, num_tiers, els_per_tier, max_obj_size,  com_key, randoms_key);
  //
  //
  cout << "All buffers initialized: ... continue: "; cout.flush();
  bool yes = false;
  cin >> yes;
  cout << yes << endl;

  for ( auto p : ssm->_ids_to_seg_sizes ) {
    cout << "ID TO SEG SIZE: " << p.first << ", " << p.second << endl;
  }
  cout << endl;

  auto check_lru_sz = LRU_cache::check_expected_lru_region_size(max_obj_size, els_per_tier,num_procs);
  auto check_hh_sz = HH_map<>::check_expected_hh_region_size(els_per_tier,num_procs);
  cout << "LRU Expected Buf size: "  << check_lru_sz << endl;
  cout << " HH Expected Buf size: "  << check_hh_sz << endl;

  for ( auto p : ssm->_seg_to_lrus ) {
    cout << "LRU SEG SIZE: " <<  ssm->_ids_to_seg_sizes[p.first] << ", " << check_lru_sz << endl;
  }

  for ( auto p : ssm->_seg_to_hh_tables ) {
    cout << " HH SEG SIZE: " <<  ssm->_ids_to_seg_sizes[p.first] << ", " << check_hh_sz << endl;
  }

  auto check_com_sz = TierAndProcManager<>::check_expected_com_region_size(num_procs,num_tiers);
  cout << "Com Buf size: " << ssm->get_seg_size(com_key) << " check_com_sz: " << check_com_sz << endl;
  //
  //
  auto rsize =  ssm->get_seg_size(randoms_key);
  size_t predicted_rsize = Random_bits_generator<>::check_expected_region_size;   //sizeof(uint32_t)*256*4;  // default sizes
  //
  cout << "Randoms size: " << rsize << " same size: " << (rsize == predicted_rsize) << " should be: " << predicted_rsize << endl;
  cout << "Possible random bits size <1024,16>: " << Random_bits_generator<1024,16>::check_expected_region_size << endl;

  cout << "Total size: " << ssm->total_mem_allocated() << endl;

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}





template<uint32_t arg_N>
struct val {
    static constexpr auto N = arg_N;
};

template<template <uint32_t> typename T, uint32_t N>
constexpr auto extract(const T<N>&) -> val<N>;

template<typename T>
constexpr auto extract_N = decltype(extract(std::declval<T>()))::N;



template<template <uint32_t,uint8_t> typename T, uint32_t N>
constexpr auto extract2(const T<N,4>&) -> val<N>;

template<typename T>
constexpr auto extract_N2 = decltype(extract2(std::declval<T>()))::N;



void butter_bug_nothing() {

  uint32_t nowish_1 = 0;
  const auto right_now_1 = std::chrono::system_clock::now();
  nowish_1 = std::chrono::system_clock::to_time_t(right_now_1);
  // ----
  uint32_t k = 0;
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    k++;
  }
  // ----
  chrono::duration<double> dur_1 = chrono::system_clock::now() - right_now_1;
  cout << "butter_bug test 1: " << k << "     " << dur_1.count() << " seconds" << endl;

}


void butter_bug_something() {

  uint8_t butter_bug[10000];

  uint32_t nowish_1 = 0;
  const auto right_now_1 = std::chrono::system_clock::now();
  nowish_1 = std::chrono::system_clock::to_time_t(right_now_1);
  // ----
  uint32_t k = 0;
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    k++;
    butter_bug[k%10000] = k;
  }
  // ----
  chrono::duration<double> dur_1 = chrono::system_clock::now() - right_now_1;
  cout << "butter_bug test 2: " << butter_bug[k%10000] << "     " << dur_1.count() << " seconds" << endl;

}



void butter_bug_walk_only() {

  uint8_t butter_bug[10000];

  uint8_t *bb_tmp = butter_bug;
  uint8_t *bb_end = butter_bug + 10000;
  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    bb_tmp++; if ( bb_tmp >= bb_end ) bb_tmp = butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test 3: " << (int)(bb_tmp - butter_bug) << "     " << dur_2.count() << " seconds" << endl;

}



void butter_bug_walk_n_store() {

  uint8_t butter_bug[10000];

  uint8_t *bb_tmp = butter_bug;
  uint8_t *bb_end = butter_bug + 10000;
  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    *bb_tmp++ = ii; if ( bb_tmp >= bb_end ) bb_tmp = butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_n_store: " << (int)(bb_tmp - butter_bug) << "     " << dur_2.count() << " seconds" << endl;

}


void butter_bug_walk_step_n_store() {

  uint8_t butter_bug[10000];

  uint8_t *bb_tmp = butter_bug;
  uint8_t *bb_end = butter_bug + 10000;

  uint32_t step = sizeof(uint32_t);

  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    *bb_tmp = ii; bb_tmp += step; if ( bb_tmp >= bb_end ) bb_tmp = butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_step_n_store: " << (int)bb_tmp[0] << "     " << dur_2.count() << " seconds" << endl;

}


void butter_bug_walk_struct() {

  uint8_t butter_bug[10000];
  memset(butter_bug,0,10000);

  hh_element *bb_tmp = (hh_element *)butter_bug;
  hh_element *bb_end = (hh_element *)(butter_bug + 10000);

  //uint32_t step = sizeof(uint32_t);

  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {               // bb_tmp->key = ii;
    bb_tmp++;  if ( bb_tmp >= bb_end ) bb_tmp = (hh_element *)butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_struct: " << (int)bb_tmp->c.key << "     " << dur_2.count() << " seconds" << endl;

}



void butter_bug_walk_struct_n_store() {

  uint8_t butter_bug[10000];

  hh_element *bb_tmp = (hh_element *)butter_bug;
  hh_element *bb_end = (hh_element *)(butter_bug + 10000) - 1;

  //uint32_t step = sizeof(uint32_t);

  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {               // bb_tmp->key = ii;
    bb_tmp->c.key = ii;
    bb_tmp++;
    if ( bb_tmp >= bb_end ) bb_tmp = (hh_element *)butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_struct_n_store: " << bb_tmp->c.key << "     " << dur_2.count() << " seconds" << endl;

}



void butter_bug_test() {
  //
  butter_bug_nothing();
  butter_bug_something();
  butter_bug_walk_only();
  butter_bug_walk_n_store();
  butter_bug_walk_step_n_store();
  butter_bug_walk_struct();
  butter_bug_walk_struct_n_store();
  //
}


void calc_prob_limis_experiment(uint64_t tab_sz,uint64_t num_keys) {
  double alpha = (double)(num_keys)/tab_sz;
  double items_per_bucket = 1.0 + (exp(2*alpha) - 1 - 2*alpha)/4;
  cout << "items_per_bucket: " << items_per_bucket << endl;
}



void calc_prob_limis() {
  uint64_t tab_sz = 4000000000L;     // m
  uint64_t num_keys = 400000000L; //3000000000L;   // n
  calc_prob_limis_experiment(tab_sz,num_keys);

  tab_sz = 4000000L;     // m
  num_keys = 3000000L; //3000000000L;   // n
  calc_prob_limis_experiment(tab_sz,num_keys);

  tab_sz = 4000000L;     // m
  num_keys = 1000000L; //3000000000L;   // n
  calc_prob_limis_experiment(tab_sz,num_keys);


  tab_sz = 4000000L;     // m
  num_keys = 4000000L; //3000000000L;   // n
  calc_prob_limis_experiment(tab_sz,num_keys);


  cout << endl;
  cout << "Probability of probe length > 32" << endl;

  double alpha = (double)(4000000000L)/4000000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;

  alpha = (double)(3500000L)/4000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;
  
  alpha = (double)(3000000L)/4000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;

  alpha = (double)(2000000L)/4000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;

  alpha = (double)(1000000L)/4000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;

  cout << endl;
  cout << "Probability of more than 32 keys in a bucket is 1/factorial_32:" << endl;

  double factorial_32 = 1.0;
  for( int i = 1; i <= 32; i++ ) factorial_32 = factorial_32*i;
  cout << "factorial_32: " << factorial_32 <<  endl;
  cout << "1/factorial_32: " << 1.0/factorial_32 << endl;
  cout << endl;




 
  struct S
  {
      // will usually occupy 2 bytes:
      unsigned char b1 : 3; // 1st 3 bits (in 1st byte) are b1
      unsigned char    : 2; // next 2 bits (in 1st byte) are blocked out as unused
      unsigned char b2 : 6; // 6 bits for b2 - doesn't fit into the 1st byte => starts a 2nd
      unsigned char b3 : 2; // 2 bits for b3 - next (and final) bits in the 2nd byte
  };
  

    std::cout << sizeof(S) << '\n'; // usually prints 2
 
    S s;
    // set distinguishable field values
    s.b1 = 0b111;
    s.b2 = 0b101111;
    s.b3 = 0b11;
 
    // show layout of fields in S
    auto i = (uint16_t)(*((uint16_t *)(&s)));
    // usually prints 1110000011110111
    // breakdown is:  \_/\/\_/\____/\/
    //                 b1 u a   b2  b3
    // where "u" marks the unused :2 specified in the struct, and
    // "a" marks compiler-added padding to byte-align the next field.
    // Byte-alignment is happening because b2's type is declared unsigned char;
    // if b2 were declared uint16_t there would be no "a", b2 would abut "u".
    for (auto b = i; b; b >>= 1) // print LSB-first
        std::cout << (b & 1);
    std::cout << '\n';

    int n = sizeof(S)*8;
    for (int b = 0; b < n; b++ ) // print LSB-first
        std::cout << ((((0x0001 << b) & i) >> b) & 0x1);
    std::cout << '\n';
    cout << bitset<16>{i} << endl;


  control_bits cb;

  cb.busy = 1;
  cb._even.busy = 0;
  cb._even.count = 7;
  cb._odd.count = 15;
  cb._odd.busy = 1;


  cout << "control_bits" << endl;

  cout << sizeof(cb) << endl;
  //
  auto j =  ((uint32_t *)(&cb));
  control_bits *ref = (control_bits *)j;

  cout << bitset<32>{*j} << endl;

  ref->busy = 0;
  cout << bitset<32>{*j} << endl;

  ref->_even.count++;
  cout << bitset<32>{*j} << endl;

  ref->_even.count--;
  cout << bitset<32>{*j} << endl;

  ref->count = 63;
  ref->busy = 1;
  //ref->mod = 1;
  ref->_even.count = 31;
  ref->_even.busy = 1;
  ref->_even.memb = 1;
  ref->_even.mod = 1;

  cout << bitset<32>{*j} << endl;


  cout << "<< control_bits" << endl;


  cout << "---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----" << endl;
  cout << endl;

  cout << "MASK VALUES" << endl;

  // n = sizeof(uint32_t)*8;
  // uint32_t tst_i = DOUBLE_COUNT_MASK_BASE;
  // cout << "DOUBLE_COUNT_MASK_BASE:  " << bitset<32>{tst_i} << endl;
  // tst_i = DOUBLE_COUNT_MASK;
  // cout << "DOUBLE_COUNT_MASK:       " << bitset<32>{tst_i} << endl;
  // cout << std::hex << DOUBLE_COUNT_MASK_BASE << endl;
  // cout << std::hex << DOUBLE_COUNT_MASK << endl;
  // tst_i = HOLD_BIT_SET;
  // cout << "HOLD_BIT_SET:            " << bitset<32>{tst_i} << endl;
  // cout << std::hex << HOLD_BIT_SET << endl;
  // tst_i = FREE_BIT_MASK;
  // cout << "FREE_BIT_MASK:           " << bitset<32>{tst_i} << endl;
  // cout << std::hex << HOLD_BIT_SET << endl;

  cout << std::dec;
  cout << endl;
  cout << endl;

}



void test_some_bit_patterns(void) {

  uint32_t A = 0b10110110101011101011011010101110;
  uint32_t B = 0b10111110101111101111111011101111;

  uint32_t D = 1;
  while ( D != 0 ) {
    uint32_t C = A | B;
    D = ~C;
    uint32_t E = A ^ B;

    cout << bitset<32>(A) << " A"  << endl;
    cout << bitset<32>(B) << " B"  << endl;
    cout << bitset<32>(C) << "  A | B" << endl;
    cout << bitset<32>(D) << "  ~(A | B)" << endl;
    cout << bitset<32>(E) << " A ^ B ... " << countr_one(E) << " ... " << countr_zero((A ^ B) & ~1) << endl;
    //
    cout << countr_zero(D)  <<  " : "  << countl_zero(D) << endl;
    bitset<32> d(D);
    cout << "any: " << d.any() <<  " count: " << d.count() << endl;
    uint8_t shft = countr_zero(D);
    A |= (1 << shft);
    B |= (1 << shft);
  }
  // cout << bitset<32>(E) << endl;
}



void test_zero_above(void) {
  //
  for ( uint8_t i = 0; i < 32; i ++ ) {
    cout << "test_zero_above:\t" << bitset<32>(zero_levels[i]) << endl;
  }

  uint32_t test_pattern =  1  | (1 << 4) | (1 << 7) | (1 << 11) | (1 << 16) | (1 << 17) | (1 << 20) | (1 << 21) | (1 << 22) | 
                          (1 << 23) | (1 << 24) | (1 << 26) | (1 << 27) | (1 << 29) | (1 << 30) | (1 << 31);

  cout << "test_pattern:\t\t" << bitset<32>(test_pattern) << endl;

  uint32_t a = test_pattern;
  a &= (~(uint32_t)1);

  while ( a ) {
    cout << countr_zero(a) << endl;
    uint8_t shift =  countr_zero(a);
    auto masked = a & zero_above(shift);
    cout << "test_mask   :\t\t" << bitset<32>(masked) << endl;
    a &= (~(uint32_t)(1 << shift));
  }

  a = test_pattern;
  a &= (~(uint32_t)1);

  while ( a ) {
    cout << countr_zero(a) << endl;
    uint8_t shift =  countr_zero(a);
    auto masked = test_pattern & zero_above(shift);
    cout << "test_masked :\t\t" << bitset<32>(masked) << endl;
    a &= (~(uint32_t)(1 << shift));
  }

  cout << " ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----" << endl;

  uint32_t b = 0b11101101111100110000101110111111;
  cout << "test_taken  :\t\t" << bitset<32>(b) << endl;

  uint64_t vb_probe_base[40];
  uint64_t *vb_probe = &vb_probe_base[0];
  for ( uint8_t i = 0; i < 32; i++ ) {
    *vb_probe++ = (i+1);
  }

  uint64_t v_passed = (((uint64_t)0b1111) << 34) | (0b10101010101010101010101010101);
  uint8_t hole = countr_one(b);
  uint32_t hbit = (1 << hole);
  a = test_pattern | hbit;
  b = b | hbit;
  // ----

  cout << "test_patt(b):\t\t" << bitset<32>(b) << endl;
  cout << "test_pattern:\t\t" << bitset<32>(test_pattern) << endl;
  cout << "test_patt(a):\t\t" << bitset<32>(a) << endl;

  //vb_probe->c.bits = a;
  //vb_probe->tv.taken = b | hbit;
  cout << "test_hole(a):\t\t" << bitset<32>(zero_above(hole)) << endl;
  a = a & zero_above(hole);
  cout << "test_patt(a):\t\t" << bitset<32>(a) << endl;
  //
  // unset the first bit (which indicates this position starts a bucket)
  a = a & (~((uint32_t)0x1));
  while ( a ) {
    vb_probe = &vb_probe_base[0];
    cout << "test_patt(a):\t\t" << bitset<32>(a) << endl;
    auto offset = countr_zero(a);
    a = a & (~((uint32_t)0x1 << offset));
    vb_probe += offset;
    swap(v_passed,*vb_probe);
    cout << "test_patt(a):\t\t" << bitset<32>(a) << " ++ " << v_passed << endl;
    //swap(time,vb_probe->tv.taken);  // when the element is not a bucket head, this is time... 
  }
  //
  cout << "v_passed: " << bitset<64>(v_passed) << endl;
  vb_probe = &vb_probe_base[0];
  for ( uint8_t i = 0; i < 32; i++ ) {
    cout << "state(" << (int)i << "): " << *vb_probe++ << endl;
  }
}


/*
const uint32_t DOUBLE_COUNT_MASK_BASE = 0xFF;  // up to (256-1)
const uint32_t DOUBLE_COUNT_MASK = (DOUBLE_COUNT_MASK_BASE<<16);

const uint32_t COUNT_MASK = 0x3F;  // up to (64-1)
const uint32_t HI_COUNT_MASK = (COUNT_MASK<<8);
//
const uint32_t HOLD_BIT_SET = (0x1 << 23);
const uint32_t FREE_BIT_MASK = ~HOLD_BIT_SET;
const uint32_t LOW_WORD = 0xFFFF;

const uint32_t HOLD_BIT_ODD_SLICE = (0x1 << (7+8));
const uint32_t FREE_BIT_ODD_SLICE_MASK = ~HOLD_BIT_ODD_SLICE;

const uint32_t HOLD_BIT_EVEN_SLICE = (0x1 << (7));
const uint32_t FREE_BIT_EVEN_SLICE_MASK = ~HOLD_BIT_EVEN_SLICE;


const uint32_t HH_SELECT_BIT = (1 << 24);
const uint32_t HH_SELECT_BIT_MASK = (~HH_SELECT_BIT);
const uint64_t HH_SELECT_BIT64 = (1 << 24);
const uint64_t HH_SELECT_BIT_MASK64 = (~HH_SELECT_BIT64);



*/



uint32_t my_zero_count[256][2048];
uint32_t my_false_count[256][2048];







void print_values_elems(hh_element *elems,uint8_t N,uint8_t start_at = 0) {
  for ( int i = 0; i < N; i++ ) {
    cout << "p values: " << elems->tv.value << " .. (" << (start_at + i) << ")" << endl;
    elems++;
  }
}





void test_some_bit_patterns_2(void) {   //  ----  ----  ----  ----  ----  ----  ----
  //
  uint32_t buffer[128];
  memset(buffer,0xFF,128*sizeof(uint32_t));

  const auto NEIGHBORHOOD = 32;

  uint8_t dist_base = 31;
  while ( dist_base ) {
    cout << " DISTANCE TO BASE IS: " << dist_base << endl;
    uint32_t last_view = (NEIGHBORHOOD - 1 - dist_base);
    //
    uint32_t c = 1 << 31;
    cout << bitset<32>(c) << " last_view = " <<  last_view << endl;

    if ( last_view > 0 ) {
      uint32_t *base = &buffer[64];
      uint32_t *viewer = base - last_view;
      while ( viewer != base ) {
        auto cbit = dist_base + last_view;
        cout << " cbit = " << cbit << " dist_base = " << (int)dist_base << " last_view = " << last_view << endl;
        auto vbits = *viewer;
        vbits = vbits & ~((uint32_t)0x1 << cbit);
        *viewer++ = vbits;
        last_view--;
      }
      // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

      uint32_t *reporter = &buffer[32];

      base++;
      while ( reporter < base ) {
        cout << bitset<32>(*reporter) << endl;
        reporter++;
      }
      // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    }
    dist_base--;
    cout << endl;
  }
}


/*
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<uint32_t> ud(0, num_elements-1);
    //
*/


const auto NEIGHBORHOOD = 32;


uint8_t test_usurp_membership_position(hh_element *hash_ref, uint32_t c_bits, hh_element *buffer,hh_element *end) {
  //
  uint8_t k = 0xFF & (c_bits >> 1);  // have stored the offsets to the bucket head
  cout << "test_usurp_membership_position: " << (int)k << endl;
  //
  hh_element *base_ref = (hash_ref - k);  // base_ref ... the base that owns the spot
  cout << "test_usurp_membership_position: cbits =  " << bitset<32>(base_ref->c.bits) << endl;

  base_ref = el_check_beg_wrap(base_ref,buffer,end);
  UNSET(base_ref->c.bits,k);   // the element has been usurped...
  cout << "test_usurp_membership_position: cbits =  " << bitset<32>(base_ref->c.bits) << endl;
  //
  uint32_t c = 1 | (base_ref->tv.taken >> k);   // start with as much of the taken spots from the original base as possible
  //
  cout << "test_usurp_membership_position(0): c =  " << bitset<32>(c) << endl;
  auto hash_nxt = base_ref + (k + 1);
  for ( uint8_t i = (k + 1); i < NEIGHBORHOOD; i++, hash_nxt++ ) {
    if ( hash_nxt->c.bits & 0x1 ) { // if there is another bucket, use its record of taken spots
      cout << "test_usurp_membership_position(1):  =  " << bitset<32>(hash_nxt->tv.taken << i) << " -> c.bits " << bitset<32>(hash_nxt->c.bits) << endl;
      c |= (hash_nxt->tv.taken << i); // this is the record, but shift it....
      cout << "test_usurp_membership_position(a): c =  " << bitset<32>(c) << endl;
      break;
    } else if ( hash_nxt->tv.value != 0 ) {  // set the bit as taken
      SET(c,i);
      cout << "test_usurp_membership_position(b): c =  " << bitset<32>(c) << endl;
    }
  }
  hash_ref->tv.taken = c;
  cout << "test_usurp_membership_position(c): c =  " << bitset<32>(c) << endl;
  return k;
}



hh_element *test_seek_next_base(hh_element *base_probe, uint32_t &c, uint32_t &offset_nxt_base, hh_element *buffer, hh_element *end) {
  hh_element *hash_base = base_probe;
  while ( c ) {
    auto offset_nxt = get_b_offset_update(c);
    base_probe += offset_nxt;
    base_probe = el_check_end_wrap(base_probe,buffer,end);
    if ( base_probe->c.bits & 0x1 ) {
      offset_nxt_base = offset_nxt;
      return base_probe;
    }
    base_probe = hash_base;
  }
  return base_probe;
}



void test_seek_min_member(hh_element **min_probe_ref, hh_element **min_base_ref, uint32_t &min_base_offset, hh_element *base_probe, uint32_t time, uint32_t offset, uint32_t offset_nxt_base, hh_element *buffer, hh_element *end) {
  auto c = base_probe->c.bits;		// nxt is the membership of this bucket that has been found
  c = c & (~((uint32_t)0x1));   // the interleaved bucket does not give up its base...
  if ( offset_nxt_base < offset ) {
    c = c & ones_above(offset - offset_nxt_base);  // don't look beyond the window of our base hash bucket
  }
  c = c & zero_above((NEIGHBORHOOD-1) - offset_nxt_base); // don't look outside the window
  cout << "test_seek_min_member: c = " << bitset<32>(c) << endl;
  //
  while ( c ) {			// same as while c
    auto vb_probe = base_probe;
    auto offset_min = get_b_offset_update(c);
    vb_probe += offset_min;
    vb_probe = el_check_end_wrap(vb_probe,buffer,end);

cout << "test_seek_min_member: offset_min = " << (int)offset_min << " time .. " << vb_probe->tv.taken << " " << time << endl;

    if ( vb_probe->tv.taken <= time ) {
			time = vb_probe->tv.taken;
      *min_probe_ref = vb_probe;
      *min_base_ref = base_probe;
      min_base_offset = offset_min;
    }
  
cout << "----" << endl;

  }
}


typedef struct Q_ENTRY_TEST {
	public:
		uint64_t 	loaded_value;
		uint32_t 	h_bucket;
		uint8_t		which_table;
		uint8_t		thread_id;
		uint8_t   __rest[2];
} q_entry_test;


/*
	QueueEntryHolder ...
*/

template<uint16_t const ExpectedMax = 100>
class QueueEntryHolder_test : public  SharedQueue_SRSW<q_entry_test,ExpectedMax> {};


void entry_holder_test(void) {

  cout << "entry_holder_test" << endl;
  //
  QueueEntryHolder_test<> *qeht = new QueueEntryHolder_test<>();
  //
  cout << "is empty: " << (qeht->empty() ? "true" : "false") << endl;

  q_entry_test  funny_bizz;
  funny_bizz.loaded_value = 1;
  qeht->push(funny_bizz);
  auto its_empty = qeht->empty();
  cout << "is empty: " << (qeht->empty() ? "true" : "false") << endl;
  if ( !(its_empty) ) {
    cout << "check w: " << (qeht->_w_cached - qeht->_beg) << endl;
    cout << "check r: " << (qeht->_r_cached - qeht->_beg) << endl;
    cout << qeht->_w_cached << " :: " << qeht->_r_cached << endl;
    cout << qeht->_w.load() << " :: " << qeht->_r.load() << endl;
  }

  q_entry_test  funny_bizz_out;
  if ( qeht->pop(funny_bizz_out) ) {
    cout << "funny_bizz_out.loaded_value: " << funny_bizz_out.loaded_value << endl;
  }

  if ( qeht->pop(funny_bizz_out) ) {
    cout << "funny_bizz_out.loaded_value: " << funny_bizz_out.loaded_value << endl;
  } else {
    cout << "q e h test is empty" << endl;
  }


  qeht->reset();
  for ( int i = 0; i < 100; i++ ) {
     q_entry_test  fizzy_bun;
     fizzy_bun.loaded_value = i+1;
     auto fullness = qeht->push(fizzy_bun);
     q_entry_test *writable, *nextable;
     if ( qeht->full(&writable,&nextable) ) {
      cout << "qeht full at " << i << " indicated by " << fullness << endl;
     }
  }
  its_empty = qeht->empty();
  cout << "again::  is empty: " << (qeht->empty() ? "true" : "false") << endl;
  if ( !(its_empty) ) {
    cout << "check w c: " << (qeht->_w_cached - qeht->_beg) << endl;
    cout << "check r c: " << (qeht->_r_cached - qeht->_beg) << endl;
    cout << "check w: " << (qeht->_w.load() - qeht->_beg) << endl;
    cout << "check r: " << (qeht->_r.load() - qeht->_beg) << endl;
    cout << qeht->_w_cached << " :: " << qeht->_r_cached << endl;
    cout << qeht->_w.load() << " :: " << qeht->_r.load() << endl;
  }

  for ( int i = 0; i < 100; i++ ) {
    cout << "stored [" << i << "]\t" << qeht->_entries[i].loaded_value << "\t";
  }
  cout << endl;

  for ( int j = 0; j < 20; j++ ) {
     q_entry_test  fizzy_bun;
     fizzy_bun.loaded_value = j+100;
     auto fullness = qeht->push(fizzy_bun);
     q_entry_test *writable, *nextable;
     if ( qeht->full(&writable,&nextable) ) {
      cout << "qeht full at " << j << " indicated by " << fullness << endl;
     }
     qeht->pop(fizzy_bun);
     cout << " popped: " << fizzy_bun.loaded_value << endl;
  }


  for ( int i = 0; i < 100; i++ ) {
    cout << "stored [" << i << "]\t" << qeht->_entries[i].loaded_value << "\t";
  }
  cout << endl;


  its_empty = qeht->empty();
  cout << "is empty: " << (qeht->empty() ? "true" : "false") << endl;
  if ( !(its_empty) ) {
    cout << "check w: " << (qeht->_w_cached - qeht->_beg) << endl;
    cout << "check r: " << (qeht->_r_cached - qeht->_beg) << endl;
    cout << qeht->_w_cached << " :: " << qeht->_r_cached << endl;
    cout << qeht->_w.load() << " :: " << qeht->_r.load() << endl;
  }

  while ( !(qeht->empty()) ) {
    q_entry_test  fuzzy_bin;
    qeht->pop(fuzzy_bin);
    cout << " popped: " << fuzzy_bin.loaded_value << endl;
    cout << "check w c: " << (qeht->_w_cached - qeht->_beg) << endl;
    cout << "check r c: " << (qeht->_r_cached - qeht->_beg) << endl;
    cout << "check w: " << (qeht->_w.load() - qeht->_beg) << endl;
    cout << "check r: " << (qeht->_r.load() - qeht->_beg) << endl;
    cout << qeht->_w_cached << " :: " << qeht->_r_cached << endl;
    cout << qeht->_w.load() << " :: " << qeht->_r.load() << endl;
    cout << " " << endl;
  }


  for ( int j = 0; j < 200; j++ ) {
     q_entry_test  fizzy_bun;
     fizzy_bun.loaded_value = j+200;
     auto fullness = qeht->push(fizzy_bun);
     q_entry_test *writable, *nextable;
     if ( qeht->full(&writable,&nextable) ) {
      cout << "qeht full at " << j << " indicated by " << fullness << endl;
     }
     qeht->pop(fizzy_bun);
     cout << " popped: " << fizzy_bun.loaded_value << endl;
  }


  for ( int i = 0; i < 100; i++ ) {
    cout << "stored [" << i << "]\t" << qeht->_entries[i].loaded_value << "\t";
  }
  cout << endl;


}








void entry_holder_threads_test(void) {

  cout << "entry_holder_test" << endl;
  //
  QueueEntryHolder_test<> *qeht = new QueueEntryHolder_test<>();
  //
  cout << "is empty: " << (qeht->empty() ? "true" : "false") << endl;

  uint64_t results[600];

  memset(results,0,600*sizeof(uint64_t));

  auto primary_runner = [&](void) {
      q_entry_test  funny_bizz;
      int loop_ctrl = 0;
      for ( int i = 0; i < 600; i++ ) {
        funny_bizz.loaded_value = i+1;
        while ( !qeht->push(funny_bizz) && (loop_ctrl < 100000) ) { // qeht->push(funny_bizz);   //  
          loop_ctrl++;
          if ( (loop_ctrl % (1<<14)) == 0 ) {
            __libcpp_thread_yield();
          }
        }
        loop_ctrl = 0;
      }
  };




    uint64_t j_final = 0;
    uint64_t k_final = 0;
    uint64_t q_final = 0;



  auto secondary_runner = [&](void) {
      q_entry_test  fizzy_bun;
      uint64_t j = 0;
      uint64_t k = 0;
      uint64_t q = 0;
      while ( k < 600 ) {
        if ( qeht->pop(fizzy_bun) ) {
          j = fizzy_bun.loaded_value;
          results[k++] = j;
        } else q++;
        //if ( q > 10000 ) break;
      }
      //
      q_final = q;
      k_final = k;
      j_final = j;
  };


  auto start = chrono::system_clock::now();


  thread th1(primary_runner);
  thread th2(secondary_runner);

  th1.join();
  th2.join();


  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;
  cout << "Duration pre print time: " << dur_t2.count() << " seconds" << endl;

  cout << " Q : " << q_final << endl;
  cout << " K : " << k_final << endl;
  cout << " J : " << j_final << endl;

  for ( int i = 1; i < 600; i++ ) {
    uint64_t j = i-1;
    if ( results[i] - results[j] > 1 ) {
      cout << "oops: (" << i << ")" << results[i] << " " << results[j] << endl;
    }
  }

  cout << "FINISHED" << endl;

} 



typedef struct LRU_ELEMENT_HDR_test {

	uint32_t	_info;
	uint32_t	_next;
	uint64_t 	_hash;
	time_t		_when;
	uint32_t	_share_key;

  void		init([[maybe_unused]] uint64_t hash_init = UINT64_MAX) {
  }

} LRU_element_test;


class LRU_cache_test : public AtomicStack<LRU_element_test> {

	public:

};




void stack_threads_test(void) {

  LRU_cache_test *stacker = new LRU_cache_test();
  //
  size_t el_sz = sizeof(LRU_element_test) + sizeof(uint64_t);
  //
  uint16_t test_size = 100;
  size_t reg_size = el_sz*test_size + 2*sizeof(atomic<uint32_t>*);  // two control words at the beginning
  //
  uint8_t *data_area = new uint8_t[reg_size];
  uint32_t *reserved_offsets = new uint32_t[test_size];
  
  stacker->setup_region_free_list(data_area,el_sz,reg_size);

  cout << "got through initialization" << endl;

  LRU_element_test *chk = stacker->_ctrl_free;

  cout << "2*sizeof(atomic<uint32_t>*) :: " << 2*sizeof(atomic<uint32_t>*) << endl;
  cout << "el_sz: " << el_sz << " = sizeof(LRU_element_test)  "
              << sizeof(LRU_element_test) << " + sizeof(uint64_t) " <<  sizeof(uint64_t) << endl;
  cout << "first check: " << chk->_next << endl;



  // uint8_t ii = 0;
  // while ( chk->_next != UINT32_MAX ) {
  //   auto nxt = chk->_next;
  //   if ( nxt == 0 ) {
  //     cout << "nxt is 0" << endl;
  //     break;
  //   }
  //   chk = (LRU_element_test *)(data_area + nxt);
  //   cout << chk->_next << endl;
  //   ii++; 
  //   if ( ii > test_size ) {
  //     cout << "ii greater than the test size" << endl;
  //     break;
  //   }
  // }


  //stacker->pop_number(data_area, 1000, reserved_offsets);

  stacker->pop_number(data_area, (test_size - 20), reserved_offsets);
  //
  cout << stacker->_reason << endl;

  for ( int i = 0; i < test_size; i++ ) {
    cout << reserved_offsets[i] << " "; cout.flush();
  }
  //
  cout << endl;
  //

  for ( int i = 0; i < test_size; i++ ) {
    auto offset = reserved_offsets[i];
    if ( offset != 0 ) {
      LRU_element_test *el = (LRU_element_test *)(data_area + offset);
      stacker->_atomic_stack_push(data_area,el);
    }
  }
  //



  uint32_t *reserved_offsets_1 = new uint32_t[test_size];
  uint32_t *reserved_offsets_2 = new uint32_t[test_size];


  auto primary_runner = [&](void) {
      for ( int k = 0; k < 10000; k++ ) {
        for ( int i = 0; i < test_size; i++ ) {
          auto offset = reserved_offsets_1[i];
          if ( offset != 0 ) {
            reserved_offsets_1[i] = 0;
            LRU_element_test *el = (LRU_element_test *)(data_area + offset);
            stacker->_atomic_stack_push(data_area,el);
            break;
          }
        }
        for ( int i = 0; i < test_size; i++ ) {
          auto offset = reserved_offsets_2[i];
          if ( offset != 0 ) {
            reserved_offsets_2[i] = 0;
            LRU_element_test *el = (LRU_element_test *)(data_area + offset);
            stacker->_atomic_stack_push(data_area,el);
            break;
          }
        }
      }

  };


  auto secondary_runner = [&](void) {
      //
      for ( int i = 0; i < 1000; i++ ) {
        memset(reserved_offsets_1,0,test_size);
        stacker->pop_number(data_area, 10, reserved_offsets_1);
      }
      //
  };



  auto tertiary_runner = [&](void) {
      //
      for ( int i = 0; i < 1000; i++ ) {
        memset(reserved_offsets_2,0,test_size);
        stacker->pop_number(data_area, 10, reserved_offsets_2);
      }
      //
  };


  auto start = chrono::system_clock::now();


  thread th1(primary_runner);
  thread th2(secondary_runner);
  thread th3(tertiary_runner);

  th1.join();
  th2.join();
  th3.join();


  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;
  cout << "Duration pre print time: " << dur_t2.count() << " seconds" << endl;
}





class LRU_cache_mock : public LRU_cache  {

	public:

		// LRU_cache -- constructor
		LRU_cache_mock(void *region, size_t record_size, size_t seg_sz, size_t els_per_tier, size_t reserve, uint16_t num_procs, bool am_initializer, uint8_t tier) 
                        : LRU_cache(region, record_size, seg_sz, els_per_tier, reserve, num_procs, am_initializer, tier) {
			//
		}

		virtual ~LRU_cache_mock() {}

	public:

    // 
		uint32_t		filter_existence_check(com_or_offset **messages,com_or_offset **accesses,uint32_t ready_msg_count,[[maybe_unused]] uint8_t thread_id) {
			uint32_t new_msgs_count = 0;
			while ( --ready_msg_count >= 0 ) {  // walk this list backwards...
				//
				[[maybe_unused]] uint64_t hash = (uint64_t)(messages[ready_msg_count]->_cel->_hash);
				uint32_t data_loc = _test_data_loc++; // _hmap->get(hash,thread_id);  // random locaton for test....
				//
				if ( data_loc != UINT32_MAX ) {    // check if this message is already stored
					messages[ready_msg_count]->_offset = data_loc;  // just putting in an offset... maybe something better
				} else {
					new_msgs_count++;							// the hash has not been found
					accesses[ready_msg_count]->_offset = 0;		// the offset is not yet known (the space will claimed later)
				}
			}
			return new_msgs_count;
		}



		uint64_t		get_augmented_hash_locking([[maybe_unused]] uint32_t full_hash, [[maybe_unused]] uint32_t *h_bucket_ref,[[maybe_unused]] uint8_t *which_table_ref,[[maybe_unused]] uint8_t thread_id = 1) {
			// [[maybe_unused]] HMap_interface *T = this->_hmap;
			uint64_t result = UINT64_MAX;
			//

			return result;
		}



		//  ----
		void			store_in_hash_unlocking([[maybe_unused]] uint32_t full_hash, [[maybe_unused]] uint32_t h_bucket,[[maybe_unused]] uint32_t offset,[[maybe_unused]] uint8_t which_table,[[maybe_unused]] uint8_t thread_id) {

		}


		uint32_t		getter([[maybe_unused]] uint32_t full_hash,[[maybe_unused]] uint32_t h_bucket,[[maybe_unused]] uint8_t thread_id,[[maybe_unused]] uint32_t timestamp = 0) {
			// can check on the limits of the timestamp if it is not zero
			return _test_data_loc;
		}


		/**
		 * update_in_hash
		*/

		uint64_t		update_in_hash([[maybe_unused]] uint32_t full_hash,[[maybe_unused]] uint32_t hash_bucket,[[maybe_unused]] uint32_t new_el_offset,[[maybe_unused]] uint8_t thread_id = 1) {
      uint64_t result =  (uint64_t)full_hash << HALF | hash_bucket;
			return result;
		}


		/**
		 * remove_key
		*/

		void 			remove_key([[maybe_unused]] uint32_t full_hash, [[maybe_unused]] uint32_t h_bucket,[[maybe_unused]] uint8_t thread_id, [[maybe_unused]] uint32_t timestamp) {
			//_timeout_table->remove_entry(timestamp);

		}



    uint32_t _test_data_loc{10};

 };




void test_tiers_and_procs() {
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  g_ssm_catostrophy_handler = ssm;

  uint32_t max_obj_size = 128;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  //uint8_t num_threads = THREAD_COUNT;
  uint32_t num_procs = 4;
  //uint8_t NUM_THREADS = 4;

  //
	bool am_initializer = false;
  uint32_t proc_number = 0;
  void *com_buffer = nullptr;


  cout << "test_tiers_and_procs: # els: " << els_per_tier << endl;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  key_t com_key = ftok(paths[0],0);
//  key_t randoms_key = ftok(paths[1],0);

  list<uint32_t> lru_keys;
  list<uint32_t> hh_keys;

  for ( uint8_t i = 0; i < num_tiers; i++ ) {
    key_t t_key = ftok(paths[2],i);
    key_t h_key = ftok(paths[3],i);
    lru_keys.push_back(t_key);
    hh_keys.push_back(h_key);
  }
  //
  //
  int status = ssm->region_intialization_ops(lru_keys, hh_keys, true,
                                  num_procs, num_tiers, els_per_tier, max_obj_size, com_key);

  g_ssm_catostrophy_handler = ssm;

  if ( status != 0 ) {
    cout << "region_intialization_ops failed" << endl;
  }
  //
  com_buffer = ssm->_com_buffer;
  //
	map<key_t,void *> &lru_segs = ssm->_seg_to_lrus;
  map<key_t,void *> &hh_table_segs = ssm->_seg_to_hh_tables;
	map<key_t,size_t> &seg_sizes = ssm->_ids_to_seg_sizes;
  //
  void *random_segs[num_tiers];
  for ( int i = 0; i < num_tiers; i++ ) {
    random_segs[i] = new uint32_t[260*4];
  }


  TierAndProcManager<4,3,LRU_cache_mock> *g_tiers_procs_mock = nullptr;


  g_tiers_procs_mock = new TierAndProcManager<4,3,LRU_cache_mock>(com_buffer,lru_segs,
                                              hh_table_segs,seg_sizes,am_initializer,proc_number,
                                                num_procs,num_tiers,els_per_tier,max_obj_size,random_segs);

  uint32_t P = g_tiers_procs_mock->_Procs;
  com_or_offset **messages_reserved = new com_or_offset *[P];
  com_or_offset **duplicate_reserved = new com_or_offset *[P];

  for ( uint8_t j = 0; j < num_tiers; j++ ) {
		g_tiers_procs_mock->_thread_running[j] = true;
  }

  thread ender([&](){
    usleep(1);
    cout << "hit em now" << endl;
    g_tiers_procs_mock->wake_up_write_handlers(0);
  });

  if ( g_tiers_procs_mock->status() ) {
    g_tiers_procs_mock->wait_for_data_present_notification(0);
  }

  ender.join();
  cout << "done" << endl;

  // second_phase_waiter(queue<uint32_t> &ready_procs,uint16_t proc_count,uint8_t assigned_tier)
/*
    CLEAR_FOR_WRITE,	// unlocked - only one process will write in this spot, so don't lock for writing. Just indicate that reading can be done
    CLEARED_FOR_ALLOC,	// the current process will set the atomic to CLEARED_FOR_ALLOC
    LOCKED_FOR_ALLOC,	// a thread (process) that picks up the reading task will block other readers from this spot
    CLEARED_FOR_COPY,	// now let the writer copy the message into storage

*/
  atomic<COM_BUFFER_STATE> *read_marker = g_tiers_procs_mock->get_read_marker(0, 0);
  auto rm = read_marker->load();
  cout << "read marker: " << rm << " " << ((rm == CLEAR_FOR_WRITE) ? "CLEAR_FOR_WRITE" : "mismatch") << endl;
  //
  cleared_for_alloc(read_marker);
  rm = read_marker->load();
  cout << "read marker: " << rm << " " << ((rm == CLEARED_FOR_ALLOC) ? "CLEARED_FOR_ALLOC" : "mismatch") << endl;
  //
  claim_for_alloc(read_marker);
  rm = read_marker->load();
  cout << "read marker: " << rm << " " << ((rm == LOCKED_FOR_ALLOC) ? "LOCKED_FOR_ALLOC" : "mismatch") << endl;
  //
  clear_for_copy(read_marker);
  rm = read_marker->load();
  cout << "read marker: " << rm << " " << ((rm == CLEARED_FOR_COPY) ? "CLEARED_FOR_COPY" : "mismatch") << endl;
  //
  clear_for_write(read_marker);
  rm = read_marker->load();
  cout << "read marker: " << rm << " " << ((rm == CLEAR_FOR_WRITE) ? "CLEAR_FOR_WRITE" : "mismatch") << endl;
  //



  thread ender2([&](){
    usleep(10);
    cout << "hit em with cleared_for_alloc" << endl;
    cleared_for_alloc(read_marker);

    atomic<COM_BUFFER_STATE> *read_marker_1 = g_tiers_procs_mock->get_read_marker(1, 0);
    atomic<COM_BUFFER_STATE> *read_marker_2 = g_tiers_procs_mock->get_read_marker(2, 0);
    atomic<COM_BUFFER_STATE> *read_marker_3 = g_tiers_procs_mock->get_read_marker(3, 0);

    cleared_for_alloc(read_marker_1);
    cleared_for_alloc(read_marker_2);
    cleared_for_alloc(read_marker_3);

    g_tiers_procs_mock->wake_up_write_handlers(0);
  });

  if ( g_tiers_procs_mock->status() ) {
    queue<uint32_t> ready_procs;
    g_tiers_procs_mock->second_phase_waiter(ready_procs,num_procs,0);
    cout << "ready_procs: sz: " << ready_procs.size() << endl;
    while ( !ready_procs.empty() ) {
      cout << "\t" << ready_procs.front() << endl;
      ready_procs.pop();
    }
  }

  ender2.join();
  cout << "done again" << endl;

  rm = read_marker->load();
  cout << "read marker: " << rm << " " << ((rm == CLEARED_FOR_ALLOC) ? "CLEARED_FOR_ALLOC" : "mismatch") << endl;

  clear_for_write(read_marker);
  rm = read_marker->load();
  cout << "read marker: " << rm << " " << ((rm == CLEAR_FOR_WRITE) ? "CLEAR_FOR_WRITE" : "mismatch") << endl;



	LRU_cache_mock *lru = g_tiers_procs_mock->access_tier(0);


  cout << "LRU :: reserve: "  << lru->_reserve << endl;

  uint32_t msg_count = 4;
  // bool add;

  cout << "free count: " << lru->free_count() << endl;
  cout << "current count: " << lru->current_count() << endl;

  lru->check_and_maybe_request_free_mem(msg_count,true);    // when add is set to true _memory_requested increments

  cout << "after req free mem" << endl;
  cout << "free count: " << lru->free_count() << endl;
  cout << "current count: " << lru->current_count() << endl;


  auto memreq = lru->free_mem_requested();

  cout << "mem requested: " << memreq << endl;

  // should be on stack
  uint32_t lru_element_offsets[msg_count+1];
  // clear the buffer
  memset((void *)lru_element_offsets,0,sizeof(uint32_t)*(msg_count+1)); 

  // the next thing off the free stack.
  //
  bool mem_claimed = (UINT32_MAX != lru->claim_free_mem(msg_count,lru_element_offsets));

  if ( mem_claimed ) cout << "mem_claimed" << endl;
  else cout << "no mem_claimed" << endl;

  cout << "after mem claimed" << endl;
  cout << "free count: " << lru->free_count() << endl;
  cout << "current count: " << lru->current_count() << endl;



  /*

  thread ender3([&](){
    usleep(10);
    cout << "hit em with cleared_for_alloc" << endl;
    cleared_for_alloc(read_marker);

    atomic<COM_BUFFER_STATE> *read_marker_1 = g_tiers_procs_mock->get_read_marker(1, 0);
    atomic<COM_BUFFER_STATE> *read_marker_2 = g_tiers_procs_mock->get_read_marker(2, 0);
    atomic<COM_BUFFER_STATE> *read_marker_3 = g_tiers_procs_mock->get_read_marker(3, 0);

    cleared_for_alloc(read_marker_1);
    cleared_for_alloc(read_marker_2);
    cleared_for_alloc(read_marker_3);

    g_tiers_procs_mock->wake_up_write_handlers(0);
  });

  if ( g_tiers_procs_mock->status() ) {
    queue<uint32_t> ready_procs;
    g_tiers_procs_mock->second_phase_write_handler(num_procs,messages_reserved,duplicate_reserved,0);
    cout << "ready_procs: sz: " << ready_procs.size() << endl;
    while ( !ready_procs.empty() ) {
      cout << "\t" << ready_procs.front() << endl;
      ready_procs.pop();
    }
  }

  ender2.join();
  cout << "done again" << endl;
*/


  delete[] messages_reserved;
  delete[] duplicate_reserved;

  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}



//
/*
static inline void cleared_for_alloc(atomic<COM_BUFFER_STATE> *read_marker) {
    auto p = read_marker;
    auto current_marker = p->load();
    while(!p->compare_exchange_weak(current_marker,CLEARED_FOR_ALLOC)
                    && ((COM_BUFFER_STATE)(p->load()) != CLEARED_FOR_ALLOC));
}
*/



class Spinlock{
public:
  Spinlock() { unlock(); }

  virtual ~Spinlock(void) { unlock(); }

  void lock(){
    while( flag.test_and_set() ) __libcpp_thread_yield();
  }

  void unlock(){
    flag.clear();
  }

 private:
   std::atomic_flag flag;
 };




void tryout_cpp_example(void) {
    //
    //
    const int N = 16; // four concurrent readers are allowed
    const int loop_n = 40;
    std::atomic<int> cnt(0);
    std::vector<int> data;
    vector<int> what_read_saw;


    Spinlock w_spin;

    bool start = false;
    bool read_start = false;
    
    auto reader = [&]([[maybe_unused]] int id) {
      while ( !read_start ) std::this_thread::sleep_for(1ms);
      while (true) {
          //
          auto count = cnt.fetch_sub(1,std::memory_order_acquire);
          if ( count <= 0 ) {
            count = 0;
            cnt.store(0,std::memory_order_release);
          }

          w_spin.lock();

          //if ( count > 0 ) {
            what_read_saw.push_back(count);
          //}

          // std::cout << ("reader " + std::to_string(id) +
          //               " sees " + std::to_string(*data.rbegin()) + '\n');

          if ( data.size() >= ((loop_n*N) - 1) && ( count <= 0 ) ) {
            w_spin.unlock();
            break;
          }

          w_spin.unlock();
          if ( count == 0 ) {
            __libcpp_thread_yield();
            //std::this_thread::sleep_for(1ms);
          }
  
          // pause
          //std::this_thread::sleep_for(1ms);
      }
    
    };

    auto writer = [&](int id) {
        while ( !start ) std::this_thread::sleep_for(1ms);
        for (int n = 0; n < loop_n; ++n) {

          w_spin.lock();
          data.push_back(n);
          cnt.fetch_add(N + 1);
          std::cout << "writer " << id << " pushed back " << n << '\n';
          w_spin.unlock();

          // pause
          std::this_thread::sleep_for(1ms);
        }
    };

    auto starter = [&]() {
      std::this_thread::sleep_for(1ms);
      read_start = true;
      std::this_thread::sleep_for(10ms);
      start = true;
    };


    std::vector<std::thread> v;
    for (int n = 0; n < N; ++n) {
      v.emplace_back(reader, 2*n+1);
      v.emplace_back(writer, 2*n+2);
    }
    v.emplace_back(starter);


 
    for (auto& t : v)
        t.join();


    cout << "SIZES: " << data.size() << " " << (N*loop_n) << endl;
    cout << "CNT: " << cnt.load() << endl;

    for ( uint32_t j = 0; j < data.size(); j++ ) {
      cout << data[j] << ", "; cout.flush();
    }
    cout << endl;

    cout << "what_read_saw" << endl;
    for ( uint32_t j = 0; j < what_read_saw.size(); j++ ) {
      cout << what_read_saw[j] << ", "; cout.flush();
    }
    cout << endl;

}







void tryout_atomic_counter(void) {
  //

  std::random_device rd;  // a seed source for the random number engine
  std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<uint32_t> ud(1, 64);

  //
  const int N = 16; // four concurrent readers are allowed
  const int loop_n = 200;
  const uint32_t max_cnt = 1000;

  std::atomic<uint32_t> cnt(0);
  vector<int> what_read_saw;

  Spinlock w_spin;

  bool start = false;
  bool read_start = false;
  
  auto returner = [&](int id) {
    while ( !read_start ) std::this_thread::sleep_for(1ms);
    while (true) {
        //
        auto count = cnt.fetch_add(1,std::memory_order_acquire);
        if ( count >= max_cnt ) {
          count = max_cnt;
          cnt.store(count,std::memory_order_release);
        }

        w_spin.lock();
          what_read_saw.push_back((int)(-id));
          what_read_saw.push_back((int)count);
        w_spin.unlock();
        //
        if ( count >= max_cnt ) {
          cout << "Thread id (" << id << ") quitting" << endl;
          break;
          //std::this_thread::sleep_for(1ms);
        }

        // pause
        //std::this_thread::sleep_for(1ms);
    }
  
  };

  auto requester = [&]([[maybe_unused]] int id) {
    while ( !start ) std::this_thread::sleep_for(1ms);
    uint32_t breakout = 0;
    for (int n = 0; n < loop_n; n++ ) {
      //
      uint32_t decr = ud(gen);
      //
      uint32_t current = cnt.load(std::memory_order_acquire);
      if ( (decr > 0) && ( current >= decr ) ) {
        auto stored = current;
        do {
          if ( current >= decr ) {
            stored = current - decr;
          } else break;
          while ( !(cnt.compare_exchange_weak(current,stored,std::memory_order_acq_rel)) && (current != stored) );
        } while ( current != stored );
      } else {
        if ( ++breakout > 50000 ) break;
        __libcpp_thread_yield();
      }
      // pause
      std::this_thread::sleep_for(1ms);
    }
  };

  auto starter = [&]() {
    std::this_thread::sleep_for(1ms);
    read_start = true;
    std::this_thread::sleep_for(10ms);
    start = true;
  };


  std::vector<std::thread> v;
  for (int n = 0; n < N; ++n) {
    v.emplace_back(returner, 2*n+1);
    v.emplace_back(requester, 2*n+2);
  }
  v.emplace_back(starter);



  for (auto& t : v)
      t.join();


  cout << "CNT: " << cnt.load() << endl;

  // cout << "what_read_saw" << endl;
  // for ( uint32_t j = 0; j < what_read_saw.size(); j++ ) {
  //   cout << what_read_saw[j] << ", "; cout.flush();
  // }
  // cout << endl;

}




void try_out_relaxed_atomic_counter(void) {
  //
  std::atomic<int> cnt = {0};
  //
  auto f = [&]() {
    for (int n = 0; n < 100; ++n) {
        cnt.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> v;
  for (int n = 0; n < 100; ++n) {
      v.emplace_back(f);
  }
  for (auto& t : v) {
      t.join();
  }
  cout << "Final counter value is " << cnt << endl;

  cout << "..." << endl;

    
  int x[500];
  
  std::atomic<bool> done{false};
  
  auto f1 = [&]() {
    for (int i = 0; i<500; i++) {  x[i]=i; std::cout << "R"; }
    
    //atomic_thread_fence(std::memory_order_release);
    done.store(true, std::memory_order_relaxed);
  };
  
  auto f2 = [&]() {
    while( !done.load(std::memory_order_relaxed) ) {
    // do operations not related with tasks
    }
    atomic_thread_fence(std::memory_order_acquire);

    // bool chck = false;
    for (int i = 0; i<500; i++) { std::cout << "A"; }
    // for (int i = 0; i<500; i++) { 
    //   if ( !chck ) { chck = done.load(std::memory_order_relaxed); i--; cout << "."; cout.flush(); }
    //   else { std::cout << "A" << i; }
    //   }
  };

  thread t1 (f1);
  thread t2 (f2);

  t1.join();
  t2.join();


  cout << endl;
  

}


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



void test_atomic_stack_and_timeout_buffer(void) {
  //
}

int main(int argc, char **argv) {
	//

  if ( noisy_test ) {
    cout << "--->>>  THIS IS A NOISY TEST" << endl;
  }

  // int status = 0;
  auto start = chrono::system_clock::now();


  //std::signal(SIGINT, handle_catastrophic);

	if ( argc == 2 ) {
		cout << argv[1] << endl;
	}

  // butter_bug_test();

  // calc_prob_limis();


  uint32_t nowish = 0;
  const auto right_now = std::chrono::system_clock::now();
  nowish = std::chrono::system_clock::to_time_t(right_now);

  //shared_mem_test_initialization_one_call();

  // ----
  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;

  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;

  cout << (UINT32_MAX - 4294916929) << endl;

  return(0);
}





/*

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <syncstream>
#include <thread>
#include <vector>
 
int main()
{
    constexpr int thread_count{5};
    constexpr int sum{5};
 
    std::atomic<int> atom{0};
    std::atomic<int> counter{0};
 
    auto increment_to_sum = [&](const int id)
    {
        for (int next = 0; next < sum;)
        {
            // each thread is writing a value from its own knowledge
            const int current = atom.exchange(next);
            counter++;
            // sync writing to prevent from interrupting by other threads
            std::osyncstream(std::cout)
                << "Thread #" << id << " (id=" << std::this_thread::get_id()
                << ") wrote " << next << " replacing the old value "
                << current << ".\n";
            next = std::max(current, next) + 1;
        }
    };
 
    std::vector<std::thread> v;
    for (std::size_t i = 0; i < thread_count; ++i)
        v.emplace_back(increment_to_sum, i);
 
    for (auto& tr : v)
        tr.join();
 
    std::cout << thread_count << " threads take "
              << counter << " times in total to "
              << "increment 0 to " << sum << ".\n";
}

*/