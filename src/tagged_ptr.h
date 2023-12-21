#pragma once

#ifndef KCAS_TAGGED_H
#define KCAS_TAGGED_H
/*
FROM Implementation of fast K-CAS by
"Reuse, don't Recycle: Transforming Lock-Free Algorithms that Throw Away Descriptors."
(Brown and Arbel-Raviv)

Copyright (C) 2018  Robert Kelly

Changes by R. Leddy 2023


This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>


using namespace std;


namespace concurrent_data_structures {


//
static const size_t S_NUM_THREADS = 144;

// 2 bits
static const uint64_t S_NO_TAG = 0x0;
static const uint64_t S_KCAS_TAG = 0x1;
static const uint64_t S_RDCSS_TAG = 0x2;

// 8 Bits
static const uint64_t S_THREAD_ID_SHIFT = 2;
static const uint64_t S_THREAD_ID_MASK = (~(uint64_t(1) << 8));

// 54 bits
static const uint64_t S_SEQUENCE_SHIFT = 10;
static const uint64_t S_SEQUENCE_MASK = (~(uint64_t(1) << 54));




  // 1. TaggedPointer - data descriptor   PRIVATE
  //
  struct TaggedPointer {

    uint64_t _raw_bits;

    // MEMBERS

    explicit TaggedPointer() noexcept : _raw_bits(0) {}

    explicit TaggedPointer(const uint64_t raw_bits) : _raw_bits(raw_bits) {
      assert(is_bits(*this));
    }

    explicit TaggedPointer(const uint64_t tag_bits, const uint64_t thread_id, const uint64_t sequence_number)
        : _raw_bits(  tag_bits  | (thread_id << S_THREAD_ID_SHIFT) 
                                | (sequence_number << S_SEQUENCE_SHIFT)) {}




    // BITS ONLY

    static TaggedPointer make_bits(const uint64_t _raw_bits) {
      return make_bits(TaggedPointer{_raw_bits});
    }

    static TaggedPointer make_bits(const TaggedPointer tagptr) {
      return TaggedPointer{tagptr._raw_bits};
    }


    // THREAD ID AND SEQUENCE NUMBER

    static const uint64_t get_thread_id(const TaggedPointer tagptr) {
      return (tagptr._raw_bits >> S_THREAD_ID_SHIFT) & S_THREAD_ID_MASK;
    }

    static const uint64_t get_sequence_number(const TaggedPointer tagptr) {
      return (tagptr._raw_bits >> S_SEQUENCE_SHIFT) & S_SEQUENCE_MASK;
    }



    // TAGGED copy - generic tag bits

    static inline TaggedPointer make_tagged(const uint64_t tag_bits, const TaggedPointer tagptr) {
        uint64_t thread_id = get_thread_id(tagptr);
        uint64_t sequence_number = get_sequence_number(tagptr);
        uint64_t bits = (tag_bits  | (thread_id << S_THREAD_ID_SHIFT) | (sequence_number << S_SEQUENCE_SHIFT));
        return TaggedPointer{bits};
    }



    static TaggedPointer mask_bits(const TaggedPointer tagptr) {
      return make_tagged(S_NO_TAG, tagptr);
    }

    
    // Pointers to descriptors
    static TaggedPointer make_rdcss(const uint64_t thread_id, const uint64_t sequence_number) {
      return TaggedPointer{S_RDCSS_TAG, thread_id, sequence_number};
    }

    static TaggedPointer make_rdcss(const TaggedPointer tagptr) {
      return make_tagged(S_RDCSS_TAG, tagptr);
    }


    static TaggedPointer make_kcas(const uint64_t thread_id, const uint64_t sequence_number) {
      return TaggedPointer{S_KCAS_TAG, thread_id, sequence_number};
    }

    static TaggedPointer make_kcas(const TaggedPointer tagptr) {
      return make_tagged(S_KCAS_TAG, tagptr);
    }



    // PROPERTIES    is_rdcss, is_kcas, is_bits

    // type of bits indicate is_rdcss descriptor pointer

    static bool is_rdcss(const TaggedPointer tagptr) {
      return (tagptr._raw_bits & S_RDCSS_TAG) == S_RDCSS_TAG;
    }

    // type of bits indicate kcas descriptor pointer

    static bool is_kcas(const TaggedPointer tagptr) {
      return (tagptr._raw_bits & S_KCAS_TAG) == S_KCAS_TAG;
    }


    // type of bits is cleared -- not in use for descriptor purposes
    static bool is_bits(const TaggedPointer tagptr) {
      return !is_kcas(tagptr) && !is_rdcss(tagptr);
    }


  };




}




#endif