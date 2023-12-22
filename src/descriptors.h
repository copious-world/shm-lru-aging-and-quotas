#pragma once

#ifndef KCAS_DESCR_H
#define KCAS_DESCR_H

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

#include "tagged_ptr.h"


namespace concurrent_data_structures {



static const size_t KCASShift = 2;


// Control regions contain reference to value locations in the value region
//
struct DescriptorEntry {
  atomic<TaggedPointer>   *_location;    // current value stored in _location
  TaggedPointer           _before;       // expected value
  TaggedPointer           _desired;      // new value 

  inline void location_update(TaggedPointer &expected, TaggedPointer new_value) {
    _location->compare_exchange_strong(expected, new_value, std::memory_order_relaxed,std::memory_order_relaxed);
  }
};          


typedef enum {
  UNDECIDED,
  SUCCEEDED,
  FAILED
} KCASDescStat;


inline bool undecided(uint64_t stat_var) { return stat_var == UNDECIDED; }
inline bool succeeded(uint64_t stat_var) { return stat_var == SUCCEEDED; }
inline bool failed(uint64_t stat_var) { return stat_var == FAILED; }


// 2. KCASDescriptorStatus - k-CAS descriptor ...   PRIVATE
// 
struct KCASDescriptorStatus {
  //
  uint64_t _status : 8;
  uint64_t _sequence_number : 54;
  //
  explicit KCASDescriptorStatus() noexcept : _status(uint64_t(UNDECIDED)), _sequence_number(0) {}

  explicit KCASDescriptorStatus(const uint64_t status, const uint64_t sequence_number) 
                                                                : _status(status), _sequence_number(sequence_number) {}
  explicit KCASDescriptorStatus(const KCASDescStat status, const uint64_t sequence_number) 
                                                                : _status(uint64_t(status)), _sequence_number(sequence_number) {}
  //
  inline bool still_undecided() { return _status == UNDECIDED; }
  inline bool succeeded() { return _status == SUCCEEDED; }
  inline bool failed() { return _status == FAILED; }

  inline bool seq_same(const uint64_t sequence_number) { return sequence_number == _sequence_number; }
  inline bool seq_same(const KCASDescriptorStatus &kcdesc) { return kcdesc._sequence_number == _sequence_number; }

  inline bool seq_different(const uint64_t sequence_number) { return sequence_number != _sequence_number; }


};



// ---
// KCASEntry
//
// Class to wrap around the types being KCAS'd
template <class Type>
class KCASEntry {
  //
  private:
    //
    union ItemBitsUnion {
      ItemBitsUnion() : _raw_bits(0) {}
      Type              _item;
      uint64_t          _raw_bits;
    };
    //

    atomic<TaggedPointer> _entry;

  public:

    // METHODS
    static Type from_raw_bits(const uint64_t raw_bits) {
      ItemBitsUnion ibu;
      ibu._raw_bits = raw_bits;
      return ibu._item;
    }

    static Type value_from_raw_bits(const uint64_t raw_bits) {
      ItemBitsUnion ibu;
      ibu._raw_bits = (raw_bits >> KCASShift);
      return ibu._item;
    }

    static uint64_t to_raw_bits(const Type &inner) {
      ItemBitsUnion ibu;
      ibu._item = inner;
      return ibu._raw_bits;
    }

    static uint64_t to_descriptor_bits(const Type &inner) {
      ItemBitsUnion ibu;
      ibu._item = inner;
      return (ibu._raw_bits << KCASShift);
    }

  public:

    atomic<TaggedPointer> *entry_ref() {
      return &_entry;
    }

    inline TaggedPointer atomic_load(std::memory_order fail) {
      return _entry.load(fail)
    }

    inline void atomic_store(TaggedPointer desc, const std::memory_order memory_order = std::memory_order_seq_cst) {
      _entry.store(desc, const std::memory_order memory_order = std::memory_order_seq_cst);
    }

