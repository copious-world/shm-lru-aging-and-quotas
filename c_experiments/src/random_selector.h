#ifndef _H_RANDOM_SELECTOR_
#define _H_RANDOM_SELECTOR_


#pragma once

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>


#include <random>
#include <bitset>


#include <list>

using namespace std;



#define  BITS_GEN_COUNT_LAPSE (3)

#define RNDMS_CONTROL_WD_CNT (4)

typedef enum _rtcli_ {
	rtcli_SHARED_INDEX,
	rtcli_SHARED_BIT_COUNT,
	rtcli_SHARED_LAST_BIT,
	rtcli_SHARED_WORDS_USED
} randoms_ctl_indecies;


template<const uint32_t NbitWords = 256,const uint8_t SELECTOR_MAX = 4>
class Random_bits_generator {

    bernoulli_distribution	_distribution;
	//
	mt19937 				_engine;
	uint32_t				_last_bits;
	uint8_t					_bcount;
	uint8_t					_gen_count;

	// ---- ---- ---- ---- ---- ---- ---- ----

	uint32_t				*_shared_bits;
	uint8_t					_shared_bcount;

	uint32_t				*_shared_bits_regions[SELECTOR_MAX];
	uint8_t					_current_shared_region;

	// ---- ---- ---- ---- ---- ---- ---- ----


public:

	static constexpr uint32_t check_expected_region_size = SELECTOR_MAX*(NbitWords + RNDMS_CONTROL_WD_CNT)*sizeof(uint32_t);
	static constexpr uint8_t _max_r_buffers{SELECTOR_MAX};

	list<uint32_t>			_bits;

public:

	Random_bits_generator() : _bcount(0), _gen_count(0), _current_shared_region(0) {

		_shared_bits = nullptr;

		typedef std::chrono::high_resolution_clock hrc;
		hrc::time_point beginning = hrc::now();

		random_device r;

		hrc::duration d = hrc::now() - beginning;
  		unsigned timer_seed = d.count();
		//
		_engine.seed(timer_seed);
		//
		_shared_bits_regions[0] = nullptr;
		_shared_bits_regions[1] = nullptr;
		_shared_bits_regions[2] = nullptr;
		_shared_bits_regions[3] = nullptr;
		//
		_create_random_bits();
	}

public:


	/**
	 * set_region
	 * 
	*/

	void set_region(uint32_t *region,uint8_t index) {
		if ( (index < SELECTOR_MAX) && (region != nullptr) ) {
			_shared_bits_regions[index] = region;
		}
	}


	uint32_t generate_word() {
		uint32_t bits = 0;
		for ( uint8_t i = 0; i < 32; i++ ) {
			bool bit = _distribution(_engine);
			bits = (bits << 1) | (bit ? 1 : 0);
		}
		return bits;
	}


    template <typename OutputIt>
    void generate(OutputIt first, OutputIt last) {
        while ( first != last ) {
			uint32_t bits = generate_word();
            *first++ = bits;
        }
    }


	void _create_random_bits() {
		_bits.resize(NbitWords);   // fr list ??
		generate(_bits.begin(), _bits.end());
	}


	/**
	 * pop_bit
	 * 
	*/

	uint8_t pop_bit() {
		//
		if ( _bcount == 0 ) {
			_last_bits = _bits.front();
			_bits.pop_front();
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


	/**
	 * transfer_to_shared_buffer
	 * 		copy from the STL list container to the shared bits area.
	*/

	void transfer_to_shared_buffer(uint32_t *shared_bit_area, uint32_t size, uint8_t selector = UINT8_MAX) {
		//
		if ( selector < SELECTOR_MAX ) {
			set_region(shared_bit_area,selector);
			_current_shared_region = selector;
		}
		//
		shared_bit_area[rtcli_SHARED_INDEX] = 0; // shared index
		shared_bit_area[rtcli_SHARED_BIT_COUNT] = 0; // shared bit count
		shared_bit_area[rtcli_SHARED_LAST_BIT] = 0; // shared last bit (being shifted 32-1 times)
		//
		_shared_bits = shared_bit_area;
		uint32_t *area_w = (shared_bit_area + RNDMS_CONTROL_WD_CNT);
		uint32_t *end = area_w + size;
		for ( auto bword : _bits ) {
			*area_w++ = bword;
			if ( area_w >= end ) break;
		}
	}


	void wakeup_random_generator([[maybe_unused]] uint8_t which_region) {
		// regenerate_shared(which_region);
	}


	/**
	 * pop_shared_bit
	 * 
	*/

	uint8_t pop_shared_bit() {
		//
		if ( _shared_bits == nullptr ) return 0;  // get nothing.... not random
		//
		share_lock();
		uint32_t shared_bcount = _shared_bits[rtcli_SHARED_BIT_COUNT];
		if ( _shared_bcount == 0 ) {
			uint32_t index = _shared_bits[rtcli_SHARED_INDEX];
			_shared_bits[rtcli_SHARED_LAST_BIT] = _shared_bits[index + RNDMS_CONTROL_WD_CNT];
			//
			if ( index < _bits.size() ) {
				_shared_bits[rtcli_SHARED_INDEX] = ++index;
			} else {
				_shared_bits[rtcli_SHARED_INDEX] = 0; // wraps (client might need to keep track...)
				swap_prepped_bit_regions(true);
			}
		}
		//
		uint32_t shared_last_bits = _shared_bits[rtcli_SHARED_LAST_BIT];
		uint8_t the_bit = (shared_last_bits & 0x1);
		//
		shared_last_bits = (shared_last_bits >> 1);
		shared_bcount = shared_bcount++ % 32;
		//
		_shared_bits[rtcli_SHARED_BIT_COUNT] = shared_bcount;
		_shared_bits[rtcli_SHARED_LAST_BIT] = shared_last_bits;
		share_unlock();
		//
		return the_bit;
	}


	/**
	 * swap_prepped_bit_regions
	 * 
	*/

	void swap_prepped_bit_regions(bool gen_wakes_up = false) {
		if ( gen_wakes_up ) {
			wakeup_random_generator(_current_shared_region);
		}
		_current_shared_region = (_current_shared_region + 1) % SELECTOR_MAX;
		_shared_bits = _shared_bits_regions[_current_shared_region];
	}

	/**
	 * regenerate_shared
	*/
	void regenerate_shared(uint8_t region_selector) {
		//
		if ( (region_selector < SELECTOR_MAX) && (_shared_bits_regions[region_selector] != nullptr) ) {
			_create_random_bits();
			share_lock();
			transfer_to_shared_buffer(_shared_bits_regions[region_selector],_bits.size());
			share_unlock();
		}
		//
	}



	virtual void share_lock(void) = 0;
	virtual void share_unlock(void) = 0;

};




#endif // _H_RANDOM_SELECTOR_





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
