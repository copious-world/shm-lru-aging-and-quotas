#ifndef _H_HAMP_INTERFACE_
#define _H_HAMP_INTERFACE_

#pragma once

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>

#include <bit>

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
const uint64_t HASH_MASK = (((uint64_t)0) | ~(uint32_t)(0));  // 32 bits
//
#define BitsPerByte 8
#define HALF (sizeof(uint32_t)*BitsPerByte)  // should be 32
#define QUARTER (sizeof(uint16_t)*BitsPerByte) // should be 16
#define EIGHTH (sizeof(uint8_t)*BitsPerByte) // should be 8
//


const uint32_t LOW_WORD = 0xFFFF;

//
// The control bit and the shared bit are mutually exclusive.
// They can both be off at the same time, but not both on at the same time.
//

// HH control buckets
const uint32_t SHARED_BIT_SHIFT = 31;
const uint32_t THREAD_ID_SHIFT = 24;
const uint32_t HOLD_BIT_SHIFT = 23;
const uint32_t DBL_COUNT_MASK_SHIFT = 16;

const uint32_t DOUBLE_COUNT_MASK_BASE = 0x3F;  // up to (64-1)
const uint32_t DOUBLE_COUNT_MASK = (DOUBLE_COUNT_MASK_BASE << DBL_COUNT_MASK_SHIFT);
const uint32_t HOLD_BIT_SET = (0x1 << HOLD_BIT_SHIFT);
const uint32_t FREE_BIT_MASK = ~HOLD_BIT_SET;

const uint32_t SHARED_BIT_SET = (0x1 << SHARED_BIT_SHIFT);
const uint32_t QUIT_SHARE_BIT_MASK = ~SHARED_BIT_SET;   // if 0 then thread_id == 0 means no thread else one thread. If 1, the XOR is two.

const uint32_t THREAD_ID_BASE = (uint32_t)0x07F;
const uint32_t THREAD_ID_SECTION = (THREAD_ID_BASE << THREAD_ID_SHIFT);
const uint32_t THREAD_ID_SECTION_CLEAR_MASK = (~THREAD_ID_SECTION & ~SHARED_BIT_SET);

const uint32_t COUNT_MASK = 0x1F;  // up to (32-1)
const uint32_t HI_COUNT_MASK = (COUNT_MASK<<8);
//

const uint32_t HOLD_BIT_ODD_SLICE = (0x1 << (7+8));
const uint32_t FREE_BIT_ODD_SLICE_MASK = ~HOLD_BIT_ODD_SLICE;

const uint32_t HOLD_BIT_EVEN_SLICE = (0x1 << (7));
const uint32_t FREE_BIT_EVEN_SLICE_MASK = ~HOLD_BIT_EVEN_SLICE;




static inline uint32_t add_thread_id(uint32_t target, uint32_t thread_id) {
	auto rethread_id = thread_id & THREAD_ID_BASE;
	if ( rethread_id != thread_id ) return 0;
	rethread_id = (rethread_id << THREAD_ID_SHIFT);
	if ( target & HOLD_BIT_SET ) {
		if ( target & SHARED_BIT_SET ) return 0;
		target = target | SHARED_BIT_SET;
		target = (target & THREAD_ID_SECTION_CLEAR_MASK) | ((target & THREAD_ID_SECTION) ^ rethread_id);
	} else {
		target = target | rethread_id;
	}
	return target;
}


static inline uint32_t remove_thread_id(uint32_t target, uint32_t thread_id) {
	auto rethread_id = thread_id & THREAD_ID_BASE;
	if ( rethread_id != thread_id ) return 0;
	if ( target & HOLD_BIT_SET ) {
		rethread_id = rethread_id << THREAD_ID_SHIFT;
		if ( target & SHARED_BIT_SET ) {
			target = target & QUIT_SHARE_BIT_MASK;
			target = (target & THREAD_ID_SECTION_CLEAR_MASK) | ((target & THREAD_ID_SECTION) ^ rethread_id);
		} else {
			target = target & QUIT_SHARE_BIT_MASK;
			target = (target & THREAD_ID_SECTION_CLEAR_MASK);
		}
	}
	return target;
}



static inline uint32_t get_partner_thread_id(uint32_t target, uint32_t thread_id) {
	auto rethread_id = thread_id & THREAD_ID_BASE;
	if ( rethread_id != thread_id ) return 0;
	if ( target & HOLD_BIT_SET ) {
		if ( target & SHARED_BIT_SET ) {
			uint32_t partner_id = target & THREAD_ID_SECTION;
			partner_id = (partner_id >> THREAD_ID_SHIFT) & THREAD_ID_BASE;
			partner_id = (partner_id ^ rethread_id);
			return partner_id;
		} else {
			return 0;
		}
	}
	return 0;
}



// select bits for the key

const uint32_t HH_SELECT_BIT = (1 << 24);
const uint32_t HH_SELECT_BIT_MASK = (~HH_SELECT_BIT);
const uint64_t HH_SELECT_BIT64 = (1 << 24);
const uint64_t HH_SELECT_BIT_MASK64 = (~HH_SELECT_BIT64);


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




class HMap_interface {
	public:
		virtual uint64_t	update(uint32_t hash_bucket, uint32_t el_key, uint32_t v_value,uint8_t thread_id = 1) = 0;
		virtual uint32_t	get(uint64_t key,uint8_t thread_id = 1) = 0;
		virtual uint32_t	get(uint32_t key,uint32_t bucket,uint8_t thread_id = 1) = 0;
		virtual uint8_t		get_bucket(uint32_t h, uint32_t xs[32]) = 0;
		virtual uint32_t	del(uint64_t key,uint8_t thread_id = 1) = 0;
		virtual void		clear(void) = 0;
		virtual uint64_t	add_key_value(uint32_t el_key,uint32_t h_bucket,uint32_t offset_value,uint8_t thread_id = 1) = 0;
		virtual void		set_random_bits(void *shared_bit_region) = 0;
};





#endif // _H_HAMP_INTERFACE_