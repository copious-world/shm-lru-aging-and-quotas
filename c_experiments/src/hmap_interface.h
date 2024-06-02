#ifndef _H_HAMP_INTERFACE_
#define _H_HAMP_INTERFACE_

#pragma once

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>

#include <bit>
#include <atomic>

using namespace std;


constexpr int SECS_TO_SLEEP = 3;
constexpr int NSEC_TO_SLEEP = 3;


struct timespec request {
	SECS_TO_SLEEP, NSEC_TO_SLEEP
}, remaining{SECS_TO_SLEEP, NSEC_TO_SLEEP};




template<typename T>
inline string joiner(list<T> &jlist) {
	if ( jlist.size() == 0 ) {
		return("");
	}
	stringstream ss;
	for ( auto v : jlist ) {
		ss << v;
		ss << ',';
	}
	string out = ss.str();
	return(out.substr(0,out.size()-1));
}

template<typename K,typename V>
inline string map_maker_destruct(map<K,V> &jmap) {
	if ( jmap.size() == 0 ) {
		return "{}";
	}
	stringstream ss;
	char del = 0;
	ss << "{";
	for ( auto p : jmap ) {
		if ( del ) { ss << del; }
		del = ',';
		K h = p.first;
		V v = p.second;
		ss << "\""  << h << "\" : \""  << v << "\"";
		delete p.second;
	}
	ss << "}";
	string out = ss.str();
	return(out.substr(0,out.size()));
}



static inline uint32_t now_time() {
	//
	uint32_t nowish = 0;
	const auto right_now = chrono::system_clock::now();
	nowish = chrono::system_clock::to_time_t(right_now);
	//
	return nowish;
}



#define WORD  (8*sizeof(uint32_t))		// 32 bits
#define BIGWORD (8*sizeof(uint64_t))
#define MOD(x, n) ((x) < (n) ? (x) : (x) - (n))
//
template<typename T>
inline T CLZ(T x) {		// count leading zeros -- make sure it is not bigger than the type size
	static uint8_t W = sizeof(T)*8;
	return(__builtin_clzl(x) % W);
}

//#define FFS(x) (__builtin_ctzl(x))				// __builtin_ Count Trailing Zeros (First Free Space in neighborhood) Long
#define FLS(x) WORD // (WORD - CLZ(x))			// number bits possible less leading zeros (limits the space of the neigborhood)
#define GET(hh, i) ((hh) & (1L << (i)))			// ith bit returned   (hh for hash home)
#define SET(hh, i) (hh = (hh) | (1L << (i)))	// or in ith bit (ith bit set - rest 0)
#define UNSET(hh, i) (hh = (hh) & ~(1L << (i)))	// and with ith bit 0 - rest 1 (think of as mask)
//
//
#define BitsPerByte 8
#define HALF (sizeof(uint32_t)*BitsPerByte)  // should be 32
#define QUARTER (sizeof(uint16_t)*BitsPerByte) // should be 16
#define EIGHTH (sizeof(uint8_t)*BitsPerByte) // should be 8
//


const uint32_t LOW_WORD = 0xFFFF;
const uint64_t HASH_MASK = (((uint64_t)0) | LOW_WORD);  // 32 bits



// CBIT LOCKING -- control bits fill the role of hopscotch membership patterns and an be swapped for 

// a base root is a bucket to which some element has directly hashed without collision.
// a base member is a bucket that stores an element that has collided with an existing base root and must be stored at an offset from the base

const uint32_t CBIT_INACTIVE_BASE_ROOT_BIT = 0x1;
const uint32_t CBIT_BASE_MEMBER_BIT = (0x1 << 31);
const uint32_t CBIT_BASE_INOP_BITS = (CBIT_INACTIVE_BASE_ROOT_BIT | CBIT_BASE_MEMBER_BIT);

//
const uint32_t CBIT_BACK_REF_BITS = 0x3E;
const uint32_t CBIT_BACK_REF_CLEAR_MASK = (~((uint32_t)0x003E));
//
const uint32_t EDITOR_CBIT_SET =		((uint32_t)(0x0001) << 8);
const uint32_t READER_CBIT_SET =		((uint32_t)(0x0001) << 9);
const uint32_t USURPED_CBIT_SET =		((uint32_t)(0x0001) << 10);
const uint32_t MOBILE_CBIT_SET =		((uint32_t)(0x0001) << 11);
const uint32_t DELETE_CBIT_SET =		((uint32_t)(0x0001) << 12);
const uint32_t IMMOBILE_CBIT_SET =		((uint32_t)(0x0001) << 13);

// 
const uint32_t READER_BIT_RESET = ~(READER_CBIT_SET);

