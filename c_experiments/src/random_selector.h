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
