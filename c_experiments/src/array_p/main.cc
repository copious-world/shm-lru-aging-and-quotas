
//
/**
 * Executable: `array_p`
 * 
 *  `array_p` or a storage table for array processes.
 * 
 * This is a separate process that communicates with readers and writers via a shared memory section
 * taking commands off of a limited queue. This module provides a simplification of table storage for the two purposes:
 *  1) This module allows for testing through put at the module front end with some sacrifice to reading speed. 
 *  2) This module allows for a first phase deployment into the copious.world service stack for entry level use (small crowd size).
 * 
 * When it comes to getting values, the shared data structure provides a place to write retrieved values for a waiting process/thread.
 * Each thread that requests (reading process) will enqueue a hash and wait for a response in its assigned output cell. Each requesting 
 * thread will write to a queue assigned to a range of the hash. In turn, each service thread will be assigned to a range queue.
 * The thread will pop the queue and fetch data from its particular region (not share/not overlapping). Each request will provide
 * a hash and an index for the requesting thread. The service thread will identify the output cell in which to write the results to 
 * the requesting thread. The requesting thread will receive notification, read the values and clear any remaining flags and exit.
 */


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
#include <array>
#include <unordered_map>

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


#include "../array_p_defs_storage_app.h"

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




/// Internal thread management

thread input_com_threads[20];
thread output_com_threads[20];
thread client_com_threads[20];

static const uint32_t TABLE_SIZE = (20000);

static Storage_ExternalInterfaceQs<THREAD_COUNT,TABLE_SIZE> *g_com = nullptr;

Storage_ExternalInterfaceQs<THREAD_COUNT,TABLE_SIZE> *initialize_com_region(uint8_t client_count,uint8_t service_count,uint8_t q_entry_count) {
  size_t rsiz = ExternalInterfaceQs<TABLE_SIZE>::check_expected_com_region_size(q_entry_count);
  void *data_region = new uint8_t[rsiz];
  Storage_ExternalInterfaceQs<THREAD_COUNT,TABLE_SIZE> *eiq = new Storage_ExternalInterfaceQs<THREAD_COUNT,TABLE_SIZE>(client_count,service_count,data_region,q_entry_count,true);
  return eiq;
}


void launch_threads(void) {

  if ( g_com == nullptr ) {
    cout << "THEADS CANNOT LAUNCH:: g_com is not initialized " << endl;
    exit(0);
  }

  uint8_t t_count = g_com->_thread_count;

  // t_count is the number of threads per section and is the same for gets and puts

  for ( uint8_t i = 0; i < t_count; i++ ) {
    input_com_threads[i] = thread([](uint8_t j){
      if ( g_com != nullptr ) {
        g_com->put_handler(j);
      }
    },i);
  }
  for ( uint8_t i = 0; i < t_count; i++ ) {
    output_com_threads[i] = thread([](uint8_t j){
      if ( g_com != nullptr ) {
        g_com->get_handler(j);
      }
    },i);
  }
}



void await_thread_end(uint8_t t_count,uint8_t client_t_count) {
  for ( uint8_t i = 0; i < t_count; i++ ) {
      input_com_threads[i].join();
      output_com_threads[i].join();
  }
  //
  for ( uint8_t i = 0; i < client_t_count; i++ ) {
    client_com_threads[i].join();
  }
}




/**
 * main ...
 * 
 */

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


  uint32_t nowish = 0;
  const auto right_now = std::chrono::system_clock::now();
  nowish = std::chrono::system_clock::to_time_t(right_now);

  const uint8_t client_count = 2;
  const uint8_t service_count = 8;

  Storage_ExternalInterfaceQs<THREAD_COUNT,TABLE_SIZE> *eiq = initialize_com_region(client_count,service_count,100);
  g_com = eiq;

  launch_threads();
  for ( int i = 0; i < 20; i++ ) tick();

  await_thread_end(8,1);

  // ----
  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;
  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;

  cout << (UINT32_MAX - 4294916929) << endl;

  return(0);
}