const uint32_t TBIT_ACTUAL_BASE_ROOT_BIT = 0x1;
const uint32_t TBIT_SEM_COUNTER_MASK = (0xFE);
const uint32_t TBIT_SEM_COUNTER_CLEAR_MASK = (~((uint32_t)0x00FE));


// EDITOR BIT and READER BIT

const uint32_t TBIT_READER_SEMAPHORE_CLEAR = 0x00FFFFFF;
const uint32_t TBIT_READER_SEMAPHORE_WORD = 0xFF000000;
const uint32_t TBIT_READER_SEMAPHORE_SHIFT = 24;
const uint32_t TBIT_READ_MAX_SEMAPHORE = 31;



const uint32_t CBIT_THREAD_SHIFT = 16;



// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


inline bool is_base_tbit(uint32_t tbits) {
	auto bit_state = (tbits & TBIT_ACTUAL_BASE_ROOT_BIT);
	return (bit_state != 0);
}


// All the following bit operations occur in registers. Storage is handled atomically around them.
//

inline bool is_base_noop(uint32_t cbits) {
	auto bit_state = (cbits & CBIT_INACTIVE_BASE_ROOT_BIT);
	return (bit_state != 0);
}

inline bool is_base_in_ops(uint32_t cbits) {
	return ((CBIT_BASE_MEMBER_BIT & cbits) == 0);
}

inline bool is_empty_bucket(uint32_t cbits) {
	return (cbits == 0);
}

inline bool is_member_bucket(uint32_t cbits) {
	auto chck = (cbits & CBIT_BASE_INOP_BITS);
	return (chck == CBIT_BASE_MEMBER_BIT);
}


// THREAD OWNER ID CONTROL

inline uint32_t cbits_thread_id_of(uint32_t cbits) {
	auto thread_id = (cbits >> CBIT_THREAD_SHIFT) & 0x00FF;
	return thread_id;
}

inline uint32_t cbit_thread_stamp(uint32_t cbits,uint8_t thread_id) {
	cbits = cbits | ((thread_id & 0x00FF) << CBIT_THREAD_SHIFT);
	return cbits;
}

inline uint32_t cbit_clear_bit(uint32_t cbits,uint8_t i) {
	UNSET(cbits, i);
	return cbits;
}

// THREAD OWNER OF TBITS for readers


inline uint32_t tbits_thread_id_of(uint32_t tbits) {
	return cbits_thread_id_of(tbits);
}

inline uint32_t tbit_thread_stamp(uint32_t cbits,uint8_t thread_id) {
	return cbit_thread_stamp(cbits,thread_id);
}

// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// HANDLE THE READER SEMAPHORE which is useful around deletes and some states of inserting.
//
inline uint8_t base_reader_sem_count(uint32_t tbits) {
	if ( is_base_tbit(tbits) ) {
		auto semcnt = tbits & 0xFE; 
		return semcnt;
	}
	return 0; // has to be greater than or equal to 1. 
}


inline bool tbits_sem_at_max(uint32_t tbits) {
	auto semcnt = ((uint8_t)tbits & 0xFE);
	return (semcnt == 0xFE); // going by multiples of two to keep the low bit zero.
}


inline bool tbits_sem_at_zero(uint32_t tbits) {
	auto semcnt = ((uint8_t)tbits & 0xFE);
	return (semcnt == 0); // going by multiples of two to keep the low bit zero.
}


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


// MEMBER BACKREF to the base bucket

inline uint8_t member_backref_offset(uint32_t cbits) {
	if ( is_member_bucket(cbits) ) {
		auto bkref = (cbits >> 1) & 0x1F; 
		return bkref;
	}
	return 0; // has to be greater than or equal to 1. 
}


inline uint8_t gen_bitsmember_backref_offset(uint32_t cbits,uint8_t bkref) {
	if ( is_member_bucket(cbits) ) {
		cbits = (CBIT_BACK_REF_CLEAR_MASK & cbits) | (bkref << 1);
	}
	return cbits; // has to be greater than or equal to 1. 
}


class hh_element;
template<class HHE>
HHE *cbits_base_from_backref(uint32_t cbits,uint8_t &backref,HHE *from,HHE *begin,HHE *end) {
	backref = ((cbits & CBIT_BACK_REF_BITS) >> 1);
	from -= backref;
	if ( backref < begin ) {
		from = end - (begin - from);
	}
	return from;
}


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


inline bool base_in_operation(uint32_t cbits) {
	if ( is_base_in_ops(cbits) ) {
		return (cbits & EDITOR_CBIT_SET) || (cbits & READER_CBIT_SET);
	}
	return false;
}


