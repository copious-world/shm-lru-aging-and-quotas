
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
static constexpr uint8_t THREAD_COUNT = 64;

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

//static TierAndProcManager<4> *g_tiers_procs = nullptr;

using namespace node_shm;




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


chrono::system_clock::time_point shared_random_bits_test() {

  auto bs = new Random_bits_generator<65000,8>();
  //random_bits_test(*bs);

  for ( int i = 0; i < 8; i++ ) {
    uint32_t *bits_for_test = new uint32_t[bs->_bits.size()+ 4*sizeof(uint32_t)];
    bs->set_region(bits_for_test,i);
    bs->regenerate_shared(i);
  }
  //

	auto start = chrono::system_clock::now();

  for ( uint32_t j = 0; j < 1000; j++ ) {
    for ( uint32_t i = 0; i < 65000; i++ ) {
      bs->pop_shared_bit();
    }
    bs->swap_prepped_bit_regions();
  }

  return start;
}

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





void test_hh_map_creation_and_initialization() {

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


  key_t hh_key = hh_keys.front();
  uint8_t *region = (uint8_t *)(ssm->get_addr(hh_key));
  uint32_t seg_sz = ssm->get_size(hh_key);
  
  cout << "seg_sz: " << seg_sz << endl;

  //
  try {
    HH_map<> *test_hh = new HH_map<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;


    cout << "template value: " << extract_N<HH_map<24>> << endl;    /// extract parameter ????
    cout << "template randoms: " << extract_N2<Random_bits_generator<>> << endl;    /// extract parameter ????


  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}


// test_lru_creation_and_initialization
//
void test_lru_creation_and_initialization() {

  int status = 0;

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

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  status = ssm->region_intialization_ops(lru_keys, hh_keys, true,
                                            num_procs, num_tiers, els_per_tier, max_obj_size, com_key, randoms_key);


  size_t reserve = 0;

  auto check_lru_sz = LRU_cache::check_expected_lru_region_size(max_obj_size, els_per_tier,num_procs);
  auto check_hh_sz = HH_map<>::check_expected_hh_region_size(els_per_tier,num_procs);
  for ( auto p : ssm->_seg_to_lrus ) {
    cout << "LRU SEG SIZE: " << p.first << " .. " <<  ssm->_ids_to_seg_sizes[p.first] << ", " << check_lru_sz << endl;
  }
  for ( auto p : ssm->_seg_to_hh_tables ) {
    cout << " HH SEG SIZE: " << p.first << " .. "  <<  ssm->_ids_to_seg_sizes[p.first] << ", " << check_hh_sz << endl;
  }

  key_t lru_key = lru_keys.front();
  cout << "lru_key: " << lru_key << endl;

  uint8_t *region = (uint8_t *)(ssm->get_addr(lru_key));
  uint32_t seg_sz = ssm->get_size(lru_key);
  
  cout << "seg_sz: " << seg_sz << endl;

  //
  try {
    LRU_cache *lru_c = new LRU_cache(region, max_obj_size, seg_sz, els_per_tier, reserve, num_procs, true, 0);
    cout << lru_c->ok() << endl;
  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}


bool stop_printing_dots = false;
void try_sleep_for() {
   using namespace std::chrono_literals;
 
    cout << "Hello waiter\n" << std::flush;
 
    const auto start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::microseconds(20));
    const auto end = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double, std::micro> elapsed = end - start;
 
    stop_printing_dots = true;
    cout << "Waited " << elapsed.count() << endl;
}


void try_spin_for() {
   using namespace std::chrono_literals;
 
    cout << "Hello waiter\n" << std::flush;
 
    const auto start = std::chrono::high_resolution_clock::now();

    int j = 0;
    while ( j++ < 64 );
    auto k = j + 1;
   
    const auto end = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double, std::nano> elapsed = end - start;
 
    stop_printing_dots = true;
    cout << "Waited " << k << " " << elapsed.count() << endl;
}




void test_sleep_methods() {

    thread t1(try_sleep_for);
    int i = 0;
    while ( !(stop_printing_dots) )  {
      i++;
      if ( i > 10000000 ) i = 0;
      if ( !(i%1000000) ) {
        cout << '.'; cout.flush();
      }
    }
    t1.join();


    thread t2(try_spin_for);
    i = 0;
    stop_printing_dots = false;
    while ( !(stop_printing_dots) )  {
      i++;
      if ( i > 10000000 ) i = 0;
      if ( !(i%1000000) ) {
        cout << '.'; cout.flush();
      }
    }
    t2.join();


    cout << (UINT32_MAX - 1) << endl;
    cout << (UINT64_MAX - 1) << endl;
    //
    uint32_t x = UINT32_MAX;
    cout << (WORD - CLZ(x)) << " :: " <<  CLZ(x) << endl;
    uint64_t y = UINT32_MAX;
    cout << (BIGWORD - CLZ(y)) << " :: " <<  CLZ(y)  << endl;
    cout << " --- " << endl;
    x = 10000000;
    cout << x << " ... " << (WORD - CLZ(x)) << " :: " <<  CLZ(x) << endl;
    x = 4000000;
    cout << x << " ... "  << (WORD - CLZ(x)) << " :: " <<  CLZ(x) << endl;
    x = 800000000;
    cout << x << " ... "  << (WORD - CLZ(x)) << " :: " <<  CLZ(x) << endl;


}




static HH_map<> *sg_share_test_hh = nullptr;

int done_cntr = 0;

void hash_counter_bucket_access(void) {
  	HHash *T = nullptr;
		hh_element *buffer = nullptr;
		hh_element *end = nullptr;
		uint8_t which_table = 0;
    // 
    for ( uint16_t j =  0; j < 1000; j++ ) {
      if ( sg_share_test_hh ) {
        if ( sg_share_test_hh->wait_if_unlock_bucket_counts(20,1,&T,&buffer,&end,which_table) ) {
          //
          int i = 0; while ( i < 100 ) i++;
          //
          sg_share_test_hh->slice_bucket_count_incr_unlock(20,which_table,1); // 
        }
      }
    }
    //cout << "finished thread: " << done_cntr++ << endl;
}





void test_hh_map_operation_initialization_linearization() {

  int status = 0;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  uint32_t max_obj_size = 128;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint32_t num_procs = 10;
  uint32_t num_threads = num_procs;

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


  key_t hh_key = hh_keys.front();
  uint8_t *region = (uint8_t *)(ssm->get_addr(hh_key));
  uint32_t seg_sz = ssm->get_size(hh_key);
  
  if ( noisy_test ) cout << "seg_sz: " << seg_sz << endl;

  //
  try {
    //
    HH_map<> *test_hh = new HH_map<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;
    //
    uint8_t *r_region = (uint8_t *)(ssm->get_addr(randoms_key));
    // uint32_t r_seg_sz = ssm->get_size(randoms_key);
    //
    test_hh->set_random_bits(r_region);

    sg_share_test_hh = test_hh;

    if ( noisy_test ) {
      for ( int i = 0; i < 100; i++ ) {
        auto bit = sg_share_test_hh->pop_shared_bit();
        cout << (bit ? "1" : "0"); cout.flush();
      } 
      cout << endl;
    }

    thread *testers[num_threads];
    for ( uint32_t i = 0; i < num_threads; i++ ) {
      testers[i] = new thread(hash_counter_bucket_access);   // uses default consts for els_per_tier,thread_id
    }

    for ( uint32_t i = 0; i < num_threads; i++ ) {
      testers[i]->join();
    }

    if ( noisy_test ) {
        uint8_t count1;
        uint8_t count2;
        uint32_t controls;
        sg_share_test_hh->bucket_counts_lock(20,controls,1,count1,count2);
        cout << "counts: " << (int)count1 << " :: " << (int)count2 << endl;
    }

  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}







uint32_t my_zero_count[256][2048];
uint32_t my_false_count[256][2048];


// thread func

void hash_counter_bucket_access_many_buckets_random(uint32_t num_elements,uint8_t thread_num) {
  	HHash *T = nullptr;
		hh_element *buffer = nullptr;
		hh_element *end = nullptr;
		uint8_t which_table = 0;
    //
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<uint32_t> ud(0, num_elements-1);
    //
    for ( uint32_t j =  0; j < 15000; j++ ) {
      if ( sg_share_test_hh ) {
        uint32_t h_bucket = ud(gen_v);
        my_zero_count[thread_num][h_bucket]++;
        //
        if ( sg_share_test_hh->wait_if_unlock_bucket_counts(h_bucket,thread_num,&T,&buffer,&end,which_table) ) {
          //
          int i = 0; while ( i < 100 ) i++;
          //
          sg_share_test_hh->slice_bucket_count_incr_unlock(h_bucket,which_table,thread_num);
        } else {
          my_false_count[thread_num][h_bucket]++;
        }
      }
    }
    //cout << "finished thread: " << done_cntr++ << endl;
}


// thread func

void hash_counter_bucket_access_many_buckets(uint32_t num_elements,uint8_t thread_num) {
  	HHash *T = nullptr;
		hh_element *buffer = nullptr;
		hh_element *end = nullptr;
		uint8_t which_table = 0;
    //
    uint32_t  bucket_counter = 0;
    uint8_t skip = 1;
    for ( uint32_t j =  0; j < 15000; j++ ) {
      if ( sg_share_test_hh ) {
        //
        bucket_counter = ((bucket_counter + skip) >= num_elements) ? 0 : bucket_counter;
        uint32_t h_bucket = bucket_counter += skip;
        //
        if ( sg_share_test_hh->wait_if_unlock_bucket_counts(h_bucket,thread_num,&T,&buffer,&end,which_table) ) {
          //
          int i = 0; while ( i < 100 ) i++;
          //
          sg_share_test_hh->slice_bucket_count_incr_unlock(h_bucket,which_table,thread_num);
        } else {
          my_false_count[thread_num][h_bucket]++;
        }
      }
    }
    //cout << "finished thread: " << done_cntr++ << endl;
}



// thread func

void hash_counter_bucket_access_many_buckets_primitive(uint32_t num_elements,uint8_t thread_num) {
  	// HHash *T = nullptr;
		// hh_element *buffer = nullptr;
		// hh_element *end = nullptr;
		//uint8_t which_table = 0;
    //
    uint32_t  bucket_counter = 0;
    uint8_t skip = 1;
    if ( sg_share_test_hh ) {
      for ( uint32_t j =  0; j < 15000; j++ ) {
          //
          bucket_counter = ((bucket_counter + skip) >= num_elements) ? 0 : bucket_counter;
          uint32_t h_bucket = bucket_counter += skip;
          //
          uint8_t count1, count2;
          uint32_t controls;

          atomic<uint32_t> *ui = sg_share_test_hh->bucket_counts_lock(h_bucket,controls,thread_num,count1,count2);
          if ( ui != nullptr ) {
            //
            int i = 0; while ( i < 100 ) i++;
            //
            sg_share_test_hh->store_unlock_controller(ui,thread_num);
          }
      }
    }
    //cout << "finished thread: " << done_cntr++ << endl;
}



// thread func

void hash_counter_bucket_access_many_buckets_shared_incr(uint32_t num_elements,uint8_t thread_id) {
  	// HHash *T = nullptr;
		// hh_element *buffer = nullptr;
		// hh_element *end = nullptr;
		//uint8_t which_table = 0;
    //
    uint32_t  bucket_counter = 0;
    uint8_t skip = 1;
    if ( sg_share_test_hh ) {
      for ( uint32_t j =  0; j < 15000; j++ ) {
          bucket_counter = (bucket_counter >= num_elements) ? 0 : bucket_counter;
          uint32_t h_bucket = bucket_counter;
          //
          uint8_t count1, count2;
          uint32_t controls;
          atomic<uint32_t> *ui = sg_share_test_hh->bucket_counts_lock(h_bucket,controls,thread_id,count1,count2);
          if ( ui != nullptr ) {
            //
            int i = 0; while ( i < 100 ) i++;
            //
            sg_share_test_hh->bucket_count_incr(ui,thread_id);
          }
          bucket_counter += skip;
      }
    }
    //cout << "finished thread: " << done_cntr++ << endl;
}




void test_hh_map_operation_initialization_linearization_many_buckets() {

  int status = 0;



  memset(my_zero_count,0,2048*256*sizeof(uint32_t));

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  g_ssm_catostrophy_handler = ssm;

  uint32_t max_obj_size = 128;

  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint8_t num_threads = THREAD_COUNT;
  uint32_t num_procs = num_threads;

  cout << "test_hh_map_operation_initialization_linearization_many_buckets: # els: " << els_per_tier << endl;


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


  key_t hh_key = hh_keys.front();
  uint8_t *region = (uint8_t *)(ssm->get_addr(hh_key));
  uint32_t seg_sz = ssm->get_size(hh_key);
  uint32_t expected_sz = HH_map<>::check_expected_hh_region_size(els_per_tier,num_procs);

  if ( noisy_test ) cout << "seg_sz: " << seg_sz << "  " <<  expected_sz << endl;

  //
  try {
    HH_map<> *test_hh = new HH_map<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;

    //
    uint8_t *r_region = (uint8_t *)(ssm->get_addr(randoms_key));
    // uint32_t r_seg_sz = ssm->get_size(randoms_key);
    //
    test_hh->set_random_bits(r_region);

    sg_share_test_hh = test_hh;


    thread *testers[256];
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    for ( int i = 0; i < num_threads; i++ ) {
      uint8_t thread_id = (i + 1);
      testers[i] = new thread(hash_counter_bucket_access_many_buckets_shared_incr,els_per_tier/2,thread_id);
    }
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    for ( int i = 0; i < num_threads; i++ ) {
      testers[i]->join();
    }
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    
    if ( noisy_test ) {
      cout << "reading bucket counts" << endl;
      uint32_t nn = els_per_tier/2;
      for ( uint32_t i = 0; i < nn; i++ ) {
        uint8_t count1;
        count1 = sg_share_test_hh->get_buckt_count(i);
        cout << "combined count: " << (int)count1 << endl;
        pair<uint8_t,uint8_t> p = sg_share_test_hh->get_bucket_counts(i);
        cout << "odd bucket: " << (int)(p.first) << " :: " << (int)(p.second) << endl;
      }
    }
  

  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}







// thread func

void hash_counter_bucket_access_try_a_few(uint32_t num_elements,uint8_t thread_id) {
  	// HHash *T = nullptr;
		// hh_element *buffer = nullptr;
		// hh_element *end = nullptr;
		//uint8_t which_table = 0;
    //
    uint32_t  bucket_counter = 0;
    uint8_t skip = 1;
    if ( sg_share_test_hh ) {
      uint32_t N = 10;
      for ( uint32_t j =  0; j < N; j++ ) {
          bucket_counter = (bucket_counter >= num_elements) ? 0 : bucket_counter;
          uint32_t h_bucket = bucket_counter;
          //
          uint8_t count1, count2;
          uint32_t controls;
          atomic<uint32_t> *ui = sg_share_test_hh->bucket_counts_lock(h_bucket,controls,thread_id,count1,count2);
          if ( ui != nullptr ) {
            //
            int i = 0; while ( i < 100 ) i++;
            //
            sg_share_test_hh->bucket_count_incr(ui,thread_id);
          } else {
            sg_share_test_hh->bucket_count_incr(h_bucket,thread_id);
          }
          bucket_counter += skip;
      }
    }
    cout << "finished thread: " << done_cntr++ << endl;
}


void handle_catastrophic(int signal) {
  gSignalStatus = signal;
  if ( g_ssm_catostrophy_handler != nullptr ) {
    g_ssm_catostrophy_handler->detach_all(true);
    exit(0);
  }
}

void test_hh_map_operation_initialization_linearization_small_noisy() {

  int status = 0;

  memset(my_zero_count,0,2048*256*sizeof(uint32_t));

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  uint32_t max_obj_size = 128;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint32_t num_procs = 4;
  uint8_t NUM_THREADS = 4;

  cout << "test_hh_map_operation_initialization_linearization_small_noisy: # els: " << els_per_tier << endl;


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

  g_ssm_catostrophy_handler = ssm;

  key_t hh_key = hh_keys.front();
  uint8_t *region = (uint8_t *)(ssm->get_addr(hh_key));
  uint32_t seg_sz = ssm->get_size(hh_key);
  uint32_t expected_sz = HH_map<>::check_expected_hh_region_size(els_per_tier,num_procs);

  if ( noisy_test ) cout << "seg_sz: " << seg_sz << "  " <<  expected_sz << endl;


  //
  try {
    HH_map<> *test_hh = new HH_map<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;

    //
    uint8_t *r_region = (uint8_t *)(ssm->get_addr(randoms_key));
    // uint32_t r_seg_sz = ssm->get_size(randoms_key);
    //
    test_hh->set_random_bits(r_region);

    sg_share_test_hh = test_hh;


    thread *testers[256];
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    for ( int i = 0; i < NUM_THREADS; i++ ) {
      uint8_t thread_id = (i + 1);
      testers[i] = new thread(hash_counter_bucket_access_try_a_few,els_per_tier/2,thread_id);
    }
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    for ( int i = 0; i < NUM_THREADS; i++ ) {
      testers[i]->join();
    }
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    
    cout << "reading bucket counts" << endl;
    uint32_t nn = 10; //els_per_tier/2;   // same as loop constant in thread
    for ( uint32_t i = 0; i < nn; i++ ) {
      uint8_t count1;
      count1 = sg_share_test_hh->get_buckt_count(i);
      cout << "combined count: " << (int)count1 << endl;
      pair<uint8_t,uint8_t> p = sg_share_test_hh->get_bucket_counts(i);
      cout << "odd bucket: " << (int)(p.first) << " :: " << (int)(p.second) << endl;
    }

  

  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}


void test_method_checks() {

  int status = 0;

  memset(my_zero_count,0,2048*256*sizeof(uint32_t));

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  uint32_t max_obj_size = 128;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint32_t num_procs = 4;


  cout << "test_method_checks: # els: " << els_per_tier << endl;


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

  //  region initialization
  status = ssm->region_intialization_ops(lru_keys, hh_keys, true,
                                  num_procs, num_tiers, els_per_tier, max_obj_size,  com_key, randoms_key);

  g_ssm_catostrophy_handler = ssm;

  key_t hh_key = hh_keys.front();
  uint8_t *region = (uint8_t *)(ssm->get_addr(hh_key));
  uint32_t seg_sz = ssm->get_size(hh_key);
  uint32_t expected_sz = HH_map<>::check_expected_hh_region_size(els_per_tier,num_procs);

  if ( noisy_test ) cout << "seg_sz: " << seg_sz << "  " <<  expected_sz << endl;

  uint8_t NUM_THREADS = 4;
  //
  try {

    //  HH_map sg_share_test_hh
    HH_map<> *test_hh = new HH_map<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;
    //
    sg_share_test_hh = test_hh;

    //
    uint8_t *r_region = (uint8_t *)(ssm->get_addr(randoms_key));
    //
    test_hh->set_random_bits(r_region);

    uint32_t controls = 0;
    uint32_t thread_id = 63 - 6;
    uint32_t lock_on_controls = test_hh->bitp_stamp_thread_id(controls,thread_id) | HOLD_BIT_SET;

    cout << "thread_id:\t\t" << bitset<32>{thread_id} << endl;
    cout << "lock_on_controls:\t"  << bitset<32>{lock_on_controls} << endl;
    controls = lock_on_controls;

    cout << "check_thread_id:\t"  << test_hh->check_thread_id(controls,lock_on_controls) << endl;

    controls |= 1;
    cout << "check_thread_id:\t"  << test_hh->check_thread_id(controls,lock_on_controls) << endl;
    controls &= ~1;
    cout << "controls:\t"   << bitset<32>{controls} << endl;

    controls = test_hh->bitp_clear_thread_stamp_unlock(controls);
    cout << "controls:\t"   << bitset<32>{controls} << endl;
    cout << "check_thread_id:\t"  << test_hh->check_thread_id(controls,lock_on_controls) << endl;




    cout << " << test_method_checks << -----------------------> "  << endl;


  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;
}




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
  cout << "butter_bug test walk_struct: " << (int)bb_tmp->_kv.key << "     " << dur_2.count() << " seconds" << endl;

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
    bb_tmp->_kv.key = ii;
    bb_tmp++;
    if ( bb_tmp >= bb_end ) bb_tmp = (hh_element *)butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_struct_n_store: " << bb_tmp->_kv.key << "     " << dur_2.count() << " seconds" << endl;

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

  n = sizeof(uint32_t)*8;
  uint32_t tst_i = DOUBLE_COUNT_MASK_BASE;
  cout << "DOUBLE_COUNT_MASK_BASE:  " << bitset<32>{tst_i} << endl;
  tst_i = DOUBLE_COUNT_MASK;
  cout << "DOUBLE_COUNT_MASK:       " << bitset<32>{tst_i} << endl;
  cout << std::hex << DOUBLE_COUNT_MASK_BASE << endl;
  cout << std::hex << DOUBLE_COUNT_MASK << endl;
  tst_i = HOLD_BIT_SET;
  cout << "HOLD_BIT_SET:            " << bitset<32>{tst_i} << endl;
  cout << std::hex << HOLD_BIT_SET << endl;
  tst_i = FREE_BIT_MASK;
  cout << "FREE_BIT_MASK:           " << bitset<32>{tst_i} << endl;
  cout << std::hex << HOLD_BIT_SET << endl;

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
    cout << "test_zero_above:\t" << bitset<32>(HH_map<>::zero_levels[i]) << endl;
  }

  uint32_t test_pattern =  1  | (1 << 4) | (1 << 7) | (1 << 11) | (1 << 16) | (1 << 17) | (1 << 20) | (1 << 21) | (1 << 22) | 
                          (1 << 23) | (1 << 24) | (1 << 26) | (1 << 27) | (1 << 29) | (1 << 30) | (1 << 31);

  cout << "test_pattern:\t\t" << bitset<32>(test_pattern) << endl;

  uint32_t a = test_pattern;
  a &= (~(uint32_t)1);

  while ( a ) {
    cout << countr_zero(a) << endl;
    uint8_t shift =  countr_zero(a);
    auto masked = a & HH_map<>::zero_above(shift);
    cout << "test_mask   :\t\t" << bitset<32>(masked) << endl;
    a &= (~(uint32_t)(1 << shift));
  }

  a = test_pattern;
  a &= (~(uint32_t)1);

  while ( a ) {
    cout << countr_zero(a) << endl;
    uint8_t shift =  countr_zero(a);
    auto masked = test_pattern & HH_map<>::zero_above(shift);
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

  //vb_probe->c_bits = a;
  //vb_probe->taken_spots = b | hbit;
  cout << "test_hole(a):\t\t" << bitset<32>(HH_map<>::zero_above(hole)) << endl;
  a = a & HH_map<>::zero_above(hole);
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
    //swap(time,vb_probe->taken_spots);  // when the element is not a bucket head, this is time... 
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



void test_hh_map_methods(void) {

  int status = 0;

  memset(my_zero_count,0,2048*256*sizeof(uint32_t));

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  g_ssm_catostrophy_handler = ssm;

  uint32_t max_obj_size = 128;

  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint8_t num_threads = THREAD_COUNT;
  uint32_t num_procs = num_threads;

  cout << "test_hh_map_methods: # els: " << els_per_tier << endl;


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


  key_t hh_key = hh_keys.front();
  uint8_t *region = (uint8_t *)(ssm->get_addr(hh_key));
  uint32_t seg_sz = ssm->get_size(hh_key);
  uint32_t expected_sz = HH_map<>::check_expected_hh_region_size(els_per_tier,num_procs);

  if ( noisy_test ) cout << "seg_sz: " << seg_sz << "  " <<  expected_sz << endl;

  //
  try {
    HH_map<> *test_hh = new HH_map<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;

    //
    // uint8_t *r_region = (uint8_t *)(ssm->get_addr(randoms_key));
    // // uint32_t r_seg_sz = ssm->get_size(randoms_key);
    // //
    // test_hh->set_random_bits(r_region);
    // //


    hh_element test_buffer[128];
    memset(test_buffer,0,sizeof(hh_element)*128);

    hh_element *hash_ref = &test_buffer[64];
    hh_element *buffer = test_buffer;
    hh_element *end = test_buffer + 128;
    //
    uint32_t hole = 0;
    //
    hh_element *prev_a = hash_ref - 2;
    prev_a->c_bits = (3 << 1);
    hh_element *prev_b = hash_ref - 5;
    prev_b->c_bits = 0b01001;
    prev_b->taken_spots = 0b01001;
    //
    hash_ref->c_bits = 1;
    hash_ref->taken_spots = 1;
    //
    auto a = prev_b->c_bits;
    uint8_t offset = get_b_offset_update(a);

    cout << (int)(offset) << " :: " << bitset<32>(a) << endl;
    offset = get_b_offset_update(a);
    cout << (int)(offset) << " :: " << bitset<32>(a) <<  " :: " <<  bitset<32>(1 << offset) << endl;

    test_hh->place_back_taken_spots(hash_ref,hole,buffer,end);

    cout << "----" << endl;

    for ( int i = 0; i < 8; i++ ) {
      cout << "(" << i + 58   <<  ")" << bitset<32>(prev_b->c_bits) << "  " << bitset<32>(prev_b->taken_spots) << endl;
      prev_b++;
    }
    cout << "--- t2 ---" << endl;

    prev_b = prev_a - (prev_a->c_bits >> 1);
    cout << bitset<32>(prev_b->c_bits) << "  " << bitset<32>(prev_b->taken_spots) << endl;

    auto c = prev_b->c_bits & ~(0x1);
    offset = countr_zero(c);

    prev_a = prev_b + offset;
    cout << bitset<32>(prev_a->c_bits) << "  " << bitset<32>(prev_a->taken_spots) << endl;


    cout << "--- t3 ---" << endl;
    c = prev_b->c_bits;
    while ( c ) {
      offset = get_b_offset_update(c);
      prev_a = prev_b + offset;
      cout << bitset<32>(prev_a->c_bits) << "  " << bitset<32>(prev_a->taken_spots) << endl;
    }

    cout << "--- t3 -- p2 ---" << endl;
    c = prev_b->taken_spots;
    while ( c ) {
      offset = get_b_offset_update(c);
      prev_a = prev_b + offset;
      cout << bitset<32>(prev_a->c_bits) << "  " << bitset<32>(prev_a->taken_spots) << endl;
    }

    cout << "--- t4 -- + ---" << endl;


    test_hh->remove_back_taken_spots(hash_ref,hole,buffer,end);
    for ( int i = 0; i < 8; i++ ) {
      cout << "(" << i + 58   <<  ")" << bitset<32>(prev_b->c_bits) << "  " << bitset<32>(prev_b->taken_spots) << endl;
      prev_b++;
    }

  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}






void test_hh_map_methods2(void) {

  int status = 0;

  memset(my_zero_count,0,2048*256*sizeof(uint32_t));

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  g_ssm_catostrophy_handler = ssm;

  uint32_t max_obj_size = 128;

  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint8_t num_threads = THREAD_COUNT;
  uint32_t num_procs = num_threads;

  cout << "test_hh_map_methods2: # els: " << els_per_tier << endl;

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
  key_t hh_key = hh_keys.front();
  uint8_t *region = (uint8_t *)(ssm->get_addr(hh_key));
  uint32_t seg_sz = ssm->get_size(hh_key);

  try {
    HH_map<> *test_hh = new HH_map<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;

    hh_element test_buffer[128];
    memset(test_buffer,0,sizeof(hh_element)*128);

    hh_element *hash_ref = &test_buffer[64];
    hh_element *buffer = test_buffer;
    hh_element *end = test_buffer + 128;
    //
    uint32_t hole = 0;
    //
    uint32_t counter = 0xFF;

    hh_element *prev_b = hash_ref - 5;
    prev_b->c_bits = 1;
    prev_b->taken_spots = 1;

    auto a = prev_b->c_bits;
    auto b = prev_b->taken_spots;


    cout << "1(" << (unsigned int)(prev_b - &test_buffer[0]) <<  ")\t" << bitset<32>(prev_b->c_bits) << " :: " << bitset<32>(prev_b->taken_spots) << endl;

    hole = 3;
    hh_element *vb_probe = prev_b + hole;
    vb_probe->c_bits = (hole << 1);
    vb_probe->taken_spots = (counter++ << 16);


    //
    uint32_t hbit = (1 << hole);
    a = a | hbit;
    b =  b | hbit;
    prev_b->c_bits = a;
    prev_b->taken_spots = b;
    // //
    //
    auto c = a ^ b;
    cout << bitset<32>(a) << endl;
    cout << bitset<32>(b) << endl;
    cout << bitset<32>(c) << endl;
    test_hh->place_taken_spots(prev_b,hole,c,buffer,end);

    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    cout << "2(" << (unsigned int)(prev_b - &test_buffer[0]) <<  ")\t" << bitset<32>(prev_b->c_bits) << " :: " << bitset<32>(prev_b->taken_spots) << endl;


    hash_ref->c_bits = 1;
    hash_ref->taken_spots = 1;
    a = 1;
    b = 1;
    //
    c = a ^ b;
    cout << bitset<32>(a) << endl;
    cout << bitset<32>(b) << endl;
    cout << bitset<32>(c) << endl;


    hole = 0;

    test_hh->place_taken_spots(hash_ref,hole,c,buffer,end);
    cout << "3(" << (unsigned int)(prev_b - &test_buffer[0]) <<  ")\t" << bitset<32>(prev_b->c_bits) << " :: " << bitset<32>(prev_b->taken_spots) << endl;
    cout << "3(" << (unsigned int)(hash_ref - &test_buffer[0]) <<  ")\t" << bitset<32>(hash_ref->c_bits) << " :: " << bitset<32>(hash_ref->taken_spots) << endl;

    cout << "CHECK offset and update" << endl;
    auto w = prev_b->taken_spots;
    cout <<  bitset<32>(w) << endl;
    auto w_offset = get_b_offset_update(w);
    cout << "1. w_offset: " << (int)w_offset << " " <<  bitset<32>(w) << endl;
    cout << bitset<32>((prev_b + w_offset)->taken_spots) << endl;
    cout << bitset<32>((prev_b + w_offset)->c_bits) << endl;

    w_offset = get_b_offset_update(w);
    cout << "2. w_offset: " << (int)w_offset << " " <<  bitset<32>(w) << endl;
    cout << bitset<32>((prev_b + w_offset)->taken_spots) << endl;
    cout << bitset<32>((prev_b + w_offset)->c_bits) << endl;

    w_offset = get_b_offset_update(w);
    cout << "3. w_offset: " << (int)w_offset << " " <<  bitset<32>(w) << endl;
    cout << bitset<32>((prev_b + w_offset)->taken_spots) << endl;
    cout << bitset<32>((prev_b + w_offset)->c_bits) << endl;


    cout << endl << endl;


    // put an element into hash_ref position

    hole = 2;
    vb_probe = hash_ref + hole;
    vb_probe->c_bits = (hole << 1);
    vb_probe->taken_spots = (counter++ << 16);

    a = hash_ref->c_bits;
    b = hash_ref->taken_spots;

    hbit = (1 << hole);
    a = a | hbit;
    b =  b | hbit;
    hash_ref->c_bits = a;
    hash_ref->taken_spots = b;
    c = a ^ b;
    test_hh->place_taken_spots(hash_ref,hole,c,buffer,end);


    cout << "4(" << (unsigned int)(hash_ref - &test_buffer[0]) <<  ")\t" << bitset<32>(hash_ref->c_bits) << " :: " << bitset<32>(hash_ref->taken_spots) << endl;

    cout << "CHECK offset and update" << endl;
    w = hash_ref->taken_spots;
    cout <<  bitset<32>(w) << endl;
    w_offset = get_b_offset_update(w);
    cout << "1. w_offset: " << (int)w_offset << " " <<  bitset<32>(w) << endl;
    cout << bitset<32>((hash_ref + w_offset)->taken_spots) << endl;
    cout << bitset<32>((hash_ref + w_offset)->c_bits) << endl;

    w_offset = get_b_offset_update(w);
    cout << "2. w_offset: " << (int)w_offset << " " <<  bitset<32>(w) << endl;
    cout << bitset<32>((hash_ref + w_offset)->taken_spots) << endl;
    cout << bitset<32>((hash_ref + w_offset)->c_bits) << endl;

    w_offset = get_b_offset_update(w);
    cout << "3. w_offset: " << (int)w_offset << " " <<  bitset<32>(w) << endl;
    cout << bitset<32>((hash_ref + w_offset)->taken_spots) << endl;
    cout << bitset<32>((hash_ref + w_offset)->c_bits) << endl;


    cout << "4(" << (unsigned int)(prev_b - &test_buffer[0]) <<  ")\t" << bitset<32>(prev_b->c_bits) << " :: " << bitset<32>(prev_b->taken_spots) << endl;

    cout << endl << endl;

    //
    hh_element *next_a = hash_ref + 6;
    hh_element *next_a_a = hash_ref + 11;
    hh_element *next_a_a_a = hash_ref + 5;

    hole = 0;   // each of these will be at the base
    c = 0;      // neither has a list of memberships 

    next_a->c_bits = 1;
    next_a->taken_spots = 1;
    test_hh->place_taken_spots(next_a,hole,c,buffer,end);
    //
    next_a_a->c_bits = 1;
    next_a_a->taken_spots = 1;
    test_hh->place_taken_spots(next_a_a,hole,c,buffer,end);
    //
    uint32_t value = 345;
    uint32_t el_key = 4774;

    test_hh->place_in_bucket_at_base(next_a_a_a,value, el_key);
    test_hh->place_back_taken_spots(next_a_a_a, 0, buffer, end);

    prev_b = hash_ref - 8;
    for ( int i = 0; i < 32; i++ ) {
      cout << "(" << (i + 56 - 64)  <<  ")\t" << bitset<32>(prev_b->c_bits) << " :: " << bitset<32>(prev_b->taken_spots) << endl;
      prev_b++;
    }

    a = hash_ref->c_bits; // membership mask
    b = hash_ref->taken_spots;
      //
    for ( int i = 0; i < 10; i++ ) {
      hole = i*2;
      vb_probe = hash_ref + hole;
      vb_probe->c_bits = (hole << 1);
      vb_probe->taken_spots = (counter++ << 16);

      //
      uint32_t hbit = (1 << hole);
      a = a | hbit;
      b =  b | hbit;
      hash_ref->c_bits = a;
      hash_ref->taken_spots = b;
      // //
      //
      auto c = a ^ b;
  cout << "Q: " << bitset<32>(c) << " "  << bitset<32>(a) << " " << bitset<32>(b) << " " << endl;
      test_hh->place_taken_spots(hash_ref,hole,c,buffer,end);
    }

    cout << "----" << endl;

    prev_b = hash_ref - 8;
    for ( int i = 0; i < 32; i++ ) {
      cout << "(" << (i + 56 - 64) <<  ")\t" << bitset<32>(prev_b->c_bits) << " :: " << bitset<32>(prev_b->taken_spots) << endl;
      prev_b++;
    }

    cout << "-- remove back --" << endl;

    test_hh->remove_back_taken_spots(hash_ref,2,buffer,end);
    test_hh->remove_back_taken_spots(hash_ref,4,buffer,end);
    test_hh->remove_back_taken_spots(hash_ref,6,buffer,end);
    //
    test_hh->remove_back_taken_spots(next_a,0,buffer,end);
    test_hh->remove_back_taken_spots(next_a_a,0,buffer,end);
    test_hh->remove_back_taken_spots(next_a_a_a,0,buffer,end);
    //

    prev_b = hash_ref - 8;
    for ( int i = 0; i < 32; i++ ) {
      cout << "(" << (i + 56 - 64) <<  ")\t" << bitset<32>(prev_b->c_bits) << " :: " << bitset<32>(prev_b->taken_spots) << endl;
      prev_b++;
    }

  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}








void test_hh_map_methods3(void) {

  int status = 0;

  memset(my_zero_count,0,2048*256*sizeof(uint32_t));

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  g_ssm_catostrophy_handler = ssm;

  uint32_t max_obj_size = 128;

  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint8_t num_threads = THREAD_COUNT;
  uint32_t num_procs = num_threads;

  cout << "test_hh_map_methods3: # els: " << els_per_tier << endl;

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
  key_t hh_key = hh_keys.front();
  uint8_t *region = (uint8_t *)(ssm->get_addr(hh_key));
  uint32_t seg_sz = ssm->get_size(hh_key);

  try {
    HH_map<> *test_hh = new HH_map<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;

    hh_element test_buffer[128];
    memset(test_buffer,0,sizeof(hh_element)*128);

    hh_element test_buffer_copy[128];
    memset(test_buffer_copy,0,sizeof(hh_element)*128);

    hh_element *hash_ref = &test_buffer[64];
    hh_element *buffer = test_buffer;
    hh_element *end = test_buffer + 128;
    //
    uint32_t hole = 0;
    uint32_t counter = 0xFFFD;


    hash_ref->c_bits = 0b11010101111100111101010111110011;
    hash_ref->taken_spots = 0xFFFFFFFF;
    //                         0b11010101111100111101010111110011;
    (hash_ref + 2)->c_bits = 0b10000010100000110000101010000011;
    (hash_ref + 2)->taken_spots = 0xFFFFFFFF;


    uint64_t B = 0b11010101111100111101010111110011;
    uint64_t C = 0b10000010100000110000101010000011;
    uint64_t VV = C;
    VV = B | (VV << 2);
    VV &= UINT32_MAX;

    uint32_t c = (uint32_t)VV;
    uint8_t offset = countr_one(c);

    (hash_ref + offset)->c_bits = 1;
    (hash_ref + offset)->taken_spots = 0xFFFFFFFF;

    cout << bitset<32>(c) << "  "  << (int)offset  << endl;

    VV = VV | (1 << offset);
    VV &= UINT32_MAX;
    c = (uint32_t)VV;

    offset = countr_one(c);
    cout << bitset<32>(c) << "  "  << (int)offset  << endl;


    hh_element *tmp = hash_ref;
    c = hash_ref->c_bits;
    offset = 0;
    while ( c ) {
      c = c & ~(0x1 << offset);
      offset = countr_zero(c);
      tmp = hash_ref + offset;
      tmp->c_bits = offset << 1;
      tmp->taken_spots = (counter-- << 18) | (1 << 16);
    }


    c = (hash_ref + 2)->c_bits;
    offset = 0;
    while ( c ) {
      c = c & ~(0x1 << offset);
      offset = countr_zero(c);
      tmp = (hash_ref + 2) + offset;
      tmp->c_bits = offset << 1;
      tmp->taken_spots = (counter-- << 18) | (1 << 16);
    }


    memcpy(test_buffer_copy,test_buffer,sizeof(hh_element)*128);

    tmp = hash_ref - 1;
    for ( int i = 0; i < 36; i++ ) {
      uint32_t backref = 0;
      if ( !(tmp->c_bits & 1) ) {
        backref = (tmp->c_bits >> 1);
      }
      cout << "(" << (i + 63 - 64) <<  ")\t" << bitset<32>(tmp->c_bits) << " :: " << bitset<32>(tmp->taken_spots) <<  " :: " << backref << endl;
      tmp++;
    }


    uint64_t v_passed = 20;
    uint32_t time = 0xEDCABEEE;

    auto a = hash_ref->c_bits;
    auto b = hash_ref->taken_spots;
		c = a ^ b;

    offset = 0;
    hh_element *vb_probe = nullptr;
    hole = 32;
    //
    offset = test_hh->inner_bucket_time_swaps(hash_ref,hole,v_passed,time, buffer, end);

    cout << "before pop_oldest_full_bucket: " <<  bitset<32>(c) << endl;
    cout << "before pop_oldest_full_bucket: " << (int)offset << endl;
    cout << "before pop_oldest_full_bucket: " << time << endl;


    tmp = hash_ref - 1;
    for ( int i = 0; i < 36; i++ ) {
      uint32_t backref = 0;
      if ( !(tmp->c_bits & 1) ) {
        backref = (tmp->c_bits >> 1);
      }
      cout << "(" << (i + 63 - 64) <<  ")\t" << bitset<32>(tmp->c_bits) << " :: " << bitset<32>(tmp->taken_spots) <<  " :: " << backref << endl;
      tmp++;
    }

    hh_element *tmp2 = &test_buffer_copy[64];

    cout << "------------- ------------- ------------- ------------- ------------- ------------- -------------" << endl;

    tmp = hash_ref - 1;
    tmp2--;
    for ( int i = 0; i < 36; i++ ) {
      uint32_t backref = 0;
      if ( !(tmp->c_bits & 1) ) {
        backref = (tmp->c_bits >> 1);
      }
      cout << "(" << (i + 63 - 64) <<  ")\t" << bitset<32>(tmp->c_bits) << " :: "  << std::hex << tmp->taken_spots <<  " :: " << std::dec << backref << endl;
      cout << "(" << (i + 63 - 64) <<  ")\t" << bitset<32>(tmp2->c_bits) << " :: " << std::hex << tmp2->taken_spots <<  " :: " << std::dec << backref << endl;
      cout << "---" << endl;
      tmp++;
      tmp2++;
    }

    test_hh->pop_oldest_full_bucket(hash_ref, c, v_passed, time, offset, buffer, end);

    tmp = hash_ref - 1;
    tmp2 = &test_buffer_copy[64];
    tmp2--;
    for ( int i = 0; i < 36; i++ ) {
      uint32_t backref = 0;
      if ( !(tmp->c_bits & 1) ) {
        backref = (tmp->c_bits >> 1);
      }
      cout << "(" << (i + 63 - 64) <<  ")\t" << bitset<32>(tmp->c_bits) << " :: "  << std::hex << tmp->taken_spots <<  " :: " << std::dec << backref << endl;
      cout << "(" << (i + 63 - 64) <<  ")\t" << bitset<32>(tmp2->c_bits) << " :: " << std::hex << tmp2->taken_spots <<  " :: " << std::dec << backref << endl;
      cout << "---" << endl;
      tmp++;
      tmp2++;
    }

    cout << endl << endl;


  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}


/**
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

static volatile sig_atomic_t keep_running = 1;

static void sig_handler(int _)
{
    (void)_;
    keep_running = 0;
}

int main(void)
{
    signal(SIGINT, sig_handler);

    while (keep_running)
        puts("Still running...");

    puts("Stopped by signal `SIGINT'");
    return EXIT_SUCCESS;
}
*/


int main(int argc, char **argv) {
	//

  // int status = 0;


  std::signal(SIGINT, handle_catastrophic);

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




  // test 2
  auto start = chrono::system_clock::now();
  // auto start = shared_random_bits_test();

    //test_hh_map_creation_and_initialization();
    //test_lru_creation_and_initialization();
    //
    // test_hh_map_operation_initialization_linearization_many_buckets();
    // test_hh_map_operation_initialization_linearization_small_noisy();
    // test_method_checks();
    // test_sleep_methods();
    // test_some_bit_patterns();

    //test_hh_map_methods2();
    test_hh_map_methods3();

    // 4298720034

    //test_zero_above();


  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;


  return(0);
}



/*
    vector<thread> v;
    for (int n = 0; n < 10; ++n)
        v.emplace_back(f, n);
    for (auto& t : v)
        t.join();
*/





/*



    cout << "sizeof hh_element: " << sizeof(hh_element) << endl;

    uint16_t my_uint = (1 << 7);
    cout << my_uint << " " << (HOLD_BIT_SET & my_uint) << "   "  << bitset<16>(HOLD_BIT_SET) << "   "  << bitset<16>(my_uint) << endl;

    uint16_t a = (my_uint<<1);
    uint16_t b = (my_uint>>1);
    
    cout << countr_zero(my_uint) << " " << countr_zero(a)<< " " << countr_zero(b) << endl;
#ifdef FFS
      cout << FFS(my_uint) << " " << FFS(a)<< " " << FFS(b) << endl;
#endif

    for (const uint8_t i : {0, 0b11111111, 0b00011100, 0b00011101})
        cout << "countr_zero( " << bitset<8>(i) << " ) = "
              << countr_zero(i) << '\n';

#ifdef FFS
    for (const uint8_t i : {0, 0b11111111, 0b00011100, 0b00011101})
        cout << "countr_zero( " << bitset<8>(i) << " ) = "
              << FFS(i) << '\n';
#endif



auto ms_since_epoch(std::int64_t m){
  return std::chrono::system_clock::from_time_t(time_t{0})+std::chrono::milliseconds(m);
}

uint64_t timeSinceEpochMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
).count();


int main()
{
    using namespace std::chrono;
 
    uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::cout << ms << " milliseconds since the Epoch\n";
 
    uint64_t sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    std::cout << sec << " seconds since the Epoch\n";
 
    return 0;
}


milliseconds ms = duration_cast< milliseconds >(
    system_clock::now().time_since_epoch()
);

*/


/*
template<typename T, typename OP>
T manipulate_bit(std::atomic<T> &a, unsigned n, OP bit_op) {
    static_assert(std::is_integral<T>::value, "atomic type not integral");

    T val = a.load();
    while (!a.compare_exchange_weak(val, bit_op(val, n)));

    return val;
}

auto set_bit = [](auto val, unsigned n) { return val | (1 << n); };
auto clr_bit = [](auto val, unsigned n) { return val & ~(1 << n); };
auto tgl_bit = [](auto val, unsigned n) { return val ^ (1 << n); };

int main() {
    std::atomic<int> a{0x2216};
    manipulate_bit(a, 3, set_bit);  // set bit 3
    manipulate_bit(a, 7, tgl_bit);  // toggle bit 7
    manipulate_bit(a, 13, clr_bit);  // clear bit 13
    bool isset = (a.load() >> 5) & 1;  // testing bit 5
}
*/

