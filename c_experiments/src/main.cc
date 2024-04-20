
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



#include "node_shm_HH_for_test.h"

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







class Test_HH : public HH_map<> {

  public:
      Test_HH(uint8_t *region, uint32_t seg_sz, uint32_t max_element_count, uint32_t num_threads, bool am_initializer = false)
          : HH_map<>(region,seg_sz,max_element_count,num_threads,am_initializer) {
      }


		// get an idea, don't try locking or anything  (combined counts)
		uint32_t get_buckt_count(uint32_t h_bucket) {
			uint32_t *controllers = _region_C;
			auto controller = (atomic<uint32_t>*)(&controllers[h_bucket]);
			return get_buckt_count(controller);
		}

		uint32_t get_buckt_count(atomic<uint32_t> *controller) {
			uint32_t controls = controller->load(std::memory_order_relaxed);
			uint8_t counter = (uint8_t)((controls & DOUBLE_COUNT_MASK) >> QUARTER);  
			return counter;
		}



		/**
		 * get_bucket -- bucket probing -- return a whole bucket... (all values)
		*/
		uint8_t get_bucket(uint32_t h_bucket, uint32_t xs[32]) {
			//
			uint8_t selector = 0;
			if ( selector_bit_is_set(h_bucket,selector) ) {
				h_bucket = clear_selector_bit(h_bucket);
			} else return UINT8_MAX;

			//
			hh_element *buffer = (selector ?_region_HV_1 : _region_HV_0 );
			hh_element *end = (selector ?_region_HV_1_end : _region_HV_0_end);
			//
			hh_element *next = bucket_at(buffer,h_bucket);
			next = el_check_end(next,buffer,end);
			auto c = next->c_bits;
			if ( ~(c & 0x1) ) {
				uint8_t offset = (c >> 0x1);
				next -= offset;
				next = el_check_beg_wrap(next,buffer,end);
				c =  next->c_bits;
			}
			//
			uint8_t count = 0;
			hh_element *base = next;
			while ( c ) {
				next = base;
				uint8_t offset = get_b_offset_update(c);				
				next = el_check_end(next + offset,buffer,end);
				xs[count++] = next->_kv.value;
			}
			//
			return count;	// no value  (values will always be positive, perhaps a hash or'ed onto a 0 value)
		}


};





// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------

