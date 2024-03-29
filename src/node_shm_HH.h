#ifndef _H_HOPSCOTCH_HASH_SHM_
#define _H_HOPSCOTCH_HASH_SHM_

#include "node.h"
#include "node_buffer.h"
#include "v8.h"
#include "nan.h"
#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>
#include <thread>
#include <ctime>
#include <atomic>
#include <thread>
#include <mutex>

#include <random>
#include <bitset>

#include <map>
#include <unordered_map>
#include <list>
#include <vector>

 
#include "hmap_interface.h"



constexpr int SECS_TO_SLEEP = 3;
constexpr int NSEC_TO_SLEEP = 3;

struct timespec request {
	SECS_TO_SLEEP, NSEC_TO_SLEEP
}, remaining{SECS_TO_SLEEP, NSEC_TO_SLEEP};



/*
static const std::size_t S_CACHE_PADDING = 128;
static const std::size_t S_CACHE_SIZE = 64;
std::uint8_t padding[S_CACHE_PADDING - (sizeof(Object) % S_CACHE_PADDING)];
*/



// USING
using namespace node;
using namespace v8;
using namespace std;




// Bringing in code from libhhash  // until further changes...


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


// ---- ---- ---- ---- ---- ----  HHash
// HHash <- HHASH



class HH_map;

/**
 * Two threads will be given access to separate hopscotch tables... 
*/
class ThreadWrapper {


	public:
		ThreadWrapper() : _stopped(false), _ready(false), _lk(_mtx), _w_thread(&ThreadWrapper::runner,this) {
			_hh_container = nullptr;
		}
		virtual ~ThreadWrapper() {
			_stopped = true;
			_cv.notify_one();
			_w_thread.join();
		}

		/**
		 * Prepare the thread (should not be busy) with the value that it will store once it can place the value with
		 * the future hash
		*/
		void prep_value(uint32_t value) {
			lock_guard _lk(_mtx);
			_last_value = value;
		}

		/**
		 * 
		*/
		void signal(uint64_t hash) {  // awaken local threads and aprise them of the requested hash
			_hash = hash;
			_ready = true
			_cv.notify_one();
		}

		/**
		 * 
		*/
		void runner() {
			while (true) {
				_lk.lock();
				cv.wait(_lk, []{ return _ready || _stopped; });
				if ( _stopped ) break;
				else _ready = false;
				//
				uint64_t hash = _hash;
				uint32_t value = _last_value;

				if ( _hh_container != nullptr ) {
					// adding the value to the table... in this region...
				}
				_lk.unlock();
			}
		}


		void set_hh(HHash *hc) {
			_hh_container = hc;		// may be nullptr
		}


	public:

		thread				_w_thread;
		mutex				_mtx;
		condition_variable	_cv;
		unique_lock 		_lk;
		bool				_ready;
		bool				_stopped;

		uint64_t			_hash;
		uint32_t			_last_value;
		HHash				*_hh_container;
		

}

#define  BITS_GEN_COUNT_LAPSE (3)

class Random_bits_generator {

    bernoulli_distribution	_distribution;
	list<uint32_t>			_bits;

	mt19937 				_engine;
	uint32_t				_last_bits;
	uint8_t					_bcount;
	uint8_t					_gen_count;

public:

	Random_bits_generator() : _bcount(0), _gen_count(0) {
		_create_random_bits();
	}

public:


	uint32_t generate_word() {
		uint32_t bits = 0;
		for ( uint8_t i = 0; i < 32; i++ ) {
			bool bit = _distribution(_engine);
			bits = (bits << 1) | (bit ? 1 : 0);
		}
		return bits;
	}

    template <typename OutputIt>
    void generate(OutputIt first, OutputIt last)
    {
        while ( first != last ) {
			uint32_t bits = generate_word();
            *first++ = bits;
        }
    }


	void _create_random_bits() {
		_bits.resize(256);   // fr list ??
		generate(_bits.begin(), _bits.end());
	}

	uint8_t pop_bit() {
		//
		if ( _bcount == 0 ) {
			_last_bits = _bits.pop_front();
			_gen_count = (_gen_count + 1) % BITS_GEN_COUNT_LAPSE;
			if ( _gen_count == 0 ) {
				for ( uint8_t i = 0; i < BITS_GEN_COUNT_LAPSE; i++ ) {
					_bits.push_back(generate_word());
				}
			}
		}
		//
		_bcount = _bcount++ % 32;
		uint8_t the_bit = _last_bits & 0x1;
		_last_bits = (_last_bits >> 1);
		//
		return the_bit;
	}


};


class HH_map : public HMap_interface, public Random_bits_generator {
	//
	public:

		// LRU_cache -- constructor
		HH_map(void *region,uint32_t max_element_count,bool am_initializer = false) {
			_reason = "OK";
			_region = (uint8_t *)region;
			_status = true;
			_initializer = am_initializer;
			_max_count = max_element_count;
			uint8_t sz = sizeof(HHash);
			uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
			// initialize from constructor
			this->setup_region(am_initializer,header_size,(max_element_count/2));
		}



		// THREAD CONTROL

		void tick() {
			nanosleep(&request, &remaining);
		}

		void sleepy_atomic_load_winner_thread_id() {

		}





/*


template< size_t size>
typename std::bitset<size> random_bitset( double p = 0.5) {

    typename std::bitset<size> bits;
    std::random_device rd;
    std::mt19937 gen( rd());
    std::bernoulli_distribution d( p);

    for( int n = 0; n < size; ++n) {
        bits[ n] = d( gen);
    }

    return bits;
}


template <typename Engine = std::mt19937>
class random_bits_generator
{
    std::bernoulli_distribution distribution;
public:
    template <typename OutputIt>
    void operator()(OutputIt first, OutputIt last)
    {
        while (first != last)
        {
            *first++ = distribution(engine);
        }
    }

    Engine get()
    {
        return engine;
    }
};
*/

		_pop_random_bits() {
			return pop_bit();
		}


		/**
		 * 
		*/
		pair<uint16_t,uint16_t> bucket_counts(uint32_t h_bucket) {
			//
			pair<uint16_t,uint16_t> counts;
			uint8_t *start = _region;

			uint32_t c_offset = _C_Offset;
			uint32_t *controllers = (uint32_t *)(start + sizof(struct HHASH) + c_offset);
			//
			auto controller = static_cast<atomic<uint32_t>*>(&controllers[h_bucket]);
			//
			uint32_t controls = controller->load(std::memory_order_consume);
			while ( controls & HOLD_BIT_MASK ) {  // while some other process is using this count bucket
				controls = controller->load(std::memory_order_consume);
			}
			//
			while ( !controller->compare_exchange_weak(controls,(controls | HOLD_BIT_MASK)) && !(controls & HOLD_BIT_MASK) );
			//
			counts.first = controls & COUNT_MASK;
			counts.second = (controls>>QUARTER) & COUNT_MASK;
			//
			return (counts);
		}