// ---- ---- ---- 

inline bool editors_are_active(uint32_t cbits) {
	if ( is_base_in_ops(cbits) ) {
		auto chck = (cbits & EDITOR_CBIT_SET);
		return chck;
	}
	return false;
}

inline uint32_t gen_bits_editor_active(uint8_t thread_id,uint32_t cbits = 0) {
	auto rdr = (cbits | EDITOR_CBIT_SET);
	rdr = cbit_thread_stamp(rdr,thread_id);
	return rdr;
}


inline bool is_member_usurped(uint32_t cbits,uint8_t &thread_id) {
	if ( is_member_bucket(cbits) ) {
		if ( cbits & USURPED_CBIT_SET ) {
			thread_id = cbits_thread_id_of(cbits);
		}
	}
	return false;
}

inline bool is_cbits_usurped(uint32_t cbits) {
	if ( cbits & USURPED_CBIT_SET ) {
		return true;
	}
	return false;
}


inline uint32_t gen_bitsmember_usurped(uint8_t thread_id,uint32_t cbits) {
	if ( is_member_bucket(cbits) ) {
		auto rdr = (cbits | USURPED_CBIT_SET);
		rdr = cbit_thread_stamp(rdr,thread_id);
		return rdr;
	}
}



inline bool is_member_in_mobile_predelete(uint32_t cbits) {
	if ( is_member_bucket(cbits) ) {
		if ( cbits & MOBILE_CBIT_SET ) {
			return true;
		}
	}
	return false;
}


inline bool is_cbits_in_mobile_predelete(uint32_t cbits) {
	if ( cbits & MOBILE_CBIT_SET ) {
		return true;
	}
	return false;
}


inline uint32_t gen_bitsmember_in_mobile_predelete(uint8_t thread_id,uint32_t cbits) {
	if ( is_member_bucket(cbits) ) {
		auto rdr = (cbits | MOBILE_CBIT_SET);
		rdr = cbit_thread_stamp(rdr,thread_id);
		return rdr;
	}
}



// is_deleted
// cropped and unacessible but not yet clear in maps and memberships...
// cropping clears the taken positions of leaving elements.
//
// after clearing the space has been reclaimed and buckets are empty


inline bool is_deleted(uint32_t cbits) { 
	if ( is_member_bucket(cbits) ) {
		if ( cbits & DELETE_CBIT_SET ) {
			return true;
		}
	}
	return false;
}



inline bool is_cbits_deleted(uint32_t cbits) {
	if ( cbits & DELETE_CBIT_SET ) {
		return true;
	}
	return false;
}



inline uint32_t gen_bitsdeleted(uint8_t thread_id,uint32_t cbits) {
	auto rdr = (cbits | DELETE_CBIT_SET);
	rdr = cbit_thread_stamp(rdr,thread_id);
	return rdr;
}



// KEY (as returned to the user application will use selector bits to search)

// select bits for the key
const uint32_t HH_SELECT_BIT_SHIFT = 30;
const uint32_t HH_SELECTOR_SET_BIT = (1 << (HH_SELECT_BIT_SHIFT + 1));
const uint32_t HH_SELECTOR_SET_BIT_MASK = (~HH_SELECTOR_SET_BIT);
//
const uint32_t HH_SELECTION_BIT = (1 << HH_SELECT_BIT_SHIFT);
const uint32_t HH_SELECTION_BIT_MASK = (~HH_SELECTION_BIT);
//
const uint64_t HH_SELECTOR_SET_BIT64 = (1 << (HH_SELECT_BIT_SHIFT + 1));
const uint64_t HH_SELECTOR_SET_BIT_MASK64 = (~HH_SELECTOR_SET_BIT64);


const uint32_t HH_SELECT_BIT_INFO_MASK = (HH_SELECTION_BIT_MASK & HH_SELECTOR_SET_BIT_MASK);
// ----

static inline bool selector_bit_is_set(uint64_t hash64,uint8_t &selector) {
	if ( (hash64 & HH_SELECTOR_SET_BIT64) != 0 ) {
		if ( selector != 0x3 ) {
			selector = hash64 & HH_SELECTION_BIT ? 1 : 0;
			return true;
		}
	}
	return false;
}


static inline bool selector_bit_is_set(uint32_t hash_bucket,uint8_t &selector) {
	if ( (hash_bucket & HH_SELECTOR_SET_BIT) != 0 ) {
		if ( selector != 0x3 ) {
			selector =  hash_bucket & HH_SELECTION_BIT ? 1 : 0;
			return true;
		}
	}
	return false;
}

