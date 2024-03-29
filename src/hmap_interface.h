#ifndef _H_HAMP_INTERFACE_
#define _H_HAMP_INTERFACE_

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>


using namespace std;


#define WORD  (8*sizeof(uint32_t))		// 32 bits
#define MOD(x, n) ((x) < (n) ? (x) : (x) - (n))
//
template<typename T>
inline T CLZ(T x) {		// count leading zeros -- make sure it is not bigger than the type size
	static uint8_t W = sizeof(T)*8;
	return(__builtin_clzl(x) % W);
}

#define FFS(x) (__builtin_ctzl(x))				// count trailing zeros (First Free Space in neighborhood)
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
//



const uint32_t COUNT_MASK = 0x3F;  // up to (64-1)
const uint32_t HI_COUNT_MASK = (COUNT_MASK<<16);
const uint32_t HOLD_BIT_MASK = (0x1 << 7);
const uint32_t FREE_BIT_MASK = ~HOLD_BIT_MASK;
const uint32_t LOW_WORD = 0xFFFF;


const uint32_t HH_SELECT_BIT = 1;

typedef struct HHASH {
	//
	uint32_t _neighbor;		// Number of elements in a neighborhood
	uint32_t _count;			// count of elements contained an any time
	uint32_t _max_n;			// max elements that can be in a container
	uint32_t _control_bits;

	uint32_t _H_Offset;
	uint32_t _V_Offset;
	uint32_t _C_Offset;

	/**
	 * Accept the value in the hopscotch table
	*/
	void pin_value() {

	}

	/**
	 * 
	*/
	void unpin_value() {
		//
	}
	uint16_t bucket_count(uint32_t h_bucket) {			// at most 255 in a bucket ... will be considerably less
		uint32_t *controllers = (uint32_t *)(static_cast<char *>((void *)(this)) + sizof(struct HHASH) + _C_Offset);
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
		virtual uint64_t store(uint32_t hash_bucket, uint32_t el_key, uint32_t v_value) = 0;
		virtual uint32_t get(uint64_t key) = 0;
		virtual uint32_t get(uint32_t key,uint32_t bucket) = 0;
		virtual uint8_t get_bucket(uint32_t h, uint32_t xs[32]) = 0;
		virtual uint32_t del(uint64_t key) = 0;
		virtual void	 clear(void) = 0;
};





#endif // _H_HAMP_INTERFACE_