    bool compare_exchange_weak(TaggedPointer &expected, const TaggedPointer &desired,
                                                            std::memory_order success, std::memory_order fail) {
      return _entry.compare_exchange_weak(expected, desired, success, fail);
    }

    bool compare_exchange_strong(TaggedPointer &expected, const TaggedPointer &desired,
                                                            std::memory_order success, std::memory_order fail) {
      return _entry.compare_exchange_strong(expected, desired, success, fail);
    }


};




// DESCRIPTOR :: KCASDescriptor
//
// contains size ... 

template<typename N>
class KCASDescriptor {

  public:

    KCASDescriptor() : _num_entries(0), _status(KCASDescriptorStatus{}) {}
    KCASDescriptor(const KCASDescriptor &rhs) = delete;
    KCASDescriptor &operator=(const KCASDescriptor &rhs) = delete;
    KCASDescriptor &operator=(KCASDescriptor &&rhs) = delete;

    // KCASDescriptor method -- increment_sequence
    //
    void increment_sequence() {
      KCASDescriptorStatus current_status = load_status(std::memory_order_relaxed);
      _status.store(KCASDescriptorStatus{KCASDescStat::UNDECIDED, current_status._sequence_number + 1},
                              std::memory_order_release);
    }

  public:

    inline KCASDescriptorStatus load_status(std::memory_order prefered = std::memory_order_acquire) {
      return _status.load(prefered);
    }

    inline void status_compare_exchange(KCASDescriptorStatus &expected_status,uint64_t status, uint64_t sequence_number) {
      _status.compare_exchange_strong(expected_status,KCASDescriptorStatus{status,sequence_number},
                  std::memory_order_relaxed, std::memory_order_relaxed);
    }

    inline void sort_kcas_entries() {
        sort(&_entries[0], &_entries[_num_entries],   // begin,end (of entries)
                    [](const DescriptorEntry &lhs, const DescriptorEntry &rhs) -> bool {
                      return lhs._location < rhs._location;
                    });
    }

    template <class ValType>
    void add_value(KCASEntry<ValType> *location, const ValType &before, const ValType &desired) {
      //
      TaggedPointers before_desc = raw_tps_shift_ptr(before);         assert(TaggedPointer::is_bits(before_desc));
      TaggedPointers desired_desc = raw_tps_shift_ptr(desired);       assert(TaggedPointer::is_bits(desired_desc));
      //
      const size_t cur_entry = _num_entries++;
      //
      _entries[cur_entry]._before = before_desc;
      _entries[cur_entry]._desired = desired_desc;
      _entries[cur_entry]._location = location->entry_ref();
      //
    }

    template <class PtrType>
    void add_ptr(const KCASEntry<PtrType> *location, const PtrType &before, const PtrType &desired) {
      //        
      TaggedPointers before_desc = raw_tps_ptr(before);       assert(TaggedPointer::is_bits(before_desc));
      TaggedPointers desired_desc = raw_tps_ptr(desired);     assert(TaggedPointer::is_bits(desired_desc));
      //
      const size_t cur_entry = _num_entries++;
      //
      _entries[cur_entry]._before = before_desc;
      _entries[cur_entry]._desired = desired_desc;
      _entries[cur_entry]._location = location->entry_ref();
      //
    }

    void update_entries(TaggedPointer tagptr,bool succeeded) {
      size_t num_entries = _num_entries;
      for ( size_t i = 0; i < num_entries; i++ ) {
        TaggedPointer new_value = succeeded ? this->_entries[i]._desired
                                            : this->_entries[i]._before;
        {
          TaggedPointer expected = tagptr;
          this->_entries[i].location_update(expected, new_value);
        }
      }
    }


    bool copy_entries_from(KCASDescriptor *snapshot_target,uint64_t sequence_number) {
        //
        const size_t num_entries = snapshot_target->_num_entries;
        //
        for ( size_t i = 0; i < num_entries; i++ ) {
            this->_entries[i] = snapshot_target->_entries[i];
        }
        //
        const KCASDescriptorStatus after_status = snapshot_target->load_status();
        //
        if ( after_status.seq_different(sequence_number) ) {
            return false;
        } else {
            _num_entries = num_entries;
            return true;
        }
    }



    //
    // Brown_KCAS::try_snapshot -- PRIVATE called only by methods in this class... 
    //
    bool try_snapshot(KCASDescriptor *snapshot_target, TaggedPointer ptr) {
      //
      const uint64_t sequence_number = TaggedPointer::get_sequence_number(ptr);
      //
      const KCASDescriptorStatus before_status = snapshot_target->load_status();
      //
      if ( before_status.seq_different(sequence_number) ) {
        return false;
      } else {
        return this->copy_entries_from(snapshot_target,sequence_number);
      }
    }


  private:
  
    size_t                        _num_entries;              // incremented in ADD methods
    atomic<KCASDescriptorStatus>  _status;
    DescriptorEntry               _entries[N];  // N - template parameter ---- 

  public:

    friend class Brown_KCAS;
    template <class T> friend class CachePadded;
};





// 3. RDCSSDescriptor -- double compare single swap... descriptor    PRIVATE
// atomics e.g. state information and shared value location which the compiler will use CAS type ops to manipulate
struct RDCSSDescriptor {
    //
    atomic_size_t                 _sequence_bits;           // seq or counter... atomic
    atomic<TaggedPointer>         *_data_location;
    atomic<KCASDescriptorStatus>  *_status_location;
    //
    TaggedPointer         _before;
    TaggedPointer         _kcas_tagptr;

    //
    inline size_t increment_sequence() {   // inline understood 
        return _sequence_bits.fetch_add(1, std::memory_order_acq_rel);
    }

    inline size_t atomic_load_sequence() {
        return _sequence_bits.load();
    }

    inline TaggedPointer atomic_load_data() {
        return _data_location->load(std::memory_order_relaxed);
    }

    inline KCASDescriptorStatus atom_load_kcas_descr() {
        return _status_location->load(std::memory_order_relaxed);
    }

    //
    template<typename N>
    void copy_from_kcas(KCASDescriptor<N> *descriptor_snapshot,KCASDescriptor<N> *original_desc, const TaggedPointer tagptr,size_t i) {
        this->increment_sequence();
        this->_data_location = descriptor_snapshot->_entries[i]._location;  // copy pointer ... reference
        this->_before = descriptor_snapshot->_entries[i]._before;
        this->_kcas_tagptr = tagptr;
        this->_status_location = &original_desc->_status;
    }

    //
    void copy_from_rdcss_descriptor(RDCSSDescriptor *from,const size_t before_sequence) {
        this->_sequence_bits.store(before_sequence, std::memory_order_relaxed);
        this->_data_location = from->_data_location;
        this->_before = from->_before;
        this->_kcas_tagptr = from->_kcas_tagptr;
        this->_status_location = from->_status_location;
    }


    inline bool location_update(TaggedPointer &expected, TaggedPointer new_value) {
        return _data_location->compare_exchange_strong(expected, new_value, std::memory_order_relaxed,std::memory_order_relaxed);
    }


    bool try_snapshot(RDCSSDescriptor *snapshot_target, TaggedPointer ptr) {
        //
        const uint64_t sequence_number = TaggedPointer::get_sequence_number(ptr);   // sequence_number - what we think it is

        // load the bits in the sequece...
        const size_t before_sequence = snapshot_target->atomic_load_sequence(); // what it seems to be in actuality...
        //
        if ( before_sequence != sequence_number ) {   // someone else changed this while we started inspecting it
            return false;       // no control over the sequence ... so leave
        }

        // Snapshot - the snapshot passed (whose is it?) that thread now gets the bits stored by the other thread
        // so ... copy all fields
        this->copy_from_rdcss_descriptor(snapshot_target,before_sequence);
        // should now have a grasp of reality

        // Check our sequence number again.
        const size_t after_sequence = snapshot_target->atomic_load_sequence();
        if ( after_sequence != sequence_number ) {
            return false;
        }

        return true;
    }


};



}



#endif