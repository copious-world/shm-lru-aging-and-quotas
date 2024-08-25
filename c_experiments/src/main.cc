
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


// test_simple_stack
//
void test_simple_stack(void) {
  //
  Stack_simple_test my_stack;
  uint32_t el_size = 64;
  uint32_t num_els = 100;
  auto need_storage = Stack_simple::check_expected_stack_region_size(el_size, num_els);
  //
  cout << "need_storage: " << need_storage << endl;

  uint8_t *beg = new uint8_t[need_storage];
  uint8_t *end = beg + need_storage;

  my_stack.set_region(beg, end, el_size, true);

  uint8_t *beg2 = new uint8_t[need_storage];
  uint8_t *end2 = beg2 + need_storage;

  my_stack.set_region(beg2, end2, el_size, true);

  my_stack.set_region(beg, end);
  cout << (my_stack.empty() ? "true" : "false") << " :: " << my_stack.count() << endl;

  my_stack.set_region(beg2, end2);
  cout << (my_stack.empty() ? "true" : "false") << " :: " << my_stack.count() << endl;

  //
  my_stack.set_region_and_walk(beg, end);
  //
  my_stack.set_region_and_walk(beg2, end2);


  my_stack.set_region(beg, end);

  uint8_t *stored = nullptr;

  vector<uint8_t *> save_els;

  while ( (stored = my_stack.pop()) != nullptr ) {
    cout << "my_stack.count: " << my_stack.count() << endl;
    save_els.push_back(stored);
  }

  cout << my_stack.count() << " " << (my_stack.empty() ? "true" : "false") << " save_els: " << save_els.size() << endl;
  for ( auto el : save_els ) {
    my_stack.push(el);
    cout << "my_stack.count: " << my_stack.count() << endl;
  }

  cout << my_stack.count() << " " << (my_stack.empty() ? "true" : "false")  << endl;
  my_stack.walk_stack();


  cout << "end test" << endl;
}



void test_toks(void) {
    TokGenerator tg;

    const char *my_file_list[3] = {
                                    "src/node_shm_HH.h", 
                                    "src/shm_seg_manager.h", 
                                    "src/hmap_interface.h"
                                  };;

    tg.set_token_grist_list(my_file_list,3);

    auto tk = tg.gen_slab_index();
    cout << tk << endl;
    tk = tg.gen_slab_index();
    cout << tk << endl;
    tk = tg.gen_slab_index();
    cout << tk << endl;
    tk = tg.gen_slab_index();
    cout << tk << endl;
}



void test_slab_primitives(void) {

  SP_slab_types st = SP_slab_t_4;

  auto p_sv = prev_slab_type(st);
  auto n_sv = next_slab_type(st);
  cout << ((p_sv == SP_slab_t_4) ? "true" : "false") << " " << ((n_sv == SP_slab_t_8) ? "true" : "false") << endl;

  st = n_sv;
  p_sv = prev_slab_type(st);
  n_sv = next_slab_type(st);
  cout << ((p_sv == SP_slab_t_4) ? "true" : "false") << " " << ((n_sv == SP_slab_t_16) ? "true" : "false") << endl;

  st = n_sv;
  p_sv = prev_slab_type(st);
  n_sv = next_slab_type(st);
  cout << ((p_sv == SP_slab_t_8) ? "true" : "false") << " " << ((n_sv == SP_slab_t_32) ? "true" : "false") << endl;

  st = n_sv;
  p_sv = prev_slab_type(st);
  n_sv = next_slab_type(st);
  cout << ((p_sv == SP_slab_t_16) ? "true" : "false") << " " << ((n_sv == SP_slab_t_32) ? "true" : "false") << endl;


  SlabProvider sp;
  uint16_t el_count = 100;

  st = SP_slab_t_4;
  cout << "bytes needed: " << sp.bytes_needed(st) << " max_els: " << (int)sp.max_els(st) 
          << " slab_size_bytes: " << sp.slab_size_bytes(st,el_count) << endl;

  st = next_slab_type(st);
  cout << "bytes needed: " << sp.bytes_needed(st) << " max_els: " << (int)sp.max_els(st) 
          << " slab_size_bytes: " << sp.slab_size_bytes(st,el_count) << endl;

  st = next_slab_type(st);
  cout << "bytes needed: " << sp.bytes_needed(st) << " max_els: " << (int)sp.max_els(st) 
          << " slab_size_bytes: " << sp.slab_size_bytes(st,el_count) << endl;

  st = next_slab_type(st);
  cout << "bytes needed: " << sp.bytes_needed(st) << " max_els: " << (int)sp.max_els(st) 
          << " slab_size_bytes: " << sp.slab_size_bytes(st,el_count) << endl;

  st = next_slab_type(st);
  cout << "bytes needed: " << sp.bytes_needed(st) << " max_els: " << (int)sp.max_els(st) 
          << " slab_size_bytes: " << sp.slab_size_bytes(st,el_count) << endl;

}




