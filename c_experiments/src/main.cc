
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

#include <chrono>
#include <vector>

#include <future>
 
#include <bitset>
#include <random>

//#include <linux/futex.h>

#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ipc.h>


using namespace std;
using namespace chrono;
using namespace literals;


// Experiment with atomics for completing hash table operations.


static_assert(sizeof(uint64_t) == sizeof(atomic<uint64_t>), 
    "atomic<T> isn't the same size as T");

static_assert(atomic<uint64_t>::is_always_lock_free,  // C++17
    "atomic<T> isn't lock-free, unusable on shared mem");

// 


// -------- -------- -------- -------- -------- -------- -------- -------- -------- -------- --------



//#include "node_shm_LRU.h"

#include "time_bucket.h"
#include "random_selector.h"
#include "shm_seg_manager.h"

//static TierAndProcManager<4> *g_tiers_procs = nullptr;

using namespace node_shm;




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
  uint32_t num_procs = 4;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;

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

  cout << "Com Buf size: " << ssm->get_seg_size(com_key) << endl;
  cout << "Randoms size: " << ssm->get_seg_size(randoms_key) << endl;
  cout << "Total size: " << ssm->total_mem_allocated() << endl;

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}




int main(int argc, char **argv) {
	//

  // int status = 0;

	if ( argc == 2 ) {
		cout << argv[1] << endl;
	}

  uint32_t nowish = 0; 
  const auto right_now = std::chrono::system_clock::now();
  nowish = std::chrono::system_clock::to_time_t(right_now);

  shared_mem_test_initialization_one_call();

  // ----
  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;

  // test 2
  auto start = chrono::system_clock::now();
  // auto start = shared_random_bits_test();
  
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

