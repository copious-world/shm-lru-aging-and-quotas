
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

  Storage_ExternalInterfaceQs<8,200> q_test(client_count,thrd_count,region,els_per_tier,true);  // thread count is relevant
  QUEUED_map<200> q_client(region,sz,els_per_tier,client_count);  // one per thread



  cout << "q_test._proc_refs._put_com->_put_queue._count_free: " << q_test._proc_refs._put_com[0]._put_queue._count_free << endl;
  cout << "q_client._com._proc_refs._put_com->_put_queue._count_free: " << q_client._com._proc_refs._put_com[0]._put_queue._count_free << endl;

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


void remove_segment(key_t key) {
  //
  app_segs.detach(key,false);
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
        unsigned int size = 64;
        char* buffer = new char[size];

        string tester = "this is a test";

        for ( int i = 0; i < 8; i++ ) {
          //
          h_bucket++;
          f_hash++;
          //
          hash_bucket = h_bucket;
          full_hash = f_hash;
          //
          tester += ' ';
          tester += (i+j);
          memset(buffer,0,size);
          strcpy(buffer,tester.c_str());
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
            cout << hash_bucket << " -- " << buffer << endl;
          }
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


//  test_shared_queue();
//  test_shared_queue_threads();
// test_node_shm_queued();

  // 
  //front_end_internal_test();

  front_end_queued_test();

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