/**
 * stamp_key
 * 
 * 		The full hash is stored in the top part of a 64 bit word. While the bucket information
 * 		is stored in the lower half.
 * 
 * 		| full 32 bit hash || control bits (2 to 4) | bucket number |
 * 		|   32 bits			| 32 bits ---->					        |
 * 							| 4 bits				| 28 bits		|  // 28 bits for 500 millon entries
 * 
*/
static inline uint32_t stamp_key(uint32_t h_bucket,uint8_t info) {
	uint32_t info32 = info | 0x2;   // info will be 0 or 1 unless the app is special (second bit is the selection set indicator)
	info32 <<= HH_SELECT_BIT_SHIFT;
	return (h_bucket | info32);
}

/**
 * clear_selector_bit
*/
static inline uint32_t clear_selector_bit(uint32_t h) {
	h = (h & HH_SELECT_BIT_INFO_MASK);  // clear both bits
	return h;
}





typedef struct BucketSliceStats {
	uint8_t			count	: 5;
	uint8_t			busy	: 1;
	uint8_t			mod		: 1;
	uint8_t			memb	: 1;
} buckets;


typedef struct ControlBits {
	buckets			_even;
	buckets			_odd;
	//
	uint8_t			busy	: 1;
	uint8_t			shared	: 1;
	uint8_t			count	: 6; // shared count
	uint8_t			thread_id;   // some of this has to do with the cache line...
} control_bits;



typedef struct HHASH {
	//
	uint32_t _neighbor;		// Number of elements in a neighborhood
	uint32_t _count;			// count of elements contained an any time
	uint32_t _max_n;			// max elements that can be in a container
	uint32_t _control_bits;

	uint32_t _HV_Offset;
	uint32_t _C_Offset;


	uint16_t bucket_count(uint32_t h_bucket) {			// at most 255 in a bucket ... will be considerably less
		uint32_t *controllers = (uint32_t *)(static_cast<char *>((void *)(this)) + sizeof(struct HHASH) + _C_Offset);
		uint16_t *controller = (uint16_t *)(&controllers[h_bucket]);
		//
		uint8_t my_word = _control_bits & 0x1;
		uint16_t count = my_word ? controller[1] : controller[0];
		return (count & COUNT_MASK);
	}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

} HHash;




class Spinners {

	public:
		Spinners(): _flag(ATOMIC_FLAG_INIT) {}

		virtual ~Spinners(void) { unlock(); }

		void lock(){
			while ( _flag.test_and_set() ) __libcpp_thread_yield();
		}

		void unlock(){
			_flag.clear();
		}

		void wait() {
			while ( _flag.test(std::memory_order_acquire) ) __libcpp_thread_yield();
			_flag.test_and_set();
		}

		void signal() {
			_flag.clear();
		}

	private:
		atomic_flag _flag;
};





class HMap_interface {
	public:
		virtual void 		value_restore_runner(uint8_t slice_for_thread) = 0;
		virtual void		random_generator_thread_runner(void) = 0;
		virtual uint64_t	update(uint32_t el_match_key, uint32_t hash_bucket, uint32_t v_value,uint8_t thread_id = 1) = 0;
		virtual uint32_t	get(uint64_t augemented_hash,uint8_t thread_id = 1) = 0;
		virtual uint32_t	get(uint32_t el_match_key,uint32_t hash_bucket,uint8_t thread_id = 1) = 0;
		virtual uint32_t	del(uint64_t augemented_hash,uint8_t thread_id = 1) = 0;
		virtual uint32_t	del(uint32_t el_match_key,uint32_t hash_bucket,uint8_t thread_id = 1) = 0;
		virtual void		clear(void) = 0;
		virtual uint64_t	add_key_value(uint32_t el_match_key,uint32_t hash_bucket,uint32_t offset_value,uint8_t thread_id = 1) = 0;
		virtual void		set_random_bits(void *shared_bit_region) = 0;
		//
		virtual bool		prepare_for_add_key_value_known_refs(atomic<uint32_t> **control_bits_ref,uint32_t h_bucket,uint8_t thread_id,uint8_t &which_table,uint32_t &cbits,hh_element **bucket_ref,hh_element **buffer_ref,hh_element **end_buffer_ref) = 0;
		virtual bool		wait_if_unlock_bucket_counts(uint32_t h_bucket, uint8_t thread_id, uint8_t &which_table) = 0;
		virtual uint64_t	add_key_value_known_refs(atomic<uint32_t> *control_bits,uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t which_table = 0,uint8_t thread_id,uint32_t cbits,hh_element *bucket,hh_element *buffer,hh_element *end_buffer) = 0;
};





#endif // _H_HAMP_INTERFACE_