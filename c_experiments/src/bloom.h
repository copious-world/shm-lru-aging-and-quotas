#pragma once
/*
Lock-Free Bloom Filter.
Copyright (C) 2024 Richard Leddy

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/



// https://callidon.github.io/bloom-filters/


#include <bitset>
#include <iostream>
#include <string.h>
#include <atomic>

using namespace std;

class AtomicBloom {

  public:

    AtomicBloom(uint8_t *bloom_bytes,uint16_t bloom_bits_count) {
      _bloom_bytes = bloom_bytes;
      _bloom_bits_count = bloom_bits_count;
      _bloom_bytes_count = bloom_bits_count/sizeof(uint8_t);
      _end_bloom_bytes = bloom_bytes + _bloom_bytes_count;
      _fill_count = 0;
    }

    virtual ~AtomicBloom(void) {
    }

  public:


    bool set_bloom_bits(uint16_t h1,uint16_t h2, uint16_t h3, uint16_t h4, uint16_t h5) {
      //
      if ( _bloom_bytes == nullptr ) return false;
      //
      uint16_t byte_index1 = (h1 >> 3);
      uint16_t byte_index2 = (h2 >> 3);
      uint16_t byte_index3 = (h3 >> 3);
      uint16_t byte_index4 = (h4 >> 3);
      uint16_t byte_index5 = (h5 >> 3);
      //
      if ( byte_index1 >= _bloom_bytes_count ) return false;
      if ( byte_index2 >= _bloom_bytes_count ) return false;
      if ( byte_index3 >= _bloom_bytes_count ) return false;
      if ( byte_index4 >= _bloom_bytes_count ) return false;
      if ( byte_index5 >= _bloom_bytes_count ) return false;

      uint16_t bit_offset1 = (h1 & 0x3);
      uint16_t bit_offset2 = (h2 & 0x3);
      uint16_t bit_offset3 = (h3 & 0x3);
      uint16_t bit_offset4 = (h4 & 0x3);
      uint16_t bit_offset5 = (h5 & 0x3);
      //
      bool adding = false;
      //
      uint8_t up_bit = (0x1 << bit_offset1);
      auto updater = _bloom_bytes[byte_index1].fetch_or(up_bit,std::memory_order_acquire);
      auto prev = updater;
      updater |= up_bit;
      if ( updater != prev ) adding = true;
      //
      up_bit = (0x1 << bit_offset2);
      updater = _bloom_bytes[byte_index2].fetch_or(up_bit,std::memory_order_acquire);
      prev = updater;
      updater |= up_bit;
      if ( updater != prev ) adding = true;      
      //
      up_bit = (0x1 << bit_offset3);
      updater = _bloom_bytes[byte_index3].fetch_or(up_bit,std::memory_order_acquire);
      prev = updater;
      updater |= up_bit;
      if ( updater != prev ) adding = true;
      //
      updater = _bloom_bytes[byte_index4];
      prev = updater;
      updater |= (0x1 << bit_offset4);
      if ( updater != prev ) adding = true;
      _bloom_bytes[byte_index4] = updater;
      //
      up_bit = (0x1 << bit_offset5);
      updater = _bloom_bytes[byte_index5].fetch_or(up_bit,std::memory_order_acquire);
      prev = updater;
      updater |= up_bit;
      if ( updater != prev ) adding = true;
      //
      if ( adding ) {
        _fill_count++;
      }
      //
      return true;
    }


    bool check_bloom_bits(uint16_t h1,uint16_t h2, uint16_t h3, uint16_t h4, uint16_t h5) {
      //
      if ( _bloom_bytes == nullptr ) return false;
      //
      uint16_t byte_index1 = (h1 >> 3);
      uint16_t byte_index2 = (h2 >> 3);
      uint16_t byte_index3 = (h3 >> 3);
      uint16_t byte_index4 = (h4 >> 3);
      uint16_t byte_index5 = (h5 >> 3);
      //
      if ( byte_index1 >= _bloom_bytes_count ) return false;
      if ( byte_index2 >= _bloom_bytes_count ) return false;
      if ( byte_index3 >= _bloom_bytes_count ) return false;
      if ( byte_index4 >= _bloom_bytes_count ) return false;
      if ( byte_index5 >= _bloom_bytes_count ) return false;

      uint16_t bit_offset1 = (h1 & 0x3);
      uint16_t bit_offset2 = (h2 & 0x3);
      uint16_t bit_offset3 = (h3 & 0x3);
      uint16_t bit_offset4 = (h4 & 0x3);
      uint16_t bit_offset5 = (h5 & 0x3);
      //
      bool included = true;
      included &= (_bloom_bytes[byte_index1].load(std::memory_order_acquire) & (0x1 << bit_offset1));
      included &= (_bloom_bytes[byte_index2].load(std::memory_order_acquire) & (0x1 << bit_offset2));
      included &= (_bloom_bytes[byte_index3].load(std::memory_order_acquire) & (0x1 << bit_offset3));
      included &= (_bloom_bytes[byte_index4].load(std::memory_order_acquire) & (0x1 << bit_offset4));
      included &= (_bloom_bytes[byte_index5].load(std::memory_order_acquire) & (0x1 << bit_offset5));
      //
      return included;
    }


    bool full_enough(double threshold) {
      double current_full = ((double)_fill_count/(double)_bloom_bits_count);
      if (current_full > threshold) return true;
      return false;
    }


  protected:

    uint16_t          _fill_count{0};
    uint16_t          _bloom_bytes_count{0};
    uint16_t          _bloom_bits_count{0};
    atomic<uint8_t>   *_bloom_bytes{nullptr};
    atomic<uint8_t>   *_end_bloom_bytes{nullptr};

};

