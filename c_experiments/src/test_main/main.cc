
//
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <iostream>
#include <cstring>
#include <string>

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

#include <iostream>
#include <random>
using namespace std;


random_device g_rd;
mt19937 g_gen;
uniform_int_distribution<> g_distrib;

void init_random(int min,int max) {
  mt19937 gen(g_rd());
  g_gen = gen;
  uniform_int_distribution<> distrib(min, max);
  g_distrib = distrib;
}

int get_rand(void) {
    int randomValue = g_distrib(g_gen);
    return randomValue;
}


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

// #include "time_bucket.h"
// #include "random_selector.h"
// #include "shm_seg_manager.h"

// #include "node_shm_tiers_and_procs.h"

#include "../time_bucket.h"
#include "../random_selector.h"
#include "../shm_seg_manager.h"

#include "../node_shm_tiers_and_procs.h"

#include "../atomic_queue.h"

#include "../array_p_defs_storage_app.h"

#include "../mock_shm_LRU.h"

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


static bool using_segments = true;
key_t g_com_key = 38450458;



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
static atomic_flag g_global_shutdown;


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



void test_shared_queue(void) {
  //
  auto sz = AtomicQueue<Basic_q_element>::check_region_size(200);
  auto cnt_atoms = AtomicQueue<Basic_q_element>::atomics_count();
  auto cnt_stck_atoms = AtomicStack<Basic_q_element>::atomics_count();
  auto cnt_q_atoms = (cnt_atoms - cnt_stck_atoms);

  cout << "test_shared_queue: " << sz << endl;
  cout << "TOTAL SHARED ATOMICS: " << (size_t)cnt_atoms << endl;

  cout << "Basic_q_element size: " << sizeof(Basic_q_element) << endl;
  cout << "Basic_q_element size less atomics: "  << (sz - cnt_atoms*(sizeof(atomic<uint32_t>))) << endl;
  cout << "Basic_q_element els: " << ((sz - cnt_atoms*(sizeof(atomic<uint32_t>)))/sizeof(Basic_q_element) - 1) << endl;

  cout << endl;

  uint8_t *region = new uint8_t[sz];

  AtomicQueue<Basic_q_element> qchck;
  auto free_els = qchck.setup_queue_region(region,sizeof(Basic_q_element),sz);


  uint32_t test = 0;
  for ( int i = 0; i < 4; i++ ) {       // note that this usage is for testing only
    auto status = qchck.pop_number(region + cnt_q_atoms*sizeof(atomic<uint32_t>),1,&test);
    cout << "pop_number: " << i << " status: " << status << " offset: " << test << endl;
  }

  cout << "allocated free elements: "  << free_els << endl;
  Basic_q_element bqe;

  for ( int i = 0; i < 4; i++ ) {
    bqe._info = i + 5;
    auto status = qchck.push_queue(bqe);
    cout << " EMPTY? " << (qchck.empty() ? "true" : "false" ) << " status: " << status << endl;
  }

  cout << "completed all pushes" << endl;

  for ( int i = 0; i < 4; i++ ) {
    auto status = qchck.pop_queue(bqe);
    cout << "bqe._info = " << bqe._info << " EMPTY? " << (qchck.empty() ? "true" : "false" ) << " status: " << status << endl;
  }

}


void test_shared_queue_threads(void) {
  //
  auto sz = AtomicQueue<Basic_q_element>::check_region_size(200);
  auto cnt_atoms = AtomicQueue<Basic_q_element>::atomics_count();
  // auto cnt_stck_atoms = AtomicStack<Basic_q_element>::atomics_count();
  // auto cnt_q_atoms = (cnt_atoms - cnt_stck_atoms);

  cout << "test_shared_queue: " << sz << endl;
  cout << "TOTAL SHARED ATOMICS: " << (size_t)cnt_atoms << endl;

  cout << "Basic_q_element size: " << sizeof(Basic_q_element) << endl;
  cout << "Basic_q_element size less atomics: "  << (sz - cnt_atoms*(sizeof(atomic<uint32_t>))) << endl;
  cout << "Basic_q_element els: " << ((sz - cnt_atoms*(sizeof(atomic<uint32_t>)))/sizeof(Basic_q_element) - 1) << endl;

  cout << endl;

  uint8_t *region = new uint8_t[sz];


  AtomicQueue<Basic_q_element> qchck;
  auto free_els = qchck.setup_queue_region(region,sizeof(Basic_q_element),sz);

  cout << "allocated free elements: "  << free_els << endl;


  uint8_t thrd_count = 8;
  thread *all_threads[thrd_count];

  for ( uint8_t t = 0; t < thrd_count; t++ ) {
    thread *a_thread = new thread([&](int j) {
      // use the queue
      Basic_q_element bqe;
      for ( int i = 0; i < 4; i++ ) {
        bqe._info = i + 5*j;
        auto status = qchck.push_queue(bqe);
        cout << " EMPTY? " << (qchck.empty() ? "true" : "false" ) << " status: " << status << endl;
      }

      cout << "completed all pushes" << endl;

      for ( int i = 0; i < 4; i++ ) {
        auto status = qchck.pop_queue(bqe);
        cout << "bqe._info = " << bqe._info << " EMPTY? " << (qchck.empty() ? "true" : "false" ) << " status: " << status << endl;
      }

    },t);
    all_threads[t] = a_thread;
  }


  for ( uint8_t t = 0; t < thrd_count; t++ ) {
    all_threads[t]->join();
  }

}


void test_node_shm_queued(void) {

  uint8_t thrd_count = 8;
  uint8_t client_count = 8;
  // thread *all_threads[thrd_count];

  uint32_t els_per_tier = 200000;
  uint32_t els_per_com_queue = 200;

  auto sz = Storage_ExternalInterfaceQs<8,200>::check_expected_com_region_size( els_per_com_queue );
  //
  cout << "region size: " << sz << endl;
  //
  uint8_t *region = new uint8_t[sz];
  //

  Storage_ExternalInterfaceQs<8,200> q_test(&g_global_shutdown,client_count,thrd_count,region,els_per_tier,true);  // thread count is relevant
  QUEUED_map<200> q_client(region,sz,els_per_tier,client_count);  // one per thread



  //cout << "q_test._proc_refs._put_com->_put_queue._count_free: " << q_test._proc_refs._put_com[0]._put_queue._count_free << endl;
  //cout << "q_client._com._proc_refs._put_com->_put_queue._count_free: " << q_client._com._proc_refs._put_com[0]._put_queue._count_free << endl;

  cout << "region initialized: ..." << endl;

  atomic_flag running;

  running.clear();

  thread *client_thread = new thread([&]([[maybe_unused]] int j) {

    for ( int i = 0; i < 8; i++ ) {
      //
      cout << "add: " << (121 + i) << "," << (200 + i) << endl;
      q_client.add_key_value_known_refs(nullptr,121 + i,UINT32_MAX,200 + i,0,0,0,0,nullptr,nullptr,nullptr,nullptr);

    }

    cout << "wait q 4 empty " << endl;
    while ( !q_client._com._proc_refs._put_com[4]._put_queue.empty() ) tick();
    cout << "wait q 5 empty " << endl;
    while ( !q_client._com._proc_refs._put_com[5]._put_queue.empty() ) tick();


    cout << "start retrieving values: " << endl;

    for ( int i = 0; i < 8; i++ ) {
      //
      auto val = q_client.get(121+i,UINT32_MAX);
      cout << val << endl;
      //
    }

    while ( !q_client._com._proc_refs._get_com[4]._get_queue.empty() ) tick();
    while ( !q_client._com._proc_refs._get_com[5]._get_queue.empty() ) tick();

    running.test_and_set();

  },1);


  thread *server_thread_put_4 = new thread([&](int j) {
    //
    while ( !running.test() ) {
      q_test.put_handler(j);
      tick();
    }
    //
  },4);


  thread *server_thread_put_5 = new thread([&](int j) {
    //
    while ( !running.test() ) {
      q_test.put_handler(j);
      tick();
    }
    //
  },5);



  thread *server_thread_get_4 = new thread([&](int j) {
    //
    while ( !running.test() ) {
      q_test.get_handler(j);
      tick();
    }

    cout << "get thread 4 exit" << endl;
    //
  },4);


  thread *server_thread_get_5 = new thread([&](int j) {
    //
    while ( !running.test() ) {
      q_test.get_handler(j);
      tick();
    }
    cout << "get thread 5 exit" << endl;
    //
  },5);



  client_thread->join();
  server_thread_put_4->join();
  server_thread_put_5->join();
  server_thread_get_4->join();
  server_thread_get_5->join();

}


void front_end_internal_test(void) {
  stp_table_choice tchoice = STP_TABLE_INTERNAL_ONLY;
  uint32_t num_procs = 8;
  uint32_t num_tiers = 3;
  uint32_t proc_number = 1;
  uint32_t els_per_tier = 20000;
  //
  LRU_Alloc_Sections_and_Threads last = TierAndProcManager<>::section_allocation_requirements(tchoice, num_procs, num_tiers);

  auto com_buf_sz = TierAndProcManager<>::check_expected_com_region_size(num_procs,num_tiers);

  uint8_t *com_buffer = new uint8_t[com_buf_sz];
  //
  cout << "front_end_internal_test com_buf_sz: " << com_buf_sz << " com_buffer: " << ((void *)com_buffer) << " com_buffer: " << ((void *)(com_buffer + com_buf_sz)) << endl;
  //
  map<key_t,void *> lru_segs;
  map<key_t,void *> hh_table_segs;
  map<key_t,size_t> seg_sizes;
  bool am_initializer = true;
  uint32_t max_obj_size = 128;
  void **random_segs = nullptr;

  for ( uint32_t t = 0; t < num_tiers; t++ ) {
    seg_sizes[t] = LRU_cache<>::check_expected_lru_region_size(max_obj_size, els_per_tier, num_procs);
    lru_segs[t] = new uint8_t[seg_sizes[t]];
    cout << "lru_segs[t]:: []" << t << "]  " << lru_segs[t] << endl;
  }

cout << "Initialize TierAndProcManager:: " << endl;
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  TierAndProcManager<> tapm(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


cout << "Launch threads... TierAndProcManager:: " << endl;

  tapm.launch_second_phase_threads(last);
  //

  uint32_t hash_bucket = 0;
  uint32_t full_hash = 0;
  bool updating = false;
  unsigned int size = 64;
  char* buffer = new char[size];
  strcpy(buffer,"this is a test");
  uint32_t timestamp = now();


  tapm.put_method(hash_bucket, full_hash, updating, buffer, size, timestamp);

cout << "Shutting down threads... TierAndProcManager:: " << endl;
  //
  tapm.shutdown_threads(last);
}






void front_end_internal_test2(void) {
  //
  stp_table_choice tchoice = STP_TABLE_INTERNAL_ONLY;
  uint32_t num_procs = 8;
  uint32_t num_tiers = 3;
  uint32_t proc_number = 1;
  uint32_t els_per_tier = 20000;
  //
  LRU_Alloc_Sections_and_Threads last = TierAndProcManager<>::section_allocation_requirements(tchoice, num_procs, num_tiers);

  auto com_buf_sz = TierAndProcManager<>::check_expected_com_region_size(num_procs,num_tiers);

  uint8_t *com_buffer = new uint8_t[com_buf_sz];
  //
  cout << "front_end_internal_test com_buf_sz: " << com_buf_sz << " com_buffer: " << ((void *)com_buffer) << " com_buffer: " << ((void *)(com_buffer + com_buf_sz)) << endl;
  //
  map<key_t,void *> lru_segs;
  map<key_t,void *> hh_table_segs;
  map<key_t,size_t> seg_sizes;
  bool am_initializer = true;
  uint32_t max_obj_size = 128;
  void **random_segs = nullptr;

  for ( uint32_t t = 0; t < num_tiers; t++ ) {
    seg_sizes[t] = LRU_cache<>::check_expected_lru_region_size(max_obj_size, els_per_tier, num_procs);
    lru_segs[t] = new uint8_t[seg_sizes[t]];
    cout << "lru_segs[t]:: []" << t << "]  " << lru_segs[t] << endl;
  }

cout << "Initialize TierAndProcManager:: " << endl;
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  TierAndProcManager<> tapm(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


cout << "Launch threads... TierAndProcManager:: " << endl;

  tapm.launch_second_phase_threads(last);
  //

  map<uint32_t,uint32_t> hash_stamp;

  thread input_all([&](){
      uint32_t hash_bucket = 0;
      uint32_t full_hash = 0;
      bool updating = false;
      unsigned int size = 64;
      char* buffer = new char[size];

    string tester = "this is a test";

    for ( int i = 0; i < 8; i++ ) {
      //
      full_hash++;
      hash_bucket++;

      tester += ' ';
      tester += i;
      memset(buffer,0,size);
      strcpy(buffer,tester.c_str());
      uint32_t timestamp = now();
      hash_stamp[full_hash] = timestamp;
      //
      tapm.put_method(hash_bucket, full_hash, updating, buffer, size, timestamp);

    }

  });


  for ( int j = 0; j < 100; j++ ) tick();

  thread get_all([&](){
      uint32_t hash_bucket = 0;
      uint32_t full_hash = 0;
      unsigned int size = 64;
      char* buffer = new char[size];

    string tester = "this is a test";

    for ( int i = 0; i < 8; i++ ) {
      //
      full_hash++;
      hash_bucket++;
      //
      tester += ' ';
      tester += i;
      memset(buffer,0,size);
      strcpy(buffer,tester.c_str());
      uint32_t timestamp = 0;
      //
      while ( timestamp == 0 ) {
        tick();
        timestamp = hash_stamp[full_hash];
      }
      //
      tapm.get_method(hash_bucket, full_hash, buffer, size, timestamp, 0);
      cout << hash_bucket << " -- " << buffer << endl;
      //
    }

  });



  for ( int j = 0; j < 100; j++ ) tick();



  thread remove_all([&](){
      uint32_t hash_bucket = 0;
      uint32_t full_hash = 0;
      unsigned int size = 64;
      char* buffer = new char[size];

    string tester = "this is a test";

    for ( int i = 0; i < 8; i++ ) {
      //
      full_hash++;
      hash_bucket++;
      //
      tester += ' ';
      tester += i;
      memset(buffer,0,size);
      strcpy(buffer,tester.c_str());
      uint32_t timestamp = 0;
      //
      while ( timestamp == 0 ) {
        tick();
        timestamp = hash_stamp[full_hash];
      }
      //
      tapm.del_method(0,hash_bucket, full_hash,timestamp,0);
      //
    }

  });


cout << "Shutting down threads... TierAndProcManager:: " << endl;
  //
  tapm.shutdown_threads(last);
}




void front_end_internal_test3(void) {
  //
  stp_table_choice tchoice = STP_TABLE_INTERNAL_ONLY;
  uint32_t num_procs = 8;
  uint32_t num_tiers = 3;
  uint32_t proc_number = 1;
  uint32_t els_per_tier = 20000;
  //
  LRU_Alloc_Sections_and_Threads last = TierAndProcManager<>::section_allocation_requirements(tchoice, num_procs, num_tiers);

  auto com_buf_sz = TierAndProcManager<>::check_expected_com_region_size(num_procs,num_tiers);

  uint8_t *com_buffer = new uint8_t[com_buf_sz];
  //
  cout << "front_end_internal_test com_buf_sz: " << com_buf_sz << " com_buffer: " << ((void *)com_buffer) << " com_buffer: " << ((void *)(com_buffer + com_buf_sz)) << endl;
  //
  map<key_t,void *> lru_segs;
  map<key_t,void *> hh_table_segs;
  map<key_t,size_t> seg_sizes;
  bool am_initializer = true;
  uint32_t max_obj_size = 128;
  void **random_segs = nullptr;

  for ( uint32_t t = 0; t < num_tiers; t++ ) {
    seg_sizes[t] = LRU_cache<>::check_expected_lru_region_size(max_obj_size, els_per_tier, num_procs);
    lru_segs[t] = new uint8_t[seg_sizes[t]];
    cout << "lru_segs[t]:: []" << t << "]  " << lru_segs[t] << endl;
  }

cout << "Initialize TierAndProcManager:: " << endl;
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  TierAndProcManager<> tapm(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


cout << "Launch threads... TierAndProcManager:: " << endl;

  tapm.launch_second_phase_threads(last);
  //

  map<uint32_t,uint32_t> hash_stamp;


  thread *input_alls[num_procs];


  for ( uint32_t i = 0; i < num_procs; i++ ) {
    input_alls[i] = new thread([&](int j){
      uint32_t hash_bucket = (uint32_t)j;
      uint32_t full_hash = (uint32_t)j;
      bool updating = false;
      unsigned int size = 64;
      char* buffer = new char[size];

      string tester = "this is a test";

      for ( int i = 0; i < 8; i++ ) {
        //
        full_hash++;
        hash_bucket++;

        tester += ' ';
        tester += (i+j);
        memset(buffer,0,size);
        strcpy(buffer,tester.c_str());
        uint32_t timestamp = now();
        hash_stamp[full_hash] = timestamp;
        //
        tapm.put_method(hash_bucket, full_hash, updating, buffer, size, timestamp);

      }

      for ( int j = 0; j < 100; j++ ) tick();

      hash_bucket = (uint32_t)j;
      full_hash = (uint32_t)j;

      for ( int i = 0; i < 8; i++ ) {
        //
        full_hash++;
        hash_bucket++;
        //
        memset(buffer,0,size);
        uint32_t timestamp = 0;
        //
        while ( timestamp == 0 ) {
          tick();
          timestamp = hash_stamp[full_hash];
        }
        //
        tapm.get_method(hash_bucket, full_hash, buffer, size, timestamp, 0);
        cout << hash_bucket << " -- " << buffer << endl;
        //
      }

      for ( int j = 0; j < 100; j++ ) tick();

      hash_bucket = (uint32_t)j;
      full_hash = (uint32_t)j;

      for ( int i = 0; i < 8; i++ ) {
        //
        full_hash++;
        hash_bucket++;
        //
        uint32_t timestamp = 0;
        //
        while ( timestamp == 0 ) {
          tick();
          timestamp = hash_stamp[full_hash];
        }
        //
        tapm.del_method(0,hash_bucket, full_hash,timestamp,0);
        //
      }

    },i);
  }


cout << "Shutting down threads... TierAndProcManager:: " << endl;
  //
  tapm.shutdown_threads(last);


    for ( uint32_t i = 0; i < num_procs; i++ ) {
      input_alls[i]->join();
    }
}


void front_end_internal_test_empty_shell(void) {
  //
  stp_table_choice tchoice = STP_TABLE_EMPTY_SHELL_TEST;
  uint32_t num_procs = 8;
  uint32_t num_tiers = 3;
  uint32_t proc_number = 0;
  uint32_t els_per_tier = 20000;
  //

  // allocation requirements
  LRU_Alloc_Sections_and_Threads last = TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>>::section_allocation_requirements(tchoice, num_procs, num_tiers);

  // size of com buffer
  auto com_buf_sz = TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>>::check_expected_com_region_size(num_procs,num_tiers);

  uint8_t *com_buffer = new uint8_t[com_buf_sz];
  //
  cout << "front_end_internal_test com_buf_sz: " << com_buf_sz << " com_buffer: " << ((void *)com_buffer) << " com_buffer: " << ((void *)(com_buffer + com_buf_sz)) << endl;
  //
  map<key_t,void *> lru_segs;
  map<key_t,void *> hh_table_segs;
  map<key_t,size_t> seg_sizes;
  bool am_initializer = true;
  uint32_t max_obj_size = 128;
  void **random_segs = nullptr;

  for ( uint32_t t = 0; t < num_tiers; t++ ) {
    seg_sizes[t] = LRU_cache<>::check_expected_lru_region_size(max_obj_size, els_per_tier, num_procs);
    lru_segs[t] = new uint8_t[seg_sizes[t]];
    cout << "lru_segs[t]:: []" << t << "]  " << lru_segs[t] << endl;
  }

cout << "Initialize TierAndProcManager:: " << endl;
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> tapm_1(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> tapm_2(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);
  
  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> tapm_3(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> tapm_4(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> tapm_5(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> tapm_6(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> tapm_7(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> tapm_8(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


cout << "Launch threads... TierAndProcManager:: " << endl;


  TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> **tapm_refs = new TierAndProcManager<8,3,LRU_cache_mock<LocalTimeManager>> *[num_procs];

  tapm_1.launch_second_phase_threads(last);
  tapm_refs[0] = &tapm_1;
  tapm_2.launch_second_phase_threads(last);
  tapm_refs[1] = &tapm_2;
  tapm_3.launch_second_phase_threads(last);
  tapm_refs[2] = &tapm_3;
  tapm_4.launch_second_phase_threads(last);
  tapm_refs[3] = &tapm_4;
  tapm_5.launch_second_phase_threads(last);
  tapm_refs[4] = &tapm_5;
  tapm_6.launch_second_phase_threads(last);
  tapm_refs[5] = &tapm_6;
  tapm_7.launch_second_phase_threads(last);
  tapm_refs[6] = &tapm_7;
  tapm_8.launch_second_phase_threads(last);
  tapm_refs[7] = &tapm_8;
  //



  cout << "ALL THREADS READY!!" << endl;


  map<uint32_t,uint32_t> hash_stamp;


  thread *input_alls[num_procs];

  for ( uint32_t i = 0; i < num_procs; i++ ) {
    input_alls[i] = new thread([&](int j){
      //

      //while ( true ) {

        uint32_t hash_bucket = (uint32_t)j*20;
        uint32_t full_hash = (uint32_t)j*20;
        //
        bool updating = false;
        unsigned int size = 64;
        char* buffer = new char[size];

        string tester = "this is a test";

        for ( int i = 0; i < 8; i++ ) {
          //
          full_hash++;
          hash_bucket++;

          tester += ' ';
          tester += (i+j);
          memset(buffer,0,size);
          strcpy(buffer,tester.c_str());
          uint32_t timestamp = now();
          hash_stamp[full_hash] = timestamp;
          //
          tapm_refs[j]->put_method(hash_bucket, full_hash, updating, buffer, size, timestamp);
          //

        {
    char logbuffer[64];
    sprintf(logbuffer, "PUT ... %d|%d|",(int)(j),(int)(i));
    cout << logbuffer << endl;
        }
        }


        {
    char logbuffer[64];
    sprintf(logbuffer, "PUT 2 ... %d|",(int)(j));
    cout << logbuffer << endl;
        }

        for ( int k = 0; k < 100; k++ ) tick();
    char logbuffer[64];
    sprintf(logbuffer, "GET ... %d|",(int)(j));
    cout << logbuffer << endl;

        hash_bucket = (uint32_t)j*20;
        full_hash = (uint32_t)j*20;

        for ( int i = 0; i < 8; i++ ) {
          //
          full_hash++;
          hash_bucket++;
          //
          memset(buffer,0,size);
          uint32_t timestamp = 0;
          //
          //while ( timestamp == 0 ) {
            tick();
            timestamp = hash_stamp[full_hash];
          //}
          //
          tapm_refs[j]->get_method(hash_bucket, full_hash, buffer, size, timestamp, 0);
          cout << hash_bucket << " -- " << buffer << endl;
          //
        }

        for ( int k = 0; k < 100; k++ ) tick();
    sprintf(logbuffer, "DEL ... %d|",(int)(j));
    cout << logbuffer << endl;

        hash_bucket = (uint32_t)j*20;
        full_hash = (uint32_t)j*20;

        for ( int i = 0; i < 8; i++ ) {
          //
          full_hash++;
          hash_bucket++;
          //
          uint32_t timestamp = 0;
          //
          //while ( timestamp == 0 ) {
            tick();
            timestamp = hash_stamp[full_hash];
          //}
          //
          tapm_refs[j]->del_method(j,hash_bucket, full_hash,timestamp,0);
          //
        }

    sprintf(logbuffer, "DONE... %d|",(int)(j));
    cout << logbuffer << endl;


    },i);
  }

  //for ( int k = 0; k < 1000; k++ ) tick();

cout << "enter when ready" << endl;
string yep = "yep";
cin >> yep;

cout << "Shutting down threads... TierAndProcManager:: " << endl;
  //
  tapm_1.shutdown_threads(last);
cout << "Shutdown threads 1" << endl;
  tapm_2.shutdown_threads(last);
cout << "Shutdown threads 2" << endl;
  tapm_3.shutdown_threads(last);
cout << "Shutdown threads 3" << endl;
  tapm_4.shutdown_threads(last);
cout << "Shutdown threads 4" << endl;
  tapm_5.shutdown_threads(last);
cout << "Shutdown threads 5" << endl;
  tapm_6.shutdown_threads(last);
cout << "Shutdown threads 6" << endl;
  tapm_7.shutdown_threads(last);
cout << "Shutdown threads 7" << endl;
  tapm_8.shutdown_threads(last);
cout << "Shutdown threads 8" << endl;
  //

    for ( uint32_t i = 0; i < num_procs; i++ ) {
      input_alls[i]->join();
    }
}











void front_end_internal_test_local(void) {
  //
  stp_table_choice tchoice = STP_TABLE_INTERNAL_ONLY;
  uint32_t num_procs = 8;
  uint32_t num_tiers = 3;
  uint32_t proc_number = 0;
  uint32_t els_per_tier = 20000;
  //

  // allocation requirements
  LRU_Alloc_Sections_and_Threads last = TierAndProcManager<8,3,LRU_cache<LocalTimeManager>>::section_allocation_requirements(tchoice, num_procs, num_tiers);

  // size of com buffer
  auto com_buf_sz = TierAndProcManager<8,3,LRU_cache<LocalTimeManager>>::check_expected_com_region_size(num_procs,num_tiers);

  uint8_t *com_buffer = new uint8_t[com_buf_sz];
  //
  cout << "front_end_internal_test com_buf_sz: " << com_buf_sz << " com_buffer: " << ((void *)com_buffer) << " com_buffer: " << ((void *)(com_buffer + com_buf_sz)) << endl;
  //
  map<key_t,void *> lru_segs;
  map<key_t,void *> hh_table_segs;
  map<key_t,size_t> seg_sizes;
  bool am_initializer = true;
  uint32_t max_obj_size = 128;
  void **random_segs = nullptr;

  for ( uint32_t t = 0; t < num_tiers; t++ ) {
    seg_sizes[t] = LRU_cache<>::check_expected_lru_region_size(max_obj_size, els_per_tier, num_procs);
    lru_segs[t] = new uint8_t[seg_sizes[t]];
    cout << "lru_segs[t]:: []" << t << "]  " << lru_segs[t] << endl;
  }

cout << "Initialize TierAndProcManager:: " << endl;
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_1(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_2(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);
  
  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_3(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_4(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_5(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_6(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_7(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_8(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


cout << "Launch threads... TierAndProcManager:: " << endl;


  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> **tapm_refs = new TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> *[num_procs];

  tapm_1.launch_second_phase_threads(last);
  tapm_refs[0] = &tapm_1;
  tapm_2.launch_second_phase_threads(last);
  tapm_refs[1] = &tapm_2;
  tapm_3.launch_second_phase_threads(last);
  tapm_refs[2] = &tapm_3;
  tapm_4.launch_second_phase_threads(last);
  tapm_refs[3] = &tapm_4;
  tapm_5.launch_second_phase_threads(last);
  tapm_refs[4] = &tapm_5;
  tapm_6.launch_second_phase_threads(last);
  tapm_refs[5] = &tapm_6;
  tapm_7.launch_second_phase_threads(last);
  tapm_refs[6] = &tapm_7;
  tapm_8.launch_second_phase_threads(last);
  tapm_refs[7] = &tapm_8;
  //



  cout << "ALL THREADS READY!!" << endl;


  map<uint32_t,uint32_t> hash_stamp;


  thread *input_alls[num_procs];

  for ( uint32_t i = 0; i < num_procs; i++ ) {
    input_alls[i] = new thread([&](int j){
      //

      //while ( true ) {

        uint32_t hash_bucket = (uint32_t)j*20;
        uint32_t full_hash = (uint32_t)j*20;
        //
        bool updating = false;
        unsigned int size = 64;
        char* buffer = new char[size];

        string tester = "this is a test";

        for ( int i = 0; i < 8; i++ ) {
          //
          full_hash++;
          hash_bucket++;

          tester += ' ';
          tester += (i+j);
          memset(buffer,0,size);
          strcpy(buffer,tester.c_str());
          uint32_t timestamp = now();
          hash_stamp[full_hash] = timestamp;
          //
          tapm_refs[j]->put_method(hash_bucket, full_hash, updating, buffer, size, timestamp);
          //

        {
    char logbuffer[64];
    sprintf(logbuffer, "PUT ... %d|%d|",(int)(j),(int)(i));
    cout << logbuffer << endl;
        }
        }


        {
    char logbuffer[64];
    sprintf(logbuffer, "PUT 2 ... %d|",(int)(j));
    cout << logbuffer << endl;
        }

        for ( int k = 0; k < 100; k++ ) tick();
    char logbuffer[64];
    sprintf(logbuffer, "GET ... %d|",(int)(j));
    cout << logbuffer << endl;

        hash_bucket = (uint32_t)j*20;
        full_hash = (uint32_t)j*20;

        for ( int i = 0; i < 8; i++ ) {
          //
          full_hash++;
          hash_bucket++;
          //
          memset(buffer,0,size);
          uint32_t timestamp = 0;
          //
          //while ( timestamp == 0 ) {
            tick();
            timestamp = hash_stamp[full_hash];
          //}
          //
          tapm_refs[j]->get_method(hash_bucket, full_hash, buffer, size, timestamp, 0);
          cout << hash_bucket << " -- " << buffer << endl;
          //
        }

        for ( int k = 0; k < 100; k++ ) tick();
    sprintf(logbuffer, "DEL ... %d|",(int)(j));
    cout << logbuffer << endl;

        hash_bucket = (uint32_t)j*20;
        full_hash = (uint32_t)j*20;

        for ( int i = 0; i < 8; i++ ) {
          //
          full_hash++;
          hash_bucket++;
          //
          uint32_t timestamp = 0;
          //
          //while ( timestamp == 0 ) {
            tick();
            timestamp = hash_stamp[full_hash];
          //}
          //
          tapm_refs[j]->del_method(j,hash_bucket, full_hash,timestamp,0);
          //
        }

    sprintf(logbuffer, "DONE... %d|",(int)(j));
    cout << logbuffer << endl;


    },i);
  }

  //for ( int k = 0; k < 1000; k++ ) tick();

cout << "enter when ready" << endl;
string yep = "yep";
cin >> yep;

cout << "Shutting down threads... TierAndProcManager:: " << endl;
  //
  tapm_1.shutdown_threads(last);
cout << "Shutdown threads 1" << endl;
  tapm_2.shutdown_threads(last);
cout << "Shutdown threads 2" << endl;
  tapm_3.shutdown_threads(last);
cout << "Shutdown threads 3" << endl;
  tapm_4.shutdown_threads(last);
cout << "Shutdown threads 4" << endl;
  tapm_5.shutdown_threads(last);
cout << "Shutdown threads 5" << endl;
  tapm_6.shutdown_threads(last);
cout << "Shutdown threads 6" << endl;
  tapm_7.shutdown_threads(last);
cout << "Shutdown threads 7" << endl;
  tapm_8.shutdown_threads(last);
cout << "Shutdown threads 8" << endl;
  //

    for ( uint32_t i = 0; i < num_procs; i++ ) {
      input_alls[i]->join();
    }
}




static const uint32_t Q_SIZE = (100);
SharedSegmentsTForm<QUEUED_map<>> app_segs;

void *create_data_region(key_t com_key,size_t size, bool am_initializer = false) {

  if ( app_segs.initialize_app_com_shm(com_key, size, am_initializer) == 0 ) {
    auto seg = app_segs._app_com_buffer;
    return seg;
  }

  return nullptr;
}


void remove_segment(key_t key, bool forced = false) {
  //
  app_segs.detach(key,forced);
  //
}


void front_end_queued_test(void) {
  //
  stp_table_choice tchoice = STP_TABLE_QUEUED;
  uint32_t num_procs = 8;
  uint32_t num_tiers = 3;
  uint32_t proc_number = 0;
  uint32_t els_per_tier = 20000;
  //

  // allocation requirements
  LRU_Alloc_Sections_and_Threads last = TierAndProcManager<8,3,LRU_cache<LocalTimeManager>>::section_allocation_requirements(tchoice, num_procs, num_tiers);

  // size of com buffer
  auto com_buf_sz = TierAndProcManager<8,3,LRU_cache<LocalTimeManager>>::check_expected_com_region_size(num_procs,num_tiers);

  uint8_t *com_buffer = new uint8_t[com_buf_sz];
  //
  cout << "front_end_internal_test com_buf_sz: " << com_buf_sz << " com_buffer: " << ((void *)com_buffer) << " com_buffer: " << ((void *)(com_buffer + com_buf_sz)) << endl;
  //
  map<key_t,void *> lru_segs;
  map<key_t,void *> hh_table_segs;
  map<key_t,size_t> seg_sizes;
  bool am_initializer = true;
  uint32_t max_obj_size = 128;
  void **random_segs = nullptr;

  for ( uint32_t t = 0; t < num_tiers; t++ ) {
    seg_sizes[t] = LRU_cache<>::check_expected_lru_region_size(max_obj_size, els_per_tier, num_procs);
    lru_segs[t] = new uint8_t[seg_sizes[t]];
    cout << "lru_segs[t]:: []" << t << "]  " << lru_segs[t] << endl;
  }

  uint8_t q_entry_count = 100;
  size_t rsiz = ExternalInterfaceQs<Q_SIZE>::check_expected_com_region_size(q_entry_count);
  //
  key_t com_key = 38450458;
  void *data_region = create_data_region(com_key,rsiz);
  hh_table_segs[38450458] = data_region;
  seg_sizes[38450458] = rsiz;

cout << "Initialize TierAndProcManager:: " << endl;
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  // all clients
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_1(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_2(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);
  
  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_3(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_4(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_5(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_6(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_7(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  proc_number++;
  
  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> tapm_8(com_buffer, lru_segs, hh_table_segs, seg_sizes,
												      am_initializer, tchoice, proc_number, num_procs, num_tiers, els_per_tier,
                              max_obj_size, random_segs);

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


cout << "Launch threads... TierAndProcManager:: " << endl;


  TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> **tapm_refs = new TierAndProcManager<8,3,LRU_cache<LocalTimeManager>> *[num_procs];

  tapm_1.launch_second_phase_threads(last);
  tapm_refs[0] = &tapm_1;
  tapm_2.launch_second_phase_threads(last);
  tapm_refs[1] = &tapm_2;
  tapm_3.launch_second_phase_threads(last);
  tapm_refs[2] = &tapm_3;
  tapm_4.launch_second_phase_threads(last);
  tapm_refs[3] = &tapm_4;
  tapm_5.launch_second_phase_threads(last);
  tapm_refs[4] = &tapm_5;
  tapm_6.launch_second_phase_threads(last);
  tapm_refs[5] = &tapm_6;
  tapm_7.launch_second_phase_threads(last);
  tapm_refs[6] = &tapm_7;
  tapm_8.launch_second_phase_threads(last);
  tapm_refs[7] = &tapm_8;
  //



  cout << "ALL THREADS BEING INITIALIZED!!" << endl;


  map<uint32_t,uint32_t> hash_stamp;


  thread *input_alls[num_procs];

  for ( uint32_t i = 0; i < num_procs; i++ ) {
    input_alls[i] = new thread([&](int j){
      //

    //   //while ( true ) {

        uint32_t hash_bucket = (uint32_t)j*20;
        uint32_t full_hash = (uint32_t)j*20;
        uint32_t h_bucket = hash_bucket;
        uint32_t f_hash = full_hash;

        //
        bool updating = false;
        unsigned int size = 128;
        char* buffer = new char[size];

        for ( int i = 0; i < 8; i++ ) {
          //
          h_bucket++;
          f_hash++;
          //
          hash_bucket = h_bucket;
          full_hash = f_hash;
          //
          memset(buffer,0,size);
          sprintf(buffer,"this is a test : %d %d %d",(i+j), i, j);
          uint32_t timestamp = now();
          hash_stamp[full_hash] = timestamp;
          //

          {
      char logbuffer[64];
      sprintf(logbuffer, "BEFORE PUT ... %d -%d -> %x",(int)(j),(int)(i),full_hash);
      cout << logbuffer << endl;
          }

          tapm_refs[j]->put_method(hash_bucket, full_hash, updating, buffer, size, timestamp);
          //

          {
      char logbuffer[64];
      sprintf(logbuffer, "PUT ... %d|%d| %x",(int)(j),(int)(i),full_hash);
      cout << logbuffer << endl;
          }
        }


        for ( int k = 0; k < 100; k++ ) tick();
    char logbuffer[64];
    sprintf(logbuffer, "GET ... %d|",(int)(j));
    cout << logbuffer << endl;

        hash_bucket = (uint32_t)j*20;
        full_hash = (uint32_t)j*20;

        for ( int i = 0; i < 8; i++ ) {
          //
          full_hash++;
          hash_bucket++;
          //
          memset(buffer,0,size);
          uint32_t timestamp = 0;
          //
          //while ( timestamp == 0 ) {
            tick();
            timestamp = hash_stamp[full_hash];
          //}
          //
          if ( tapm_refs[j]->get_method(hash_bucket, full_hash, buffer, size, timestamp, 0) >= 0 ) {
    sprintf(logbuffer, "<- GET ... %d| %s",(int)(hash_bucket),buffer);
    cout << logbuffer << endl;
          }
          //
        }

    //     for ( int k = 0; k < 100; k++ ) tick();
    // sprintf(logbuffer, "DEL ... %d|",(int)(j));
    // cout << logbuffer << endl;

    //     hash_bucket = (uint32_t)j*20;
    //     full_hash = (uint32_t)j*20;

    //     for ( int i = 0; i < 8; i++ ) {
    //       //
    //       full_hash++;
    //       hash_bucket++;
    //       //
    //       uint32_t timestamp = 0;
    //       //
    //       //while ( timestamp == 0 ) {
    //         tick();
    //         timestamp = hash_stamp[full_hash];
    //       //}
    //       //
    //       tapm_refs[j]->del_method(j,hash_bucket, full_hash,timestamp,0);
    //       //
    //     }

    sprintf(logbuffer, "DONE... %d|",(int)(j));
    cout << logbuffer << endl;


    },i);
  }

cout << "ALL THREADS INITIALIZED" <<  endl;

  //for ( int k = 0; k < 1000; k++ ) tick();

cout << "enter when ready" << endl;
string yep = "yep";
cin >> yep;

cout << "Shutting down threads... TierAndProcManager:: " << endl;
  //
  tapm_1.shutdown_threads(last);
cout << "Shutdown threads 1" << endl;
  tapm_2.shutdown_threads(last);
cout << "Shutdown threads 2" << endl;
  tapm_3.shutdown_threads(last);
cout << "Shutdown threads 3" << endl;
  tapm_4.shutdown_threads(last);
cout << "Shutdown threads 4" << endl;
  tapm_5.shutdown_threads(last);
cout << "Shutdown threads 5" << endl;
  tapm_6.shutdown_threads(last);
cout << "Shutdown threads 6" << endl;
  tapm_7.shutdown_threads(last);
cout << "Shutdown threads 7" << endl;
  tapm_8.shutdown_threads(last);
cout << "Shutdown threads 8" << endl;
  //

    for ( uint32_t i = 0; i < num_procs; i++ ) {
      input_alls[i]->join();
    }


    remove_segment(com_key);
}




void test_circ_buf(void) {

  auto sz = c_table_proc_com::check_expected_region_size(20);
  cout << "c_table_proc_com::check_expected_region_size(20): " << sz << endl;

  auto sz2 = c_table_proc_com::check_expected_region_size(20,2,8);
  cout << "c_table_proc_com::check_expected_region_size(20,2,8): " << sz2 << endl;

  c_table_proc_com  cbuf{
    ._num_client_p = 2,
    ._num_service_threads = 8
  };

  uint8_t *data_region = new uint8_t[sz2];
  

  cout << " cbuf._num_service_threads  : " << (int)cbuf._num_service_threads << endl; 
  cbuf.set_region(data_region,20,sz2,true);
  //
  //
  if ( cbuf._put_com[0]._put_queue.full() ) cout << "put queue is full " << endl;
  if ( cbuf._put_com[0]._put_queue.empty() ) cout << "put queue is empty " << endl;

  int i = 0;
  while ( !(cbuf._put_com[0]._put_queue.full()) ) {
    c_put_cell input;
    c_put_cell output;
    //
    i++;
    input._hash = i + 11;
    input._proc_id =  1;
    input._value = 2 + i*23;
    cbuf._put_com[0]._put_queue.push_queue(input);
    if ( i%3 == 0 ) {
      cbuf._put_com[0]._put_queue.pop_queue(output);
      cout << " output : " << output._hash << " :: " << output._value << endl;
    }
    if ( cbuf._put_com[0]._put_queue.full() ) cout << "put queue is full " << endl;
    if ( cbuf._put_com[0]._put_queue.empty() ) cout << "put queue is empty " << endl;
  }
  
  i = 0;
  while ( !(cbuf._put_com[0]._put_queue.empty()) ) {
    c_put_cell input;
    c_put_cell output;
    //
    i++;
    if ( i%2 == 0 ) {
      input._hash = i + 11;
      input._proc_id =  1;
      input._value = 2 + i*23;
      cbuf._put_com[0]._put_queue.push_queue(input);
    }
    cbuf._put_com[0]._put_queue.pop_queue(output);
    cout << " output : " << output._hash << " :: " << output._value << endl;
  }

  if ( cbuf._put_com[0]._put_queue.full() ) cout << "put queue is full " << endl;
  if ( cbuf._put_com[0]._put_queue.empty() ) cout << "put queue is empty " << endl;

}


atomic_flag _write_awake;
atomic_flag _get_awake;

atomic_flag *ref_write_awake[32];
atomic_flag *ref_get_awake[32];


void setup_get_write_awake(void) {
  for ( int i = 0; i < 32; i++  ) {
    ref_write_awake[i] = nullptr;
    ref_get_awake[i] = nullptr;
  }
}

void clear_all_atomic_flags(void) {
  for ( int i = 0; i < 32; i++  ) {
    if ( ref_write_awake[i] != nullptr ) {
      ref_write_awake[i]->clear();
    }    ref_get_awake[i] = nullptr;
    if ( ref_get_awake[i] != nullptr ) {
      ref_get_awake[i]->clear();
    }
  }
}


void add_write_awake(uint8_t q,atomic_flag *rflag) {
  rflag->clear();
  ref_write_awake[q] = rflag;
}


void add_get_awake(uint8_t q,atomic_flag *rflag) {
  rflag->clear();
  ref_get_awake[q] = rflag;
}

// 
void await_put(uint8_t q) {

  // while ( true ) {
  //   if ( g_global_shutdown.test() ) return;
  //   while ( _write_awake.test() ) {
  //     if ( g_global_shutdown.test() ) return;
  //     tick();
  //   }
  //   if ( _write_awake.test_and_set() ) continue;
  //   break;
  // }

  while ( true ) {
    if ( g_global_shutdown.test() ) return;
    uint16_t check = 0;
    while ( ref_write_awake[q]->test() ) {
      if ( g_global_shutdown.test() ) return;
      tick();
      if ( ++check > 1000 ) { ref_write_awake[q]->clear();  check = 0; }
    }
    if ( ref_write_awake[q]->test_and_set() ) continue;
    break;
  }

}
// 
void clear_put(uint8_t q) {
  //_write_awake.clear();
  ref_write_awake[q]->clear();
}


void await_get(uint8_t q) {

  // while ( true ) {
  //   if ( g_global_shutdown.test() ) return;
  //   while ( _get_awake.test() ) {
  //     if ( g_global_shutdown.test() ) return;
  //     tick();
  //   }
  //   if ( _get_awake.test_and_set() ) continue;
  //   break;
  // }

  while ( true ) {
    if ( g_global_shutdown.test() ) return;
    uint16_t check = 0;
    while ( ref_get_awake[q]->test() ) {
      if ( g_global_shutdown.test() ) return;
      tick();
      if ( ++check > 1000 ) {ref_get_awake[q]->clear(); check = 0; }
    }
    if ( ref_get_awake[q]->test_and_set() ) continue;
    break;
  }
}
// 
void clear_get(uint8_t q) {
  //_get_awake.clear();
  ref_get_awake[q]->clear();
}


// void test_circ_buf_threads(uint8_t test_proc_number = 0) {

//   using_segments = false;
//   _write_awake.clear();

//   // auto sz = c_table_proc_com::check_expected_region_size(20);
//   // cout << "c_table_proc_com::check_expected_region_size(20): " << sz << endl;

//   auto rsiz = c_table_proc_com::check_expected_region_size(20,2,8);
//   cout << "c_table_proc_com::check_expected_region_size(20,2,8): " << rsiz << endl;

//   // map<key_t,void *> lru_segs;
//   // map<key_t,void *> hh_table_segs;
//   // map<key_t,size_t> seg_sizes;
//   bool am_initializer = true;
//   // uint32_t max_obj_size = 128;
//   // void **random_segs = nullptr;

//   // uint8_t q_entry_count = 100;
//   // size_t rsiz = ExternalInterfaceWaitQs<Q_SIZE>::check_expected_com_region_size(q_entry_count);
//   //
//   key_t com_key = 38450458;
//   g_com_key = com_key;
//   void *data_region = create_data_region(com_key,rsiz,( (test_proc_number < 2) ? true : false ));    // this test is without partner

//   cout << data_region << endl;
//   if ( data_region == nullptr ) {
//     cout << " NO DATA REGION" << endl;
//     exit(0);
//   }
//   // hh_table_segs[38450458] = data_region;
//   // seg_sizes[38450458] = rsiz;


//   c_table_proc_com  cbuf{
//     ._num_client_p = 2,
//     ._num_service_threads = 8
//   };

//   c_table_proc_com  cbuf2{
//     ._num_client_p = 2,
//     ._num_service_threads = 8
//   };

//   c_table_proc_com  cbuf3{
//     ._num_client_p = 2,
//     ._num_service_threads = 8
//   };

//   // uint8_t *data_region = new uint8_t[rsiz];

//   bool may_use_counter = true;
//   thread *input_alls[3] = {nullptr,nullptr,nullptr};

//   if ( (test_proc_number == 0) || (test_proc_number == 1) ) {
//   cout << " cbuf._num_service_threads  : " << (int)cbuf._num_service_threads << endl; 
//     cbuf.set_region(data_region,20,rsiz,am_initializer);

//     am_initializer = false;

//     if ( cbuf._put_com[0]._put_queue.full() ) cout << "put queue is full " << endl;
//     if ( cbuf._put_com[0]._put_queue.empty() ) cout << "put queue is empty " << endl;

//   cout << "ATTACHING DATA REGION" << endl;
//     cbuf2.set_region(data_region,20,rsiz,am_initializer);
//       // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
//     for ( uint32_t i = 0; i < 2; i++ ) {
//       input_alls[i] = new thread([&](int j){
//         //
//         c_table_proc_com *cbufp = nullptr;
//         if ( j == 0 ) {
//           cbufp = &cbuf;
//         } else {
//           cbufp = &cbuf2;
//         }
//         int i = 0;
//         while ( may_use_counter ) {
//           //
//           while ( !(cbufp->_put_com[0]._put_queue.full()) ) {
//             c_put_cell input;
//             //
//             i++;
//             input._hash = i + 11;
//             input._proc_id =  1;
//             input._value = 2 + i*23;
//             await_put(j);
//             cbufp->_put_com[0]._put_queue.push_queue(input);
//             clear_put(j);
//             //
//             if ( cbufp->_put_com[0]._put_queue.full() ) cout << "put queue is full " << endl;
//             if ( cbufp->_put_com[0]._put_queue.empty() ) cout << "put queue is empty " << endl;
//           }
//           //
//           int k = 0;
//           while ( (k < 50) && may_use_counter ) {
//             k++;
//             while ( cbuf._put_com[0]._put_queue.full() && may_use_counter ) tick();
//           }
//         }
//         //
//       },i);
//     }
//   }
//   //
//   if ( test_proc_number == 0 ) {
//     for ( int i = 0; i < 100; i++ ) tick();
//   }
//   //
//   if ( (test_proc_number == 0) || (test_proc_number == 2) ) {
//     //
//   cout << "ATTACHING DATA REGION" << endl;
//   am_initializer = false;
//   cbuf3.set_region(data_region,20,rsiz,am_initializer);
//     //
//     input_alls[2] = new thread([&](int j){
//       //
//       int i = 0;
//       while ( may_use_counter ) {
//         //
//         while ( !(cbuf3._put_com[0]._put_queue.empty()) ) {
//           c_put_cell input;
//           c_put_cell output;
//           //
//           await_put(j);
//           cbuf3._put_com[0]._put_queue.pop_queue(output);
//           clear_put(j);
//           cout << " output : " << output._hash << " :: " << output._value << endl;
//         }
//         //
//         int k = 0;
//         while ( (k < 50) && may_use_counter ) {
//           k++;
//           while ( cbuf3._put_com[0]._put_queue.empty() && may_use_counter ) tick();
//         }
//       }
//       //
//     },2);
//   }

//   cout << "ALL THREADS INITIALIZED" <<  endl;

//     //for ( int k = 0; k < 1000; k++ ) tick();

//   cout << "enter when ready" << endl;
//   string yep = "yep";
//   cin >> yep;
//   may_use_counter = false;

//   for ( int i = 0; i < 3; i++ ) {
//     if ( input_alls[i] != nullptr ) {
//       input_alls[i]->join();
//     }
//   }
  
//   remove_segment(com_key,( (test_proc_number < 2) ? true : false ));

// }





// WORKS... 

void test_circ_buf_prod_threads(uint8_t test_proc_number = 0) {

  using_segments = true;
  //_write_awake.clear();
  g_global_shutdown.clear();

  uint8_t number_clients = 10;
  uint8_t n_type_service = 8;
  uint8_t number_service_threads = n_type_service*2;
  uint8_t queue_depth = 20;
  uint32_t sect_size = 200;

  uint8_t total_threads = number_clients + number_service_threads;

  init_random(0,n_type_service-1);
  setup_get_write_awake();

  auto rsiz = c_table_proc_com::check_expected_region_size(queue_depth,number_clients,n_type_service);
  cout << "c_table_proc_com::check_expected_region_size(" << (int)queue_depth 
              << "," << (int)number_clients << "," << (int)n_type_service << "): " << rsiz << endl;

  // map<key_t,void *> lru_segs;
  // map<key_t,void *> hh_table_segs;
  // map<key_t,size_t> seg_sizes;
  bool am_initializer = (((test_proc_number == 2) || (test_proc_number == 0)) ? true : false );
  // uint32_t max_obj_size = 128;
  // void **random_segs = nullptr;

  // uint8_t q_entry_count = 100;
  // size_t rsiz = ExternalInterfaceWaitQs<Q_SIZE>::check_expected_com_region_size(q_entry_count);
  //
  key_t com_key = 38450458;
  g_com_key = com_key;
  void *data_region = create_data_region(com_key, rsiz*4, am_initializer);    // this test is without partner

  cout << data_region << endl;
  if ( data_region == nullptr ) {
    cout << " NO DATA REGION" << endl;
    exit(0);
  }

  // hh_table_segs[38450458] = data_region;
  // seg_sizes[38450458] = rsiz;

  c_table_proc_com  cbufs[32];  // 2 writers, 8+ getters = 10 cbufs, 8 sections == 16 service threads = 1 cbuf

// total_threads = number_clients + number_service_threads
  auto need_cbufs = number_clients + 1;
  for ( int i = 0; i < need_cbufs; i++ ) {
    cbufs[i]._num_client_p = number_clients;
    cbufs[i]._num_service_threads = n_type_service;
  }

  bool may_use_counter = true;
  thread *input_alls[total_threads];
  // INITIALIZE THREADS
  for ( int i = 0; i < total_threads; i++ ) {
    input_alls[i] = nullptr;
  }


  // CLIENT QUEUES
  if ( (test_proc_number == 0) || (test_proc_number == 1) ) {

    // try {
    //   cbufs[0].set_region(data_region,queue_depth,rsiz,am_initializer);
    // } catch ( std::exception ex ) {
    //   cout << "Failed to create region" << endl;
    //   remove_segment(com_key,( (test_proc_number < 2) ? true : false ));
    //   exit(0);
    // }

  cout << "ATTACHING DATA REGION" << endl;
    am_initializer = false;
    for ( int i = 0; i < number_clients; i++ ) {
      try {
        cbufs[i].set_region(data_region,queue_depth,rsiz,am_initializer);
      } catch ( std::exception ex ) {
        cout << "Failed to attach region" << endl;
        remove_segment(com_key,( (test_proc_number < 2) ? true : false ));
        exit(0);
      }
    }
  cout << "DATA REGIONS ATTACHED" << endl;
    uint8_t num_puters = 2;
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    for ( uint32_t i = 0; i < num_puters; i++ ) {
      //
      input_alls[i] = new thread([&](int j){
        //
        c_table_proc_com *cbufp = &cbufs[j];

        for ( uint8_t w = 0; w < n_type_service; w++ ) {
          add_write_awake(w,cbufp->_put_com[w]._write_awake);
        }

        //
        uint32_t k = 0;
        bool pick_2 = false;
        while ( may_use_counter && !(g_global_shutdown.test()) ) {
          //
          //
          c_put_cell input;
          //
          // auto which_q = (pick_2 ? 0 : 1); //get_rand();
          // pick_2 = !pick_2;
          auto which_q = get_rand();
          if ( ( which_q > 7 ) || ( which_q < 0 ) ) continue;
          k++;
          k = k%(UINT32_MAX-20);
          input._hash = (which_q*11 + k)%(UINT32_MAX-20);
          input._proc_id =  j;
          input._value = (2 + k*23)%(UINT32_MAX-20);
          //

{
char buffer[64];
sprintf(buffer,"-%d+",(int)which_q);
cout << buffer << endl; 
}

          await_put(which_q);
          if ( !g_global_shutdown.test() ) {
            if ( may_use_counter && cbufp->_put_com[which_q]._put_queue.full() ) {
              clear_put(which_q);
              continue;
            }
            //
            if ( may_use_counter && !g_global_shutdown.test() ) {
              cbufp->_put_com[which_q]._put_queue.push_queue(input);
              clear_put(which_q);
            }
            //
            if ( may_use_counter && !g_global_shutdown.test() ) {
              if ( cbufp->_put_com[which_q]._put_queue.full() ) cout << "put queue is full " << endl;
              if ( cbufp->_put_com[which_q]._put_queue.empty() ) cout << "put queue is empty " << endl;
            }
          }
        }


cout << "THIS THREAD IS DONE" << endl;
        //
      },i);
    }

    for ( uint32_t i = num_puters; i < number_clients; i++ ) {
      //
      input_alls[i] = new thread([&](int j){
        //
        c_table_proc_com *cbufp = &cbufs[j];

        for ( uint8_t w = 0; w < n_type_service; w++ ) {
          add_get_awake(w,cbufp->_get_com[w]._get_awake);
          cbufp->_outputs[j]._reader.clear();
        }

        //
        uint32_t k = 0;
        bool pick_2 = false;
        while ( may_use_counter && !(g_global_shutdown.test()) ) {
          //
          //
          c_request_cell input;
          //
          // auto which_q = (pick_2 ? 0 : 1); //get_rand();
          // pick_2 = !pick_2;
          auto which_q = get_rand();
          if ( ( which_q > 7 ) || ( which_q < 0 ) ) continue;
          k++;
          k = k%(UINT32_MAX-20);
          input._hash = (which_q*11 + k)%(UINT32_MAX-20);
          input._proc_id =  j;
          //

// {
// char buffer[64];
// sprintf(buffer,"&%dg",(int)which_q);
// cout << buffer << endl; 
// }

          await_put(which_q);
          if ( !g_global_shutdown.test() ) {
            if ( may_use_counter && cbufp->_get_com[which_q]._get_queue.full() ) {
              clear_put(which_q);
              continue;
            }
            //
            if ( may_use_counter && !g_global_shutdown.test() ) {
              while( !(cbufp->_outputs[j]._reader.test_and_set()) );
              //
              cbufp->_get_com[which_q]._get_queue.push_queue(input);
              uint16_t check  = 0;
              while ( cbufp->_outputs[j]._reader.test() ) {
                if ( check++ > 1000 ) {
                  if ( g_global_shutdown.test() ) return;
                  break; 
                }
                tick();
              }
              if ( check < 1000 ) {
                auto val = cbufp->_outputs[j]._value;
cout << "GOT VALUE " << val << endl;
              } else {
                cbufp->_outputs[j]._reader.clear();
cout << "NO VALUE DELIVERED" << endl;
              }
              clear_put(which_q);
            }
            //
            if ( may_use_counter && !g_global_shutdown.test() ) {
              if ( cbufp->_get_com[which_q]._get_queue.full() ) cout << "put queue is full " << endl;
              if ( cbufp->_get_com[which_q]._get_queue.empty() ) cout << "put queue is empty " << endl;
            }
          }
        }


cout << "GET THREAD IS DONE" << endl;
        //
      },i);

    }

  }
  //
  if ( test_proc_number == 0 ) {
    for ( int i = 0; i < 100; i++ ) tick();
  }
  //

  // SERVICE QUEUES
  if ( (test_proc_number == 0) || (test_proc_number == 2) ) {
    //

cout << "ATTACHING DATA REGION" << endl;
    am_initializer = true;
    try {
      cbufs[number_clients].set_region(data_region,queue_depth,rsiz,am_initializer);
    } catch ( std::exception ex ) {
      cout << "Failed to attach region" << endl;
      remove_segment(com_key,true);
      exit(0);
    }

    c_table_proc_com *cbufp =  &cbufs[number_clients];

cout << "DATA REGIONS ATTACHED" << endl;
    //
    for ( uint32_t i = 0; i < n_type_service; i++ ) {
      //
      auto m = i;
      //m += (test_proc_number == 2) ? 0 : number_clients;
      input_alls[m] = new thread([&](int j){
        add_write_awake(j,cbufp->_put_com[j]._write_awake);
        //
        while ( may_use_counter && !g_global_shutdown.test() ) {
          //
          int k = 0;
          while ( (k < 50) && may_use_counter ) {
            k++;
            if ( g_global_shutdown.test() ) {
              cout << "quick exit thread " << j << endl;
              return;
            }
            if ( !(g_global_shutdown.test()) && may_use_counter && cbufp->_put_com[j]._put_queue.empty() ) tick();
            else break;
          }
          //
          await_put(j);
          if ( !(g_global_shutdown.test()) ) {
            if ( cbufp->_put_com[j]._put_queue.empty() ) {
              clear_put(j);
              continue;
            }
            if ( may_use_counter && !g_global_shutdown.test() ) {
              c_put_cell output;
              cbufp->_put_com[j]._put_queue.pop_queue(output);
              clear_put(j);
{
char buffer[64];
sprintf(buffer,"> %d  output : %d :: %d ",(int)j,output._hash,output._value);
cout << buffer << endl; 
}
            }
          }
        }
        //
        cout << "Leaving service thread" << j << endl;
      },i);

      auto n = n_type_service + m;
      input_alls[m] = new thread([&](int j){
        add_get_awake(j,cbufp->_get_com[j]._get_awake);
        cbufp->_get_com[j]._client_privilege->clear();
        //
       while ( may_use_counter && !g_global_shutdown.test() ) {
          //
          int k = 0;
          while ( (k < 50) && may_use_counter ) {
            k++;
            if ( g_global_shutdown.test() ) {
              cout << "quick exit thread " << j << endl;
              return;
            }
            if ( !(g_global_shutdown.test()) && may_use_counter && cbufp->_get_com[j]._get_queue.empty() ) tick();
            else break;
          }
          //
          await_get(j);
          if ( !(g_global_shutdown.test()) ) {
            if ( cbufp->_get_com[j]._get_queue.empty() ) {
              clear_get(j);
              continue;
            }
            if ( may_use_counter && !g_global_shutdown.test() ) {
              c_request_cell output;
              //
              cbufp->_get_com[j]._get_queue.pop_queue(output);
              //
              if ( output._proc_id < number_clients ) {
                cbufp->_outputs[output._proc_id]._value = get_rand();
                cbufp->_outputs[output._proc_id]._reader.clear();
              } else {
                cout << " output._proc_id way too big: " << (int) output._proc_id << endl;
              }

              clear_get(j);
// {
// char buffer[64];
// sprintf(buffer,"> %d  get request : %d :: %d ",(int)j,output._hash,output._proc_id);
// cout << buffer << endl; 
// }
            }
          }
        }

        cout << "Leaving GET service thread" << j << endl;

        //
      },m);

    }
  }

  cout << "ALL THREADS INITIALIZED" <<  endl;

    //for ( int k = 0; k < 1000; k++ ) tick();

  if (  test_proc_number != 2 ) {
    cout << "enter when ready" << endl;
    string yep = "yep";
    cin >> yep;
    

    cout << "ENDING NOW " << endl;
    may_use_counter = false;
    cout << "GLOBAL CLOSE " << endl;

    
    while ( !(g_global_shutdown.test_and_set()) ) tick();
    cout << "GOING... " << endl;

    for ( int i = 0; i < 1000; i++ ) tick();

    cout << "WAITING FOR USER INPUT " << endl;

    for ( int i = 0; i < 1000; i++ ) tick();
  }

  for ( int i = 0; i < total_threads; i++ ) {
cout << "joining " << i << endl;
    if ( input_alls[i] != nullptr ) {
      input_alls[i]->join();
    }
  }
  
  if (  test_proc_number != 2 ) {
    cout << "REMOVING SEGMENTS" << endl;
    remove_segment(com_key,am_initializer);
  }

}







uint32_t g_put_count[2] = {0,0};
//
void mid_layer_queued_test(void) {
  //
  stp_table_choice tchoice = STP_TABLE_QUEUED;
  uint32_t num_procs = 8;
  uint32_t num_tiers = 3;
  uint32_t proc_number = 0;
  uint32_t els_per_tier = 20000;

  //
  map<key_t,void *> lru_segs;
  map<key_t,void *> hh_table_segs;
  map<key_t,size_t> seg_sizes;
  bool am_initializer = true;
  uint32_t max_obj_size = 128;
  void **random_segs = nullptr;

  uint8_t q_entry_count = 100;
  size_t rsiz = ExternalInterfaceWaitQs<Q_SIZE>::check_expected_com_region_size(q_entry_count);
  //
  key_t com_key = 38450458;
  g_com_key = com_key;
  void *data_region = create_data_region(com_key,rsiz);
  hh_table_segs[38450458] = data_region;
  seg_sizes[38450458] = rsiz;


  ExternalInterfaceWaitQs<Q_SIZE> x_i_q;

  uint8_t client_count = 8;
  uint8_t thread_count = 8;

  x_i_q.initialize(client_count,thread_count,data_region,els_per_tier);
  //
  //  //

  auto sect_size = els_per_tier/thread_count;
  thread *input_alls[client_count];


  map<uint32_t,uint32_t> stored[2];
  map<uint32_t,uint32_t> retrieved[8];
  map<uint32_t,uint32_t> retrieved_count[8];

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  for ( uint32_t i = 0; i < 2; i++ ) {
    //
    auto thread_id = x_i_q.next_thread_id();
    //
    input_alls[i] = new thread([&](int j){
      //
      cout << "thread_id: " << (int)j << endl;
      //
      uint16_t m = 0;
      while ( m++ < 1000 ) {
        //
        uint32_t w = 0;
        g_put_count[j]++;
        //
        //for ( uint32_t hh = 0; hh < els_per_tier; hh += sect_size ) {
        for ( uint32_t hh = 0; hh < 1; hh += sect_size ) {
          //
          auto h_hat = hh;
          for ( int i = 0; i < 10; i++ ) {
            h_hat++;
            h_hat += j*20;
            x_i_q.com_put(h_hat,++w,j);
            stored[j][h_hat] = w;
          }
          //
        }

        // for ( int n = 0; n < 500; n++ ) tick();
      }
      //
      cout << "WRITER THREAD DONE: " << (int)j << endl;
      //
    },thread_id);
    //
  }


// cout << "enter when ready for gets::: " << endl;
// string go = "yep";
// cin >> go;


  bool may_use_counter = true;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  for ( uint32_t i = 2; i < client_count; i++ ) {
    //
    auto thread_id = x_i_q.next_thread_id();
    //
    input_alls[i] = new thread([&](int j){
      //
      cout << "thread_id: " << (int)j << endl;
      //
      uint16_t m = 0;
      while ( m++ < 1000 ) {
        //
        uint32_t val = 0;
        //
        //for ( uint32_t hh = 0; hh < els_per_tier; hh += sect_size ) {
        for ( uint32_t hh = 0; hh < 1; hh += sect_size ) {
          //
          auto h_hat = hh;
          for ( int i = 0; i < 10; i++ ) {
            h_hat++;
            h_hat += (j%2)*20;
            //
            x_i_q.com_req(h_hat,val,j);
            //
            if ( may_use_counter ) {
              retrieved[j][h_hat] = val;
              auto rc = retrieved_count[j][h_hat];
              retrieved_count[j][h_hat] = rc + 1;
            }
            //
          }
          //
        }

      }

    },thread_id);
  }

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

      // cout << x_i_q.com_put(1,UINT32_MAX,thread_id) << endl;
      // cout << x_i_q.com_req(1,val,thread_id) << endl;

      // cout << "val: " << val << endl;

  //


cout << "ALL THREADS INITIALIZED" <<  endl;

  //for ( int k = 0; k < 1000; k++ ) tick();

cout << "enter when ready" << endl;
string yep = "yep";
cin >> yep;
may_use_counter = false;


cout << "REPORT" << endl;
// map<uint32_t,uint32_t> stored[2];
// map<uint32_t,uint32_t> retrieved[8];
// map<uint32_t,uint32_t> retrieved_count[8];

for ( int l = 0; l < 2; l++ ) {
  for ( auto p : stored[l] ) {
    auto hh = p.first;
    auto val = p.second;
    for ( int k = l; k < 8; k += 2 ) {
      auto gval = retrieved[k][hh];
      cout << val << " " << "gval == val: " << (gval == val ? "true" : "false") << endl;
      auto count = retrieved_count[k][hh];
      cout << "retrieved by " << k << " ---> " << count << " times" << endl;
    }
  }
}


cout << "Shutting down threads... TierAndProcManager:: " << endl;


    for ( uint32_t i = 0; i < client_count; i++ ) {
      input_alls[i]->join();
    }


    remove_segment(com_key);
}


/**
 * shutdown_on_signal
 */
void shutdown_on_signal(int signal) {
  gSignalStatus = signal;

  while ( !(g_global_shutdown.test_and_set()) );
  for ( int i = 0; i < 10; i++ ) clear_all_atomic_flags();
  uint16_t tick_max = 10000;
  while ( g_global_shutdown.test() && (--tick_max > 0) ) tick();
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  cout << "g_put_count[0]: " << g_put_count[0] << "  g_put_count[1]: " << g_put_count[1] << endl;
  if ( using_segments ) {
    remove_segment(g_com_key);
  }
  exit(0);
}



/**
 * main ...
 * 
 */

int main(int argc, char **argv) {
	//
  std::signal(SIGINT, shutdown_on_signal);

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


//  test_shared_queue();
//  test_shared_queue_threads();
// test_node_shm_queued();

  // 
  // front_end_internal_test();

  // front_end_queued_test();

  // mid_layer_queued_test();

  // test_circ_buf();

  uint8_t pnum = 0;
	if ( argc == 2 ) {
    pnum = atoi(argv[1]);
	}

  //test_circ_buf_threads(pnum);
  test_circ_buf_prod_threads(pnum);

// test_circ_buf_threads(uint8_t test_proc_number = 0)
  // test_simple_stack();
  // test_toks();
  // test_slab_primitives();
  // test_initialization();

  // test_slab_threads();


  // ----
  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;

  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;

  cout << (UINT32_MAX - 4294916929) << endl;

  return(0);
}

