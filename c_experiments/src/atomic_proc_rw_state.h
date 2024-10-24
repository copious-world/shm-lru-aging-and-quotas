#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>

#include <map>
#include <unordered_map>
#include <list>
#include <chrono>
#include <atomic>


using namespace std;


const uint16_t MAX_WAIT_LOOPS = 1000;


typedef enum {
    CLEAR_FOR_WRITE,	// unlocked - only one process will write in this spot, so don't lock for writing. Just indicate that reading can be done
    CLEARED_FOR_ALLOC,	// the current process will set the atomic to CLEARED_FOR_ALLOC
    LOCKED_FOR_ALLOC,	// a thread (process) that picks up the reading task will block other readers from this spot
    CLEARED_FOR_COPY,	// now let the writer copy the message into storage
    FAILED_ALLOCATOR
} COM_BUFFER_STATE;

// The data storage table is arranged as an array of cells, where each cell has a header which may belong to a free list
// or to the LRU list data structure, which will allow for quick access to aged out entries. 

// There is one LRU per hash table group (pool), and the size of the LRU is at most 50% (maybe or) of all possible hash table
// entries summed across the pool entries. 

//
static inline void clear_for_write(atomic<COM_BUFFER_STATE> *read_marker) {   // first and last
    auto p = read_marker;
    auto current_marker = p->load(std::memory_order_acquire);
    auto prev_marker = current_marker;
    //
    while ( CLEAR_FOR_WRITE != current_marker ) {
        while( !(p->compare_exchange_weak(current_marker,CLEAR_FOR_WRITE,std::memory_order_release)) && ( prev_marker == current_marker ) )
        ;
        current_marker = p->load(std::memory_order_acquire);
        prev_marker = current_marker;
    }
}

//
static inline void cleared_for_alloc(atomic<COM_BUFFER_STATE> *read_marker) {
    auto p = read_marker;
    auto current_marker = p->load(std::memory_order_acquire);
    while(!p->compare_exchange_weak(current_marker,CLEARED_FOR_ALLOC,std::memory_order_release)
                    && (p->load() != CLEARED_FOR_ALLOC));
}

//
static inline bool claim_for_alloc(atomic<COM_BUFFER_STATE> *read_marker) {
    if ( read_marker == nullptr ) return false;
    //
    auto p = read_marker;
    auto current_marker = p->load(std::memory_order_acquire);
    while ( CLEARED_FOR_ALLOC == current_marker ) {
        bool status = false;
        while( !(p->compare_exchange_weak(current_marker,LOCKED_FOR_ALLOC,std::memory_order_release)) && (current_marker != CLEARED_FOR_ALLOC) )
        ;
        if ( current_marker == LOCKED_FOR_ALLOC ) return false; // someone else owns this position
        status = (p->load() == LOCKED_FOR_ALLOC);  // this process owns the lock unless some other process set a state such as an error different than the states handled here.
        return status;
    }
    //
    return false;
}

//
static inline void clear_for_copy(atomic<COM_BUFFER_STATE> *read_marker) {
    auto p = read_marker;
    auto current_marker = p->load(std::memory_order_acquire);
    while(!p->compare_exchange_weak(current_marker,CLEARED_FOR_COPY,std::memory_order_release)
                    && ((COM_BUFFER_STATE)(p->load()) != CLEARED_FOR_COPY));
}

//
static inline void indicate_error(atomic<COM_BUFFER_STATE> *read_marker) {
    auto p = read_marker;
    auto current_marker = p->load(std::memory_order_acquire);
    while(!p->compare_exchange_weak(current_marker,FAILED_ALLOCATOR,std::memory_order_release)
                    && ((COM_BUFFER_STATE)(p->load()) != FAILED_ALLOCATOR));
}

//
static inline void useless_wait() {}

//
// only one process/thread should own this position, the `read_marker`.
// The only contention will be that some process/thread will inspect the buffer to see if there is a job there.
// waiting for the state to be CLEAR_FOR_WRITE
//
inline bool wait_to_write(atomic<COM_BUFFER_STATE> *read_marker,uint16_t loops = MAX_WAIT_LOOPS,void (delay_func)() = useless_wait ) {
    auto p = read_marker;
    uint16_t count = 0;
    while ( true ) {
        count++;
        COM_BUFFER_STATE clear = (COM_BUFFER_STATE)(p->load(std::memory_order_relaxed));
        if ( clear == CLEAR_FOR_WRITE ) break;
        //
        if ( count > loops ) {
            return false;
        }
        delay_func();
    }
    return true;
}


// MAX_WAIT_LOOPS
// await_write_offset(read_marker,MAX_WAIT_LOOPS,4)
//
static inline bool await_write_offset(atomic<COM_BUFFER_STATE> *read_marker,uint16_t loops,void (delay_func)()) {
    loops = min(MAX_WAIT_LOOPS,loops);
    auto p = read_marker;
    //    auto current_marker = p->load();
    uint32_t count = 0;
    while ( true ) {
        count++;
        COM_BUFFER_STATE clear = (COM_BUFFER_STATE)(p->load(std::memory_order_relaxed));
        if ( clear == CLEARED_FOR_COPY ) break;
         if ( clear == FAILED_ALLOCATOR ) {
            return false;
         }
        //
        if ( count > loops ) {
            return false;
        }
        delay_func();
    }
    return true;
}