void test_initialization(void) {
//  * 		void slab_thread_runner([[maybe_unused]] int i)
/*
*/

  const uint16_t tc = 12;

  uint8_t *slab_com =  new uint8_t[sizeof(sp_comm_events) + sizeof(sp_communication_cell)*tc];

  SlabProvider sp;
  uint16_t el_count = 100;

  cout << "Setting slab communicator" << endl;
  sp.set_slab_communicator(slab_com,tc,1);

  const char *my_file_list[4] = {
                                  "src/node_shm_HH.h", 
                                  "src/shm_seg_manager.h", 
                                  "src/hmap_interface.h",
                                  "src/main.c"
                                };

  sp.set_token_grist_list(my_file_list,4);
  //
  slab_parameters sparms[4];
  SP_slab_types st = SP_slab_t_4;

  cout << "creating slab parameters" << endl;
  for ( int i = 0; i < 4; i++ ) {
    sparms[i].el_count = el_count;
    sparms[i].st = st;
    auto tk = sp.gen_slab_index();
    sparms[i].slab_index = tk;

    auto sz = sp.slab_size_bytes(st,el_count);

    void *a_slab = new uint8_t[sz];
    sparms[i].slab = a_slab;

    st = next_slab_type(st);
  }

  cout << "setting slabs_for_startup" << endl;

  sp.set_slabs_for_startup(sparms,4);

  for ( int i = 0; i < 4; i++ ) {
    st = sparms[i].st;
    key_t slab_index = sparms[i].slab_index;
    uint32_t slab_offset = 0;
    uint16_t bytes_needed = sp.bytes_needed(st);
    uint8_t *buffer = (uint8_t *)sparms[i].slab;

    sp.load_bytes(st, slab_index, slab_offset, buffer, bytes_needed);

    sp.unload_bytes(st, slab_index, slab_offset, buffer, bytes_needed);
    st = next_slab_type(st);
  }
  //

  sp_element spe;

  cout << "expanded: st: " << sparms[0].st <<  " index: "  << sparms[0].slab_index << endl;

  spe._bucket_count = 0;
  spe._slab_index = sparms[0].slab_index;
  spe._slab_type = sparms[0].st;
  spe._slab_offset = sp.init_from_free(spe._slab_index);

  st = sparms[0].st;
  // uint16_t el_count_sub = 100/(1 << (st+1));
  // cout << "expanding: " << el_count_sub << endl;      // init_from_free
  cout << "expanded: st: " << spe._slab_type <<  " index: "  << spe._slab_index << " offset: " << spe._slab_offset << endl;
  //
  sp.expand(spe._slab_type,spe._slab_index,spe._slab_offset,el_count);
  //
  cout << "expanded: st: " << spe._slab_type <<  " index: "  << spe._slab_index << " offset: " << spe._slab_offset << endl;
  //
  sp.contract(spe._slab_type,spe._slab_index,spe._slab_offset,el_count);
  //
  cout << "contracted: st: " << spe._slab_type <<  " index: "  << spe._slab_index << " offset: " << spe._slab_offset << endl;
  //
  cout << "finished" << endl;

}