SharedSegmentsManager *g_ssm_catostrophy_handler = nullptr;
SharedSegmentsTForm<HH_map_test<>> *g_ssm_catostrophy_handler_custom = nullptr;


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
    Test_HH *test_hh = new Test_HH(region, seg_sz, els_per_tier, num_procs, true);
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
        count1 = test_hh->get_buckt_count(i);
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
  if ( g_ssm_catostrophy_handler_custom != nullptr ) {
    g_ssm_catostrophy_handler_custom->detach_all();
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
    Test_HH *test_hh = new Test_HH(region, seg_sz, els_per_tier, num_procs, true);
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
      count1 = test_hh->get_buckt_count(i);
      cout << "combined count: " << (int)count1 << endl;
      pair<uint8_t,uint8_t> p = test_hh->get_bucket_counts(i);
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

  [[maybe_unused]] uint8_t NUM_THREADS = 4;
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

  //vb_probe->c_bits = a;
  //vb_probe->taken_spots = b | hbit;
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






void print_values_elems(hh_element *elems,uint8_t N,uint8_t start_at = 0) {
  for ( int i = 0; i < N; i++ ) {
    cout << "p values: " << elems->_V << " .. (" << (start_at + i) << ")" << endl;
    elems++;
  }
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


    uint8_t test_base_offset = 3;

    /* test 3 */
    hash_ref->c_bits = 0b00010101111100111101010111110111;
    hash_ref->taken_spots = 0xFFFFFFFF;
    //   
    auto tester = (hash_ref + test_base_offset);  
    tester->c_bits = ~(hash_ref->c_bits >> test_base_offset);
    tester->taken_spots = 0xFFFFFFFF;

    cout << "hash ref and test base: " << bitset<32>(hash_ref->c_bits) << " :: " << endl;
    cout << "hash ref and test base: " << bitset<32>(tester->c_bits  << test_base_offset) << " :: " << endl;
    cout << "hash ref and test base: " << bitset<32>(tester->c_bits) << " :: " << endl;

    auto q = hash_ref->c_bits;
    auto r = tester->c_bits;
    cout << "hash ref and test base: " << bitset<32>((r << test_base_offset) & q) << " :: " <<  bitset<32>((r << test_base_offset) | q) << endl;


    uint64_t B = hash_ref->c_bits;
    uint64_t C = tester->c_bits;
    uint64_t VV = C;
    VV = B | (VV << test_base_offset);
    VV &= UINT32_MAX;

    uint32_t c = (uint32_t)VV;
    uint8_t offset = countr_one(c);

    if ( offset < 32 ) {
      //
      (hash_ref + offset)->c_bits = 1;
      (hash_ref + offset)->taken_spots = 0xFFFFFFFF;

      cout <<  "COUNTR ONE FFFFFFFF: " << countr_one((hash_ref + offset)->taken_spots) << endl;

      cout << bitset<32>(c) << "  "  << (int)offset  << endl;

      VV = VV | (1 << offset);
      VV &= UINT32_MAX;
      c = (uint32_t)VV;

      offset = countr_one(c);
      cout << bitset<32>(c) << "  "  << (int)offset  << endl;
      //
    }


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


    c = (hash_ref + test_base_offset)->c_bits;
    offset = 0;
    while ( c ) {
      c = c & ~(0x1 << offset);
      offset = countr_zero(c);
      tmp = (hash_ref + test_base_offset) + offset;
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
    hole = 32;
    //
    offset = test_hh->inner_bucket_time_swaps(hash_ref,hole,v_passed,time, buffer, end, 1);

    cout << "before pop_oldest_full_bucket: " <<  bitset<32>(c) << endl;
    cout << "before pop_oldest_full_bucket: " << (int)offset << endl;
    cout << "before pop_oldest_full_bucket: " << std::hex << time << std::dec << endl;


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

    cout << " a^b "  << " -- " << bitset<32>(c) <<  " offset: " << (int)(offset) << endl;

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

    tmp = buffer;
    uint8_t k = 1;
    while ( tmp < end ) {
      tmp->_V = k++;
      tmp++;
    }
    

    cout << endl << endl;


    cout << "NOW REMOVING THINGS" << endl;

    cout << "hash ref: " << bitset<32>(hash_ref->c_bits) << " " << bitset<32>(hash_ref->taken_spots)  << endl;
    //
    {
      auto hash_base = hash_ref;

      print_values_elems(hash_base,32,64);

      uint32_t a = hash_base->c_bits; // membership mask
      uint8_t nxt = countr_one(a);
      cout << "prev_ref: " << (hash_base+nxt)->_V << " " <<  bitset<32>((hash_base+nxt)->c_bits) <<  " nxt: " << (int)nxt  << endl;
      nxt--;
      hash_ref += nxt;
      cout << "hash_ref: " << hash_ref->_V << " " <<  bitset<32>(hash_ref->c_bits) <<  " nxt: " << (int)nxt  << endl;
      uint32_t b = hash_base->taken_spots;
      //
      hh_element *vb_last = nullptr;

      auto c = a;   // use c as temporary
      cout << bitset<32>(c) << endl;
      //
      if ( hash_base != hash_ref ) {  // if so, the bucket base is being replaced.
        uint32_t offset = (hash_ref - hash_base);
        cout << "offset: " << offset << endl;
        c = c & ones_above(offset);
      }
      cout << bitset<32>(c) << endl;
      //
      a = hash_base->c_bits; // membership mask
      cout << bitset<32>(a) << " " << bitset<32>(b) <<  endl;
      vb_last = test_hh->shift_membership_spots(hash_base,hash_ref,c,buffer,end);
      a = hash_base->c_bits; // membership mask
      b = hash_base->taken_spots;
      cout << bitset<32>(a) << " " << bitset<32>(b) <<  endl;
      if ( vb_last == nullptr ) {
        cout << "REMOVALS: " << "got a null pointer" << endl;
      } else {
        cout << "VB LAST: " << vb_last->_V << " " <<  bitset<32>(vb_last->c_bits) << endl;
      }
      print_values_elems(hash_base,32,64);

			uint8_t nxt_loc = (vb_last - hash_base);   // the spot that actually cleared...

      cout << "nxt_loc LAST: " << (int)(nxt_loc) << " " <<  (int)(vb_last->c_bits >> 1) << endl;

      //
			// vb_probe should now point to the last position of the bucket, and it can be cleared...
			vb_last->c_bits = 0;
			vb_last->taken_spots = 0;
			vb_last->_V = 0;


			// recall, a and b are from the base
			// clear the bits for the element being removed
			UNSET(a,nxt_loc);
			UNSET(b,nxt_loc);
			hash_base->c_bits = a;
			hash_base->taken_spots = b;
      
      
      cout << "BEFORE remove bucket: " <<  bitset<32>(a) << " " <<  bitset<32>(b) << endl;

			c = a ^ b;   // now look at the bits within the range of the current bucket indicating holdings of other buckets.

      cout << "BEFORE remove bucket: " <<  bitset<32>(c) << endl;
			c = c & zero_above(nxt_loc);

      cout << "BEFORE remove bucket: " <<  bitset<32>(c) << endl;

      test_hh->remove_bucket_taken_spots(hash_base,nxt_loc,a,b,buffer,end);

    }


  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

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
    for ( uint32_t j =  0; j < 15000; j++ ) {
      if ( sg_share_test_hh ) {
        uint32_t h_bucket = ud(gen_v);
*/


const auto NEIGHBORHOOD = 32;


uint8_t test_usurp_membership_position(hh_element *hash_ref, uint32_t c_bits, hh_element *buffer,hh_element *end) {
  //
  uint8_t k = 0xFF & (c_bits >> 1);  // have stored the offsets to the bucket head
  cout << "test_usurp_membership_position: " << (int)k << endl;
  //
  hh_element *base_ref = (hash_ref - k);  // base_ref ... the base that owns the spot
  cout << "test_usurp_membership_position: cbits =  " << bitset<32>(base_ref->c_bits) << endl;

  base_ref = el_check_beg_wrap(base_ref,buffer,end);
  UNSET(base_ref->c_bits,k);   // the element has been usurped...
  cout << "test_usurp_membership_position: cbits =  " << bitset<32>(base_ref->c_bits) << endl;
  //
  uint32_t c = 1 | (base_ref->taken_spots >> k);   // start with as much of the taken spots from the original base as possible
  //
  cout << "test_usurp_membership_position(0): c =  " << bitset<32>(c) << endl;
  auto hash_nxt = base_ref + (k + 1);
  for ( uint8_t i = (k + 1); i < NEIGHBORHOOD; i++, hash_nxt++ ) {
    if ( hash_nxt->c_bits & 0x1 ) { // if there is another bucket, use its record of taken spots
      cout << "test_usurp_membership_position(1):  =  " << bitset<32>(hash_nxt->taken_spots << i) << " -> c_bits " << bitset<32>(hash_nxt->c_bits) << endl;
      c |= (hash_nxt->taken_spots << i); // this is the record, but shift it....
      cout << "test_usurp_membership_position(a): c =  " << bitset<32>(c) << endl;
      break;
    } else if ( hash_nxt->_kv.value != 0 ) {  // set the bit as taken
      SET(c,i);
      cout << "test_usurp_membership_position(b): c =  " << bitset<32>(c) << endl;
    }
  }
  hash_ref->taken_spots = c;
  cout << "test_usurp_membership_position(c): c =  " << bitset<32>(c) << endl;
  return k;
}



hh_element *test_seek_next_base(hh_element *base_probe, uint32_t &c, uint32_t &offset_nxt_base, hh_element *buffer, hh_element *end) {
  hh_element *hash_base = base_probe;
  while ( c ) {
    auto offset_nxt = get_b_offset_update(c);
    base_probe += offset_nxt;
    base_probe = el_check_end_wrap(base_probe,buffer,end);
    if ( base_probe->c_bits & 0x1 ) {
      offset_nxt_base = offset_nxt;
      return base_probe;
    }
    base_probe = hash_base;
  }
  return base_probe;
}



void test_seek_min_member(hh_element **min_probe_ref, hh_element **min_base_ref, uint32_t &min_base_offset, hh_element *base_probe, uint32_t time, uint32_t offset, uint32_t offset_nxt_base, hh_element *buffer, hh_element *end) {
  auto c = base_probe->c_bits;		// nxt is the membership of this bucket that has been found
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

cout << "test_seek_min_member: offset_min = " << (int)offset_min << " time .. " << vb_probe->taken_spots << " " << time << endl;

    if ( vb_probe->taken_spots <= time ) {
			time = vb_probe->taken_spots;
      *min_probe_ref = vb_probe;
      *min_base_ref = base_probe;
      min_base_offset = offset_min;
    }
  
cout << "----" << endl;

  }
}




void test_some_bit_patterns_3(void) {   //  ----  ----  ----  ----  ----  ----  ----
  //

  std::random_device rd;  // a seed source for the random number engine
  std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<uint32_t> ud(1, 31);


  std::uniform_int_distribution<uint32_t> ud_wide(1, UINT32_MAX/3);

  //
  uint32_t buffer[128];
  uint32_t cbits[128];
  memset(buffer,0xFF,128*sizeof(uint32_t));
  memset(cbits,0,128*sizeof(uint32_t));


  for ( int j = 0; j < 5; j++ ) {
    uint32_t h_bucket = ud(gen);
    cbits[64 - h_bucket] = 1;
  }

  uint32_t cbit_gen = ud_wide(gen);
  uint32_t cbit_update = cbit_gen;
  uint32_t cbit_accumulate = cbit_gen;
  uint32_t prev_pos = 0;
  for ( int j = 32; j < 64; j++ ) {
    if ( cbits[j] == 1 ) {
      cbits[j] |= cbit_update;
      if ( prev_pos > 0 ) {
        uint32_t shft = (j - prev_pos);
        cbit_accumulate = cbit_accumulate << shft;
      }
      //
      cbit_accumulate |= cbits[j];
      cbit_gen = ud_wide(gen);
      cbit_update = cbit_gen & ~cbit_accumulate;
      prev_pos = j;
    }
  }

  cout << "cbit_accumulate: " << bitset<32>(cbit_accumulate) << endl;

  cbit_accumulate = 0;
  prev_pos = 0;
  for ( int j = 32; j < 64; j++ ) {
    if ( cbits[j]  & 0x1 ) {
      auto stored_bits = cbits[j];
      if ( prev_pos > 0 ) {
        uint32_t shft = (j - prev_pos);
        cbit_accumulate <<= shft;
      }
      cbit_accumulate |= stored_bits;
      cout << "cbits: " << j << " :: " << bitset<32>(stored_bits) << endl;
      prev_pos = j;
    }
  }

  cout << "cbit_accumulate: " << bitset<32>(cbit_accumulate) << endl;


  // 01011111011111110110110011100001
  // 01011111011111110110110011100001
  // 01111110111110011100001001100101

  uint8_t dist_base = 31;
  uint8_t g = NEIGHBORHOOD - 1;
  while ( dist_base ) {
    //
    cout << " DISTANCE TO BASE IS: " << dist_base << endl;
    uint32_t last_view = (g - dist_base);
    //
    uint32_t c = 1 << g;
    cout << bitset<32>(c) << " last_view = " <<  last_view << endl;

    if ( last_view > 0 ) {
      uint32_t *base = &buffer[64];
      uint32_t *cbits_base = &cbits[64];
      //
      uint32_t *viewer = base - last_view;
      uint32_t *cbts_v = cbits_base - last_view;
      uint8_t k = g;      ///  (dist_base + last_view) :: dist_base + (g - dist_base) ::= dist_base + (NEIGHBORHOOD - 1) - dist_base ::= (NEIGHBORHOOD - 1) ::= g
      while ( viewer != base ) {
        cout << " base - viewer:  " << (base - viewer) << endl;
        //
        if ( *cbts_v & 0x1 ) {
          cout << " cbit = " << k << " dist_base = " << (int)dist_base << " last_view = " << last_view << endl;
          auto vbits = *viewer;
          vbits = vbits & ~((uint32_t)0x1 << k);
          *viewer = vbits;
          c = *cbts_v;
          break;
        }
        viewer++; k--; cbts_v++;
      }
      // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
      cout << "c = "  << bitset<32>(c) << endl;
      c = ~c & zero_above(last_view - (g - k)); // these are not the members of the first found bucket, necessarily in range of hole.
      cout << "c = "  << bitset<32>(c) << "  *viewer= " << bitset<32>(*viewer) << endl;
      c = c & *viewer;   // taken bits
      cout << "c = "  << bitset<32>(c) << endl;

      while ( c ) {
        auto base_probe = viewer;
        auto base_cbts = cbts_v;
        auto offset_nxt = get_b_offset_update(c);
        base_probe += offset_nxt;
        base_cbts += offset_nxt;
        if ( *base_cbts & 0x1 ) {
          auto j = k;
          j -= offset_nxt;
          *base_probe = *base_probe & ~((uint32_t)0x1 << j);  // the bit is not as far out
          cout << "members nxt :: c = "  << bitset<32>(c) << "  ~(*base_cbts): " <<  bitset<32>(~(*base_cbts)) << " : " << (int)j << " : " << (int)k << " : " << (int)offset_nxt << endl;
          c = c & ~(*base_cbts);  // no need to look at base probe members anymore ... remaining bits are other buckets
          cout << "members nxt :: c = "  << bitset<32>(c) << endl;
        }
        cout << "members :: c = "  << bitset<32>(c) << endl;
      }


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

  {
    hh_element test_hh_elements[128];
    hh_element *buffer = test_hh_elements;
    hh_element *end = &test_hh_elements[128];

    hh_element *base_probe = &test_hh_elements[64];
    hh_element *base_probe_2 = &test_hh_elements[66];

    base_probe->c_bits = 1;
    base_probe->_V = 1;

    (base_probe + 1)->c_bits = 1 << 1;


    base_probe_2->c_bits = 1;
    base_probe_2->_V = 5;

    uint32_t c = 0b1011;
    uint32_t offset_nxt_base = 0;

    cout << "test seek_next_base" << endl;

    c = ~c;
    base_probe_2 = test_seek_next_base(base_probe, c, offset_nxt_base, buffer, end);

    cout << "offset_nxt_base = " << offset_nxt_base <<  " " << bitset<32>(c) <<  " "  << base_probe_2->_V << endl;

    hh_element *min_probe = nullptr;
    hh_element *min_base = nullptr;


    uint32_t min_base_offset = 2;
    uint32_t offset = 3;

       base_probe->c_bits =   0b0001011;
       base_probe_2->c_bits = 0b11101;
  base_probe->taken_spots = 0b1111111;
 base_probe_2->taken_spots = 0b00001111;

    //
    c = base_probe_2->c_bits;
    uint32_t time = 200;
    //
    base_probe_2 = base_probe;
    for ( int i = 0; i < 10; i++ ) {
      base_probe_2++;
      base_probe_2->taken_spots = --time;
      base_probe_2->_V = 10*i;
    }
    base_probe_2 = &test_hh_elements[66];


    time = 200 - 2;
    //
    test_seek_min_member(&min_probe, &min_base, min_base_offset, base_probe_2, time, offset, offset_nxt_base, buffer, end);
    //
    if ( min_probe != nullptr ) {
      cout << bitset<32>(min_base->c_bits) << " " << bitset<32>(min_base->taken_spots) << endl;
      cout << min_probe->_V  << " " << min_probe->taken_spots << endl;
      cout << min_base_offset << endl;
    }
    //

       base_probe->c_bits =   0b0001011;
       base_probe_2->c_bits = 0b11101;
      base_probe->taken_spots =   0b1111111;
 base_probe_2->taken_spots = 0b1011111111;

    auto hash_ref = base_probe + 1;
    hash_ref->c_bits = (1 << 1);
    auto c_bits = hash_ref->c_bits;

    auto K = test_usurp_membership_position(hash_ref, c_bits,  buffer, end);

    cout << (int)K << endl;
    cout << "bitset<32>(0b1111111 | (0b1011111111 << 1))::  "<< bitset<32>((0b1111111 >> 1) | (0b1011111111 << 1)) << endl;
  }


}












void test_hh_map_for_test_methods(void) {

  int status = 0;

  memset(my_zero_count,0,2048*256*sizeof(uint32_t));

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsTForm<HH_map_test<>> *ssm = new SharedSegmentsTForm<HH_map_test<>>();

  g_ssm_catostrophy_handler_custom = ssm;

  uint32_t max_obj_size = 128;

  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint8_t num_threads = THREAD_COUNT;
  uint32_t num_procs = num_threads;

  cout << "test_hh_map_for_test_methods: # els: " << els_per_tier << endl;

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
    HH_map_test<> *test_hh = new HH_map_test<>(region, seg_sz, els_per_tier, num_procs, true);
    cout << test_hh->ok() << endl;

    uint64_t loaded_value = ((uint64_t)0xFFFFF << 32 | 0x1234);
    auto time = now_time();
    uint32_t back_to_base = 4;   // not an offset, since for the testing, this will just be the ref

    //
    cout << "T0: " << test_hh->_T0 << " buffer: " << test_hh->_T0->buffer << endl;
    cout << "T1: " << test_hh->_T1 << " buffer: " << test_hh->_T1->buffer << endl;

    hh_element *h1 = (hh_element *)(test_hh->_T0 + 1);
    hh_element *h2 = (hh_element *)(test_hh->_T1 + 1);
    //
    cout << "h1: " << h1 << endl;
    cout << "h2: " << h2 << endl;

    test_hh->allocate_hh_element(test_hh->_T0,back_to_base,loaded_value,time);
    test_hh->allocate_hh_element(test_hh->_T1,back_to_base,loaded_value,time+1);

    back_to_base = 6;
    loaded_value = ((uint64_t)0xFFFF7  << 32 | 0x2346);
    test_hh->allocate_hh_element(test_hh->_T0,back_to_base,loaded_value,time+2);
    test_hh->allocate_hh_element(test_hh->_T1,back_to_base,loaded_value,time+3);


    loaded_value = ((uint64_t)0xEEEE7  << 32 | 0x9876);
    time = now_time();


    uint32_t el_key = 0xEEEE7;
    uint32_t h_bucket = 24;
    // ----
    uint32_t value = loaded_value & UINT32_MAX;
    el_key = (loaded_value >> 32) & UINT32_MAX;

    test_hh->add_into_test_storage(nullptr,h_bucket,el_key,value,time,test_hh->_T0->buffer);
    test_hh->add_into_test_storage(nullptr,h_bucket,el_key,value,time,test_hh->_T1->buffer);


    uint32_t el_key_2 = 0xEEEE8;
    uint32_t h_bucket_2 = 29;
    uint32_t value_2 = 0xAE26;


    test_hh->add_into_test_storage(nullptr,h_bucket_2,el_key_2,value_2,time,test_hh->_T0->buffer);
    test_hh->add_into_test_storage(nullptr,h_bucket_2,el_key_2,value_2,time,test_hh->_T1->buffer);


    for ( auto p : *(test_hh->_T0->test_it) ) {
      cout << "T0 BUCKET:: " << p.first  << endl;
      for ( auto p2 : p.second ) {
        cout << "\tBUCKETS " << hex << p2.first << "  " <<  p2.second->_V << dec << endl;
      }
    }

    for ( auto p : *(test_hh->_T1->test_it) ) {
      cout << "T1 BUCKET:: " << p.first  << endl;
      for ( auto p2 : p.second  ) {
        cout << "\tBUCKETS " << hex << p2.first << "  " <<  p2.second->_V << dec << endl;
      }
    }

    uint32_t h_start = 24;
    el_key = (loaded_value >> 32) & UINT32_MAX;

    hh_element *ba = test_hh->bucket_at(test_hh->_T0->buffer,h_start,el_key);
    //
    cout << ba << endl;
    if ( ba != nullptr ) {
        cout << "BA " << hex <<  ba->_V << dec << endl;
    }

    try {

      if ( test_hh-> remove_from_storage(h_start, el_key, test_hh->_T0->buffer) ) {
        cout << "successfully removed" << endl;
      }
      //
      if ( test_hh-> remove_from_storage(h_start, el_key, test_hh->_T0->buffer) ) {
        cout << "successfully removed twice " << endl;
      }

    } catch ( void *err ) {
      cout << "something broke" << endl;
    }

    //

    for ( int i = 0; i < 8; i++ ) {
      cout << std::hex << h1->_V << std::dec << "  --  " << bitset<32>(h1->c_bits)<< endl;
      cout << std::hex << h2->_V << std::dec << "  --  " << bitset<32>(h2->c_bits)<< endl;
      h1++; h2++;
    }

    cout << " get ref, del ref " << endl;
    //
    hh_element *hel = test_hh->get_ref(h_start, el_key, test_hh->_T0->buffer, test_hh->_T0->end);
    //
    cout << "hello .... " << hel << endl;
    //

    h_bucket = stamp_key(h_bucket,1);
    uint32_t big_V = test_hh->get(el_key,h_bucket,1);

    cout << "big_V: " <<  std::hex <<  big_V << std::dec << endl;

    hel = test_hh->get_ref(h_start, el_key, test_hh->_T1->buffer, test_hh->_T1->end);

    cout << "hello delete.... " << hel << endl;
    if ( hel ) {
      cout << "Give em: " << hex << hel->_V << dec << endl;
      test_hh->del_ref(h_start, el_key, test_hh->_T1->buffer, test_hh->_T1->end);
    }

    hel = test_hh->get_ref(h_bucket_2, el_key_2, test_hh->_T1->buffer, test_hh->_T1->end);
    if ( hel ) {
      cout << "Give em: " << hex << hel->_V << dec << endl;
      cout << " now delete " << endl;
      //
      h_bucket_2 = stamp_key(h_bucket_2,1);

      uint32_t v_value = 0xABADFEE;
      test_hh->update(el_key_2,h_bucket_2,v_value,1);
      //
      auto was_stored = test_hh->get(el_key_2,h_bucket_2,1);

      cout << "was_stored: " << hex << was_stored << dec << endl;
      //
      test_hh->del(el_key_2,h_bucket_2,1);
      //
      hel = test_hh->get_ref(h_bucket_2, el_key_2, test_hh->_T1->buffer, test_hh->_T1->end);
      if ( hel ) {
        cout << "WHAT THE " << hel << endl;
      }
    }

    //
    cout << endl << endl;

  } catch ( const char *err ) {
    cout << err << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

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

      cout << " Q : " << q << endl;
      cout << " K : " << k << endl;
      cout << " J : " << j << endl;
  };


  thread th1(primary_runner);
  thread th2(secondary_runner);

  th1.join();
  th2.join();


  for ( int i = 1; i < 600; i++ ) {
    uint64_t j = i-1;
    if ( results[i] - results[j] > 1 ) {
      cout << "oops: (" << i << ")" << results[i] << " " << results[j] << endl;
    }
  }

  cout << "FINISHED" << endl;

} 





/*



*/


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
    //test_hh_map_methods3(); 

    // test_some_bit_patterns_2();  // auto last_view = (NEIGHBORHOOD - 1 - dist_base);
    // test_some_bit_patterns_3();

    //test_hh_map_for_test_methods();

    //test_zero_above();

    // entry_holder_test();

    entry_holder_threads_test();


  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;


  return(0);
}