		/**
		 * 
		*/
		void bucket_count_incr(uint32_t h_bucket,uint8_t which_thread) {
			uint8_t *start = _region;
			//
			uint32_t c_offset = _C_Offset;
			uint32_t *controllers = (uint32_t *)(start + sizof(struct HHASH) + c_offset);
			//
			auto controller = static_cast<atomic<uint32_t>*>(&controllers[h_bucket]);
			//
			uint32_t controls = controller->load(std::memory_order_consume);
			if ( !(controls & HOLD_BIT_MASK) ) {	// should be the only one able to get here on this bucket.
				this->_status = -1;
				return;
			}
			//
			uint16_t counter = 0;
			if ( which_thread == 0 ) {
				counter = controls & COUNT_MASK;
				counter++;
				controls = (controls & ~COUNT_MASK) | (COUNT_MASK & counter);
			} else {
				counter = (controls>>QUARTER) & COUNT_MASK;
				counter++;
				uint32_t update = (counter) << QUARTER) & HI_COUNT_MASK;
				controls = (controls & ~HI_COUNT_MASK) | update;
			}
			//
			controller->store(controls & FREE_BIT_MASK,std::memory_order_release);
		}



		// REGIONS...

		// setup_region -- part of initialization if the process is the intiator..
		void setup_region(bool am_initializer,uint8_t header_size,uint32_t max_count) {
			// ----
			uint8_t *start = _region;
			HHash *T = (HHash *)start;

			_h_tables[0] = T;			// ---- ---- ---- ---- ---- ---- ---- ---- ----

			// # 1
			// ----
			if ( am_initializer ) {
				T->_count = 0;
				T->_max_n = max_count;
				T->_neighbor = FLS(max_count - 1);
				T->_control_bits = 0;
			} else {
				max_count = T->_max_n;	// just in case
			}

			// # 1
			//
			_region_V_1 = (uint64_t *)(start + header_size);  // start on word boundary
			uint32_t v_regions_size = (sizeof(uint64_t)*max_count);
			//
			_region_H_1 = (uint32_t *)(start + header_size + v_regions_size);
			uint32_t h_regions_size = (sizeof(uint32_t)*max_count);
			//
			if ( am_initializer ) {
				// storing at most 4GB hashes as 32 bit with 4GB values as 64 bit
				memset((void *)(start + header_size),0,(h_regions_size + v_regions_size ));
			}

			// # 2
			// ----
			T = _h_tables[1] = (HHash *)(start + h_regions_size + v_regions_size + c_regions_size);
			start = (char *)(&_h_tables[1]);

			if ( am_initializer ) {
				T->_count = 0;
				T->_max_n = max_count;
				T->_neighbor = FLS(max_count - 1);
				T->_control_bits = 1;
			} else {
				max_count = T->_max_n;	// just in case
			}

			// # 2
			//
			_region_V_2 = (uint64_t *)(start + header_size);  // start on word boundary
			uint32_t v_regions_size = (sizeof(uint64_t)*max_count);
			//
			_region_H_2 = (uint32_t *)(start + header_size + v_regions_size);
			uint32_t h_regions_size = (sizeof(uint32_t)*max_count);
			//
			if ( am_initializer ) {
				// storing at most 4GB hashes as 32 bit with 4GB values as 64 bit
				memset((void *)(start + header_size),0,(h_regions_size + v_regions_size));
			}


			_h_tables[0]->_C_Offset = (header_size + v_regions_size + h_regions_size)*2;
			_h_tables[1]->_C_Offset = (header_size + v_regions_size + h_regions_size)*2;
			//
			_C_Offset = (header_size + v_regions_size + h_regions_size)*2;

			if ( am_initializer ) {
				// storing at most 4GB hashes as 32 bit with 4GB values as 64 bit
				memset((void *)(_region + _C_Offset),0,((sizeof(uint32_t)*max_count));
			}

		}




		bool ok(void) {
			return(this->_status);
		}

		//  store    //  hash_bucket, full_hash,   el_key = full_hash
		uint64_t store(uint32_t hash_bucket, uint32_t el_key, uint32_t v_value) {
			if ( v_value == 0 ) return false;
			//
			HHash *T = (HHash *)_region;
			//
			uint64_t loaded_value = (((uint64_t)v_value) << HALF) | el_key;
			bool put_ok = put_hh_hash(T, hash_bucket, loaded_value);
			if ( put_ok ) {
				uint64_t loaded_key = (((uint64_t)el_key) << HALF) | hash_bucket; // LOADED
				return(loaded_key);
			} else {
				return(UINT64_MAX);
			}
		}



		// get
		uint32_t get(uint64_t key) {
			HHash *T = (HHash *)_region;
			return get_hh_map(T, key);
		}

		// get
		uint32_t get(uint32_t key,uint32_t bucket) {  // full_hash,hash_bucket
			HHash *T = (HHash *)_region;
			return (uint32_t)(get_hh_set(T, key, bucket) >> HALF);  // the value is in the top half of the 64 bits.
		}

		

		// bucket probing
		// 
		uint8_t get_bucket(uint32_t h, uint32_t xs[32]) {
			HHash *T = (HHash *)_region;
			uint8_t count = 0;
			uint32_t i = _succ_hh_hash(T, h, 0);
			while ( i != UINT32_MAX ) {
				uint64_t x = get_val_at_hh_hash(T, h, i);  // get ith value matching this hash (collision)
				xs[count++] = (uint32_t)((x >> HALF) & HASH_MASK);
				i = _succ_hh_hash(T, h, i + 1);  // increment i in some sense (skip unallocated holes)
			}
			return count;	// no value  (values will always be positive, perhaps a hash or'ed onto a 0 value)
		}


		// del
		uint32_t del(uint64_t key) {
			HHash *T = (HHash *)_region;
			return del_hh_map(T, key);
		}

		void clear(void) {
			if ( _initializer ) {
				uint8_t sz = sizeof(HHash);
				uint8_t header_size = (sz  + (sz % sizeof(uint32_t)));
				this->setup_region(_initializer,header_size,_max_count);
			}
		}





	// HH_map method - calls partition_get -- 
	//
	/**
	 * the offset_value is taken from the memory allocation ... it is the index into the LRU array of objects
	 * 
	 * The contention here is not between the two threads that each examine different memory regions.
	 * However, other processes may content for the same buckets that these threads attempt to manipulate.
	 * 
	*/
	bool add_key_value(uint64_t hash64,uint32_t offset_value) {
		//
		HHash *T_1 = (HHash *)(&_h_tables[0]);
		HHash *T_2 = (HHash *)(&_h_tables[1]);
		//
		uint32_t h_bucket = (uint32_t)(key & HASH_MASK);
		
		// Threads from this process will not contend with each other.
		// They will work on two different memory sections.
		// The favored should set the value first. 
		//			Check thread IDs in undecided state (can be atomic)


		// About waiting on threads from the current process. Two threads are assigned to the current table.
		// The process will lock a mutex for just those two threads for the tier that has been called upon. 
		// The mutex will lock more globally. It will not interfere with other processes requesting a conversation 
		// with the threads, just current process threads at the curren tier.

		pair<uint16_t,uint16_t> counts = this->bucket_counts(h_bucket);
		uint16_t count_1 = counts.first;		// favor the least full bucket ... but in case something doesn't move try both
		uint16_t count_2 = counts.second;
		//
		uint8_t tfirst = 0;
		uint8_t tsecond = 1;
		if ( count_2 < count_1 ) {
			tfirst = 1;
			tsecond = 0;
		} else if ( count_2 == count_1 ) {
			tfirst = _pop_random_bits();
			tsecond = (tfirst + 1) % 2;
		}
		// ....
		_threads[tfirst].prep_value(offset_value);
		_threads[tfirst].signal(hash64);
		this->tick();						// The favored should set the value first. (gets a head start)
		_threads[tsecond].prep_value(offset_value);
		_threads[tsecond].signal(hash64);
		// ....
		uint8_t which_thread = 2;
		while ( which_thread == 2 ) {
			uint8_t which_thread = this->sleepy_atomic_load_winner_thread_id();
			if ( which_thread == 1 ) {
				T_1->unpin_value();  // pin the last value
				T_2->pin_value();  // pin the last value ... increment buckt count
			} else if ( which_thread == 0 ) {
				T_2->unpin_value();  // pin the last value
				T_1->pin_value();  // pin the last value ... increment buckt count
			}
		}
		//
		this->bucket_count_incr(h_bucket,which_thread);

		return which_thread;
	}


	private:
 

		// operate on the hash bucket bits
		uint32_t _next(uint32_t _H, uint32_t i) {
  			uint32_t H = _H & (~0 << i);
  			if ( H == 0 ) return UINT32_MAX;  // like -1
  			return FFS(H);	// return the count of trailing zeros
		}

		// operate on the hash bucket bits  -- find the next set bit
		uint32_t _succ(uint32_t h_bucket, uint32_t i) {
			uint32_t *bit_mask_buckets = _region_H;		// the binary pattern storage area
			uint32_t H = bit_mask_buckets[h_bucket];	// the one for the bucket
   			if ( GET(H, i) ) return i;			// look at the control bits of the test position... see if the position is set.
  			return _next(H, i);					// otherwise, what's next...
		}

		// operate on the hash bucket bits .. take into consideration that the bucket range is limited
		uint32_t _succ_hh_hash(HHash *T, uint32_t h_bucket, uint32_t i) {
			if ( i == 32 ) return(UINT32_MAX);  // all the bits in the bucket have been checked
			uint32_t N = T->_max_n;
			uint32_t h = (h_bucket % N);
			return _succ(h, i);
		}


		void del_hh_hash(HHash *T, uint32_t h, uint32_t i) {
			uint32_t *buffer = _region_H;
			uint64_t *buffer_v = _region_V;

			uint32_t N = T->_max_n;
			h = (h % N);
			uint32_t j = MOD(h + i, N);  // the offset relative to the original hash bucket + bucket position = absolute address
			//
			uint32_t V = buffer_v[j];
			uint32_t H = buffer[h];		// the control bit in the original hash bucket
			//
			if ( (V == 0) || !GET(H, i)) return;
			//
			// reset the hash machine
			buffer_v[j] = 0;	// clear the value slot
			UNSET(H,i);			// remove relative position from the hash bucket
			buffer[h] = H;		// store it
			// lower the count
			T->_count--;
		}

		// put_hh_hash
		// Given a hash and a value, find a place for storing this pair
		// (If the buffer is nearly full, this can take considerable time)
		// Attempt to keep things organized in buckets, indexed by the hash module the number of elements
		//
		bool put_hh_hash(HHash *T, uint32_t h, uint64_t v) {
			uint32_t N = T->_max_n;
			if ( (T->_count == N) || (v == 0) ) return(false);  // FULL
			//
			h = h % N;  // scale the hash .. make sure it indexes the array...
			uint32_t d = _probe(T, h);  // a distance starting from h (if wrapped, then past N)
			if ( d == UINT32_MAX ) return(false); // the positions in the entire buffer are full.
	//cout << "put_hh_hash: d> " << d;
			//
			uint32_t K =  T->_neighbor;
			while ( d >= K ) {						// the number may be bigger than K. if wrapping, then bigger than N. 2N < UINT32_MAX.
				uint32_t hd = MOD( (h + d), N );	// d is allowed to wrap around.
				uint32_t z = _hop_scotch(T, hd);	// hop scotch back to a moveable positions
	//cout << " put_hh_hash: z> " << z;
				if ( z == 0 ) return(false);			// could not find anything that could move. (Frozen at this point..)
				// found a position that can be moved... (offset from h <= d closer to the neighborhood)
				uint32_t j = z;
				z = MOD((N + hd - z), N);		// hd - z is an (offset from h) < h + d or (h + z) < (h + d)  ... see hopscotch 
				uint32_t i = _succ(z, 0);		// either this is moveable or there's another one. (checking the bitmap ...)
				_swap(T, z, i, j);				// swap bits and values between i and j offsets within the bucket h
				d = MOD( (N + z + i - h), N );  // N + z - (h - i) ... a new distance, should be less than before
			}
			//
			uint32_t *buffer = _region_H;
			uint64_t *buffer_v = _region_V;
			//
			uint32_t hd = MOD( (h + d), N );  // store the value
			buffer_v[hd] = v;
	//cout << " put_hh_hash: hd> " << hd  << " val: "  << v;

			//
			uint32_t H = buffer[h]; // update the hash machine
			SET(H,d);
			buffer[h] = H;
	//cout << " put_hh_hash: h> " << h  << " H: "  << H << endl;

			// up the count 
			T->_count++;
			return(true);
		}

		/**
		 * Swap bits and values 
		 * The bitmap is for the h'th bucket. And, i and j are bits within bitmap i'th and j'th.
		 * 
		 * i and j are used later as offsets from h when doing the value swap. 
		*/
		void _swap(HHash *T, uint32_t h, uint32_t i, uint32_t j) {
			uint32_t *buffer = _region_H;
			uint64_t *v_buffer = _region_V;
			//
			uint32_t H = buffer[h];
			UNSET(H, i);
			SET(H, j);
			buffer[h] = H;
			//
			uint32_t N = T->_max_n;
			i = MOD((h + i), N);		// offsets from the moveable position (i will often be 0)
			j = MOD((h + j), N);
			//
			uint64_t v = v_buffer[i];	// swap
			v_buffer[i] = 0;
			v_buffer[j] = v;
		}

		/**
		 *  _probe -- search for a free space within a bucket
		 * 		h : the bucket starts at h (an offset in _region_V)
		 * 
		 *  zero in the value buffer means no entry, because values do not start at zero for the offsets 
		 *  (free list header is at zero if not allocated list)
		 * 
		 * `_probe` wraps around search before h (bucket index) returns the larger value N + j if the wrap returns a position
		 * 
		 * @returns {uint32_t} distance of the bucket from h
		*/
		uint32_t _probe(HHash *T, uint32_t h) {   // value probe ... looking for zero
			uint64_t *v_buffer = _region_V;
			// // 
			uint32_t N = T->_max_n;		// upper bound (count of elements in buffer)
			//
			// search in the bucket
			v_buffer += h;
			for ( uint32_t i = h; i < N; ++i ) {			// search forward to the end of the array (all the way even it its millions.)
				uint64_t V = *v_buffer++;	// is this an empty slot? Usually, when the table is not very full.
				if ( V == 0 ) return (i-h);			// look no further
			}
			//
			// look for anything starting at the beginning of the segment
			// wrap... start searching from the start of all data...
			v_buffer = _region_V;
			for ( uint32_t j = 0; j < h ; ++j ) {
				uint64_t V = *v_buffer++;	// is this an empty slot? Usually, when the table is not very full.
				if ( V == 0 ) return (N + j - h);	// look no further (notice quasi modular addition)
			}
			return UINT32_MAX;  // this will be taken care of by a modulus in the caller
		}

		/**
		 * Look at one bit pattern after another from distance `d` shifted 'down' to h by K (as close as possible).
		 * Loosen the restriction on the distance of the new buffer until K (the max) away from h is reached.
		 * If something within K (for swapping) can be found return it, otherwise 0 (indicates frozen)
		*/
		uint32_t _hop_scotch(HHash *T, uint32_t hd) {  // return an index
			uint32_t *buffer = _region_H;
			uint32_t N = T->_max_n;
			uint32_t K =  T->_neighbor;
			for ( uint32_t i = (K - 1); i > 0; --i ) {
				uint32_t hi = MOD(N + hd - i, N);			// hop backwards towards the original hash position (h)...
				uint32_t H = buffer[hi];
				if ( (H != 0) && (((uint32_t)FFS(H)) < i) ) return i;	// count of trailing zeros less than offset from h
			}
			return 0;
		}



		/**
		 * Returns the value (for this use an offset into the data storage area.)
		*/
		uint64_t get_val_at_hh_hash(HHash *T, uint32_t h_bucket, uint32_t i) {
			uint32_t offset = (h_bucket + i);		// offset from the hash position...
			uint32_t N = T->_max_n;
			uint32_t j = (offset % N);		// if wrapping around
			return(_region_V[j]);			// return value
		}


		// ---- ---- ---- ---- ---- ---- ----
		// the actual hash key is stored in the lower part of the value pair.
		// the top part is an index into an array of objects.
		bool _cmp(uint64_t k, uint64_t x) {		// compares the bottom part of the words
			bool eq = ((HASH_MASK & k) == (HASH_MASK & x));
			return(eq); //
		}


		// SET OPERATION
		// originally called hunt for a set type...
		// In this applicatoin k is a value comparison... and the k value is an offset into an array of stored objects 
		// walk through the list of position occupied by bucket members. (Those are the ones with the positional bit set.)
		//
		uint64_t hunt_hash_set(HHash *T, uint32_t h_bucket, uint64_t key_null, bool kill) {
			uint32_t i = _succ_hh_hash(T, h_bucket, 0);   // i is the offset into the hash bucket.
			while ( i != UINT32_MAX ) {
				// x is from the value region..
				uint64_t x = get_val_at_hh_hash(T, h_bucket, i);  // get ith value matching this hash (collision)
				if ( _cmp(key_null, x) ) {		// compare the discerning hash part of the values (in the case of map, hash of the stored value)
					if (kill) del_hh_hash(T, h_bucket, i);
					return x;   // the value is a pair of 32 bit words. The top 32 bits word is the actual value.
				}
				i = _succ_hh_hash(T, h_bucket, (i + 1));  // increment i in some sense (skip unallocated holes)
			}
			return 0;		// no value  (values will always be positive, perhaps a hash or'ed onto a 0 value)
		}


		// The 64 bit key_null is a hash-value pair, with the hash (pattern match) in the bottom 32 bits.
		// the top 32 bits will contain a value if it is stored. Otherwise, it will be zero (no value stored).
		// In the main use of this code, the value will be an offset into an array of objects.

		uint64_t get_hh_set(HHash *T, uint32_t hbucket, uint32_t key) {  // T, full_hash, hash_bucket
			uint64_t zero = 0;
			uint64_t key_null = (zero | (uint64_t)key); // hopefully this explains it... top 32 are zero (hence no value represented)
//cout << "get_hh_set: key_null: " << key_null << " hash: " << hash <<  endl;
			bool flag_delete = false;
			return hunt_hash_set(T, hbucket, key_null, flag_delete);
		}


		bool put_hh_set(HHash *T, uint32_t h, uint64_t key_val) {
			if ( key_val == 0 ) return 0;		// cannot store zero values
			if ( get_hh_set(T, h, (uint32_t)key_val) != 0 ) return (true);  // found, do not duplicate ... _cmp has been called
			if ( put_hh_hash(T, h, key_val)) return (true); // success
			// not implementing resize
			return (false);
		}


		uint64_t del_hh_set(HHash *T, uint32_t hbucket, uint32_t key) { 
			uint64_t zero = 0;
			uint64_t key_null = (zero | (uint64_t)key); // hopefully this explains it... 
			bool flag_delete = true;
			return hunt_hash_set(T, hbucket, key_null, flag_delete); 
		}

		// note: not implementing resize since the size of the share segment is controlled by the application..

		// MAP OPERATION

		// loaded value -- value is on top (high word) and the index (top of loaded hash) is on the

		// T, hash_bucket, el_key, v_value   el_key == full_hash

		uint64_t put_hh_map(HHash *T, uint32_t hash_bucket, uint32_t full_hash, uint32_t value) {
			if ( value == 0 ) return false;
//cout <<  " put_hh_map: loaded_value [value] " << value << " loaded_value [index] " << index;
			uint64_t loaded_value = (((uint64_t)value) << HALF) | full_hash;
//cout << " loaded_value: " << loaded_value << endl;
			bool put_ok = put_hh_set(T, hash_bucket, loaded_value);
			if ( put_ok ) {
				uint64_t loaded_key = (((uint64_t)full_hash) << HALF) | hash_bucket; // LOADED
				return(loaded_key);
			} else {
				return(UINT64_MAX);
			}
		}

		uint32_t get_hh_map(HHash *T, uint64_t key) { 
			 // UNLOADED
			uint32_t element_diff = (uint32_t)((key >> HALF) & HASH_MASK);  // just unloads it (was index)
			uint32_t hash = (uint32_t)(key & HASH_MASK);
//cout << "get_hh_map>> element_diff: " << element_diff << " hash: " << hash << " ";
//cout << " _region_H[hash] " << _region_H[hash] << " _region_V[hash]  "  << _region_V[hash]  << endl;

			return (uint32_t)(get_hh_set(T, hash, element_diff) >> HALF); 
		}

		uint32_t del_hh_map(HHash *T, uint64_t key) {
			 // UNLOADED
			uint32_t element_diff = (uint32_t)((key >> HALF) & HASH_MASK);
			uint32_t hash = (uint32_t)(key & HASH_MASK);
			return (uint32_t)(del_hh_set(T, hash, element_diff) >> HALF);
		}

		// ---- ---- ---- ---- ---- ---- ----
		//
		bool							_status;
		bool							_initializer;
		uint32_t						_max_count;
		const char 						*_reason;
		uint8_t		 					*_region;
		//
		uint32_t		 				*_region_H_1;
		uint64_t		 				*_region_V_1;
		uint32_t		 				*_region_H_2;
		uint64_t		 				*_region_V_2;

		// threads ...

		ThreadWrapper					_threads[2];
		HHASH							*_h_tables[2];

};


namespace node {
namespace node_shm {

	/**
	 * Setup a segment as a container of a hop scotch hash table
	 * Params:
	 *  key_t key
	 */
	NAN_METHOD(get_HH);

}
}


#endif // _H_HOPSCOTCH_HASH_SHM_