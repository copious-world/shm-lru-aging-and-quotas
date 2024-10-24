#pragma once

#include "errno.h"

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
#include <tuple>


using namespace std;

#include "random_selector.h"
#include "atomic_proc_rw_state.h"
#include "holey_buffer.h"

#include "worker_waiters.h"

using namespace std::chrono;


#define MAX_BUCKET_FLUSH 12

#define ONE_HOUR 	(60*60*1000)

// ---- ---- ---- ---- ---- ---- ---- ---- ----
//
#define NUM_ATOMIC_FLAG_OPS_PER_TIER		(4)
#define LRU_ATOMIC_HEADER_WORDS				(8)


/**
 * The 64 bit key stores a 32bit hash (xxhash or other) in the lower word.
 * The top 32 bits stores a structured bit array. The top 1 bit will be
 * the selector of the hash region where the key match will be found. The
 * bottom 20 bits will store the element offset (an element number) to the position the
 * element data is stored. 
*/


/**
 * The atomic stack is a fixed size memory allocation scheme. 
 * The intention is that elements will be moved out by their age if the stack becomes full.
 * 
*/


inline uint64_t epoch_ms(void) {
	uint64_t ms;
	ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	return ms;
}


inline uint32_t now(void) {
	uint32_t ms;
	ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	return ms;
}


uint32_t g_prev_time = 0;
uint32_t g_interval_counter = 0;
//
inline uint32_t current_time_next() {
	//
	uint32_t probe_time = epoch_ms();
	if ( g_prev_time == probe_time ) {
		g_interval_counter++;
	} else {
		g_interval_counter = 0;
		g_prev_time = probe_time;
	}
	//
	uint32_t timemarker = (g_interval_counter & (~0x1111)) | (uint32_t)(g_prev_time << 4);
	return timemarker;
}

void init_diff_timer() {
	g_prev_time = (uint32_t)epoch_ms();
	g_interval_counter = 0;
}

const uint32_t MAX_MESSAGE_SIZE = 128;
//
const uint32_t OFFSET_TO_MARKER = 0;					// in bytes
const uint32_t OFFSET_TO_OFFSET = sizeof(uint32_t);		// 
const uint32_t OFFSET_TO_HASH = (OFFSET_TO_OFFSET + sizeof(uint32_t));   // start of 64bits

const uint32_t TOTAL_ATOMIC_OFFSET = (OFFSET_TO_HASH + sizeof(uint64_t));	
//
const uint32_t DEFAULT_MICRO_TIMEOUT = 2; // 2 seconds



// ----

typedef enum STP_table_choice {
	STP_TABLE_HH,
	STP_TABLE_SLABS,
	STP_TABLE_QUEUED,
	STP_TABLE_INTERNAL_ONLY,
	STP_TABLE_EMPTY_SHELL_TEST
} stp_table_choice;

// uint64_t hash is the augmented hash...

// five fences (??)
typedef struct COM_ELEMENT {
	atomic<COM_BUFFER_STATE>	_marker;	// control over accessing the message buffer
	uint8_t						_proc;		// the index of the owning proc
	uint8_t						_tier;		// the index of the managed tier (this com element)
	uint8_t						_ops;		// read/write/delete and other flags. 
	//
	uint64_t					_hash;		// the hash of the message
	uint32_t					_offset;	// offset to the LRU element... where data will be stored
	uint32_t					_timestamp;	// control over accessing the message buffer
	char						_message[MAX_MESSAGE_SIZE];   // 64*2
} Com_element;


typedef union {
	uint32_t			_offset;
	Com_element			*_cel;
} com_or_offset;


typedef struct LRU_ELEMENT_HDR {
	uint32_t	_info;
	uint32_t	_next;
	uint64_t 	_hash;
	time_t		_when;
	uint32_t	_share_key;

	void		init(uint64_t hash_init = UINT64_MAX) {
		this->_info = UINT32_MAX;
		this->_hash = hash_init;
		this->_when = 0;
	}

} LRU_element;



typedef struct _LRU_Alloc_Sections_and_Threads_ {
	
	bool			_run_random_upates_threads{false};
	bool			_run_restore_threads{false};
	bool			_run_cropper_threads{false};
	bool			_test_no_run_evictor_threads{false};
	//
	bool			_alloc_randoms;		// do allocation and launch threads
	bool			_alloc_hash_tables;	// do allocation and launch threads
	uint8_t			_num_hash_tables;	// usually one or two (sparse may be more)
	uint8_t			_num_initial_typed_slab{0};	// usually one or two (sparse may be more)
	//
	uint8_t			_num_tiers;
	//
	bool			_alloc_secondary_com_buffer{false}; // for the queued case

} LRU_Alloc_Sections_and_Threads;


//
//	LRU_cache --
//
//	Interleaved free memory is a stack -- fixed sized elements
//


class LRU_Consts {

	public: 

		LRU_Consts() {
			_NTiers = 0;
		}

		virtual ~LRU_Consts() {}

	public:

		static const uint8_t	NUM_ATOMIC_OPS{NUM_ATOMIC_FLAG_OPS_PER_TIER};			

		uint32_t			_NTiers;
		uint32_t			_Procs;
		bool				_am_initializer;



		/**
		 * check_expected_lru_region_size
		*/

		static uint32_t check_expected_lru_region_size(size_t record_size, size_t els_per_tier, uint32_t num_procs) {
			//
			size_t this_tier_atomics_sz = LRU_ATOMIC_HEADER_WORDS*sizeof(atomic<uint32_t>);  // currently 5 accessed
			size_t com_reader_per_proc_sz = sizeof(Com_element)*num_procs;
			size_t max_count_lru_regions_sz = (sizeof(LRU_element) + record_size)*(els_per_tier + 2);
			// _max_count*2 + num_procs
			size_t holey_buffer_sz = Shared_KeyValueManager::check_expected_holey_buffer_size(els_per_tier,num_procs); // storage for timeout management

			uint32_t predict = (this_tier_atomics_sz + com_reader_per_proc_sz + max_count_lru_regions_sz + holey_buffer_sz);
			//
			return predict;
		}


		static uint32_t check_expected_com_size(uint32_t num_procs, uint32_t num_tiers) {
			size_t tier_atomics_sz = NUM_ATOMIC_OPS*num_tiers*sizeof(atomic_flag *);  // ref to the atomic flag
			size_t proc_tier_com_sz = sizeof(Com_element)*num_procs*num_tiers;
			uint32_t seg_size = tier_atomics_sz + proc_tier_com_sz;
			return seg_size;
		}

};


