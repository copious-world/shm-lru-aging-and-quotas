
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



//static TierAndProcManager<4> *g_tiers_procs = nullptr;





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

    // while ( counter < countlimit ) {
    //     while ( g_ping_lock.test(memory_order_relaxed) ) ;
    //     //g_ping_lock.wait(true);
    //     ++counter;
    //     f_hit(true,counter);
    //     g_ping_lock.test_and_set();   // set the flag to true
    //     g_ping_lock.notify_one();
    // }
    // g_ping_lock.test_and_set();
    // g_ping_lock.notify_one();

   // cout << "P1: " << counter << " is diff: " << (countlimit - counter) << endl;
}

void a_pong_1() {

    // while ( counter <= countlimit ) {
    //     while ( !(g_ping_lock.test(memory_order_relaxed)) ) g_ping_lock.wait(false);
    //     uint32_t old_counter = counter;
    //     f_hit(false,counter);
    //     g_ping_lock.clear(memory_order_release);
    //     g_ping_lock.notify_one();
    //     if ( counter == countlimit ) {
    //       if ( old_counter < counter ) {
    //         while ( !(g_ping_lock.test_and_set()) ) usleep(1);
    //         f_hit(false,counter);
    //       }
    //       break;
    //     }
    // }
    //cout << "P1-b: " << counter << " is diff: " << (countlimit - counter) << endl;
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


/*
    //atomic<bool> all_tasks_completed{false};
    //
    atomic_flag all_tasks_completed{false};
    atomic<unsigned> completion_count{};
    future<void> task_futures[16];
    atomic<unsigned> outstanding_task_count{16};
 
    // Spawn several tasks which take different amounts of
    // time, then decrement the outstanding task count.
    for (future<void>& task_future : task_futures)
        task_future = async([&]
        {
            // This sleep represents doing real work...
            this_thread::sleep_for(50ms);
 
            ++completion_count;
            --outstanding_task_count;
 
            // When the task count falls to zero, notify
            // the waiter (main thread in this case).
            if (outstanding_task_count.load() == 0)
            {
                //all_tasks_completed = true;
                all_tasks_completed.test_and_set();
                atomic_flag_notify_one( &all_tasks_completed );
            }
        });
 
    all_tasks_completed.wait(false);
 
    cout << "Tasks completed = " << completion_count.load() << '\n';
*/


int main(int argc, char **argv) {
	//

    #if defined(__cpp_lib_atomic_flag_test)

        cout << "THERE REALLY ARE ATOMIC FLAGS" << endl;

    #endif

  #if defined(_GLIBCXX_HAVE_LINUX_FUTEX)
        cout << "There really is a platform wait" << endl;
  #endif

  cout << "size of unsigned long: " << sizeof(unsigned long) << endl;

	if ( argc == 2 ) {
		cout << argv[1] << endl;
	}



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


/*
    vector<thread> v;
    for (int n = 0; n < 10; ++n)
        v.emplace_back(f, n);
    for (auto& t : v)
        t.join();
*/
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

  //
  //

	// if ( (argc == 2) && (strncmp(argv[1],"a1",2) == 0) ) {
  //   //cout << "starting a test" << endl;
  //   //g_ping_lock.clear();
  //   //
  //   //
	// 	thread t1(a_ping_1);
	// 	thread t2(a_pong_1);
  //   //
  //   //usleep(20);
  //   start = chrono::system_clock::now();
  //   //g_ping_lock.notify_all();
	// 	//
	// 	t1.join();
	// 	t2.join();
  // } else if ( (argc == 2) && (strncmp(argv[1],"nada",4) == 0) ) {
  //   cout << "NADA" << endl;
	// } else {
	// 	thread t1(ping);
	// 	thread t2(pong);
	// 	//
	// 	t1.join();
	// 	t2.join();
	// }

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


  return(0);
}








/*

// Following the "mutex2" implementation in Drepper's "Futexes Are Tricky"
// Note: this version works for threads, not between processes.
//
// Eli Bendersky [http://eli.thegreenplace.net]
// This code is in the public domain.
#include <atomic>
#include <cstdint>
#include <iostream>
#include <linux/futex.h>
#include <pthread.h>
#include <sstream>
#include <sys/resource.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>

// An atomic_compare_exchange wrapper with semantics expected by the paper's
// mutex - return the old value stored in the atom.
int cmpxchg(atomic<int>* atom, int expected, int desired) {
  int* ep = &expected;
  atomic_compare_exchange_strong(atom, ep, desired);
  return *ep;
}

class Mutex {
public:
  Mutex() : atom_(0) {}

  void lock() {
    int c = cmpxchg(&atom_, 0, 1);
    // If the lock was previously unlocked, there's nothing else for us to do.
    // Otherwise, we'll probably have to wait.
    if (c != 0) {
      do {
        // If the mutex is locked, we signal that we're waiting by setting the
        // atom to 2. A shortcut checks is it's 2 already and avoids the atomic
        // operation in this case.
        if (c == 2 || cmpxchg(&atom_, 1, 2) != 0) {
          // Here we have to actually sleep, because the mutex is actually
          // locked. Note that it's not necessary to loop around this syscall;
          // a spurious wakeup will do no harm since we only exit the do...while
          // loop when atom_ is indeed 0.
          syscall(SYS_futex, (int*)&atom_, FUTEX_WAIT, 2, 0, 0, 0);
        }
        // We're here when either:
        // (a) the mutex was in fact unlocked (by an intervening thread).
        // (b) we slept waiting for the atom and were awoken.
        //
        // So we try to lock the atom again. We set teh state to 2 because we
        // can't be certain there's no other thread at this exact point. So we
        // prefer to err on the safe side.
      } while ((c = cmpxchg(&atom_, 0, 2)) != 0);
    }
  }

  void unlock() {
    if (atom_.fetch_sub(1) != 1) {
      atom_.store(0);
      syscall(SYS_futex, (int*)&atom_, FUTEX_WAKE, 1, 0, 0, 0);
    }
  }

private:
  // 0 means unlocked
  // 1 means locked, no waiters
  // 2 means locked, there are waiters in lock()
  atomic<int> atom_;
};

// Simple function that increments the value pointed to by n, 10 million times.
// If m is not nullptr, it's a Mutex that will be used to protect the increment
// operation.
void threadfunc(int64_t* n, Mutex* m = nullptr) {
  for (int i = 0; i < 10000000; ++i) {
    if (m != nullptr) {
      m->lock();
    }
    *n += 1;
    if (m != nullptr) {
      m->unlock();
    }
  }
}

int main(int argc, char** argv) {
  {
    int64_t vnoprotect = 0;
    thread t1(threadfunc, &vnoprotect, nullptr);
    thread t2(threadfunc, &vnoprotect, nullptr);
    thread t3(threadfunc, &vnoprotect, nullptr);

    t1.join();
    t2.join();
    t3.join();

    cout << "vnoprotect = " << vnoprotect << "\n";
  }

  {
    int64_t v = 0;
    Mutex m;

    thread t1(threadfunc, &v, &m);
    thread t2(threadfunc, &v, &m);
    thread t3(threadfunc, &v, &m);

    t1.join();
    t2.join();
    t3.join();

    cout << "v = " << v << "\n";
  }

  return 0;
}


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