void test_main_thread_flow(void) {

  test_SlabProvider sp;

  uint8_t tc = 8;


  uint8_t *slab_com =  new uint8_t[sizeof(sp_comm_events) + sizeof(sp_communication_cell)*tc];

  uint16_t el_count = 100;

  cout << "Setting slab communicator" << endl;
  sp.set_slab_communicator(slab_com,tc,1);

  const char *my_file_list[4] = {
                                  "src/node_shm_HH.h", 
                                  "src/shm_seg_manager.h", 
                                  "src/hmap_interface.h",
                                  "src/main.c"
                                };

  sp.set_token_grist_list(my_file_list,4);
  //
  slab_parameters sparms[4];
  SP_slab_types st = SP_slab_t_4;

  cout << "creating slab parameters" << endl;
  for ( int i = 0; i < 4; i++ ) {
    sparms[i].el_count = el_count;
    sparms[i].st = st;
    auto tk = sp.gen_slab_index();
    sparms[i].slab_index = tk;

    auto sz = sp.slab_size_bytes(st,el_count);

    void *a_slab = new uint8_t[sz];
    sparms[i].slab = a_slab;

    st = next_slab_type(st);
  }

  cout << "setting slabs_for_startup" << endl;

  auto sz = sp.slab_size_bytes(st,el_count);
  uint8_t buffer[sz];
  sp.set_test_region(buffer);

  sp.set_slabs_for_startup(sparms,4);

  sp._slab_events->_op = SP_CELL_ADD;
  sp.handle_receive_slab_event();

  sp._slab_events->_op = SP_CELL_REMOVE;
  sp.handle_receive_slab_event();

  //
  uint8_t buffer2[sz];
  sp.set_second_test_region(buffer2);

  //
  sp.set_allocator_role(false);
  sp._slab_events->_op = SP_CELL_REQUEST;
  sp.handle_receive_slab_event();

  //
  sp.set_allocator_role(true);
  sp._slab_events->_op = SP_CELL_REQUEST;
  sp.handle_receive_slab_event();
  
}


void test_slab_threads() {
  //
  // slab_thread_runner
  test_SlabProvider sp;
  test_SlabProvider sp_alloc;

  sp_alloc.set_allocator_role(true);


  uint8_t tc = 2;

  uint8_t *slab_com =  new uint8_t[sizeof(sp_comm_events) + sizeof(sp_communication_cell)*tc];

  uint16_t el_count = 100;

  cout << "Setting slab communicator" << endl;
  sp.set_slab_communicator(slab_com,tc,1);
  sp_alloc.set_slab_communicator(slab_com,tc,1);

  const char *my_file_list[4] = {
                                  "src/node_shm_HH.h", 
                                  "src/shm_seg_manager.h", 
                                  "src/hmap_interface.h",
                                  "src/main.c"
                                };

  sp_alloc.set_token_grist_list(my_file_list,4);
  //
  slab_parameters sparms[4];
  SP_slab_types st = SP_slab_t_4;

  cout << "creating slab parameters" << endl;
  for ( int i = 0; i < 4; i++ ) {
    sparms[i].el_count = el_count;
    sparms[i].st = st;
    auto tk = sp_alloc.gen_slab_index();
    sparms[i].slab_index = tk;

    auto sz = sp_alloc.slab_size_bytes(st,el_count);

    void *a_slab = new uint8_t[sz];
    sparms[i].slab = a_slab;

    st = next_slab_type(st);
  }

  //
  st = SP_slab_t_4;
  auto sz = sp_alloc.slab_size_bytes(st,el_count);

  uint8_t buffer2[sz];
  sp_alloc.set_second_test_region(buffer2);

  thread handler([&](void) {
      for ( int i = 0; i < 3; i++ ) {
        sp.slab_thread_runner(0);
      }
      cout << " thread finished " << endl;
  });


  thread alloc_handler([&](void) {
      sp_alloc.slab_thread_runner(0);
      cout << " thread finished " << endl;
  });

  

  try {
    auto slab_index = sp_alloc.create_slab_and_broadcast(st,el_count);
    for ( int i = 0; i < 4; i++ ) tick();
    sp_alloc.remove_slab_and_broadcast(slab_index,st,el_count);
    for ( int i = 0; i < 4; i++ ) tick();
    sp.request_slab_and_broadcast(st,el_count);
  } catch ( std::runtime_error e ) {
    cout << e.what() << endl;
  }

  handler.join();
  alloc_handler.join();

  // while ( true ) tick();

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


  test_simple_stack();
  test_toks();
  test_slab_primitives();
  test_initialization();
  test_slab_threads();

  // ----
  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;

  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;

  cout << (UINT32_MAX - 4294916929) << endl;

  return(0);
}



