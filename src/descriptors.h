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
//  DescriptorEntry -- location will be a reference to the _entry of a KCASEntry, which is a tagged pointer.
//  The KCASEntry is provided by the application wishing to perform cooperative operations.
//  
//
struct DescriptorEntry {
  atomic<TaggedPointer>   *_location_atm;    // current value stored in _location_atm
  TaggedPointer           _before;       // expected value
  TaggedPointer           _desired;      // new value 

  inline void descr_location_update(TaggedPointer &expected, TaggedPointer new_value) {
    _location_atm->compare_exchange_strong(expected, new_value, std::memory_order_relaxed,std::memory_order_relaxed);
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
  uint64_t _sequence_number : 54;   // total 64 bits - 2
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


// sizeof(KCASDescriptorStatus) should be 8 bytes



// ---
// KCASEntry  -- defined for  KCASDescriptor
//  -- the application passes this in for management in the CAS table...
//
//
// Class to wrap around the types being KCAS'd
template <class Type>
struct KCASEntry {
  //
  union ItemBitsUnion {
    ItemBitsUnion() : _raw_bits(0) {}   /// most likely the Type and the _raw_bits will be the same.
    Type              _item;
    uint64_t          _raw_bits;
  };
  //

  atomic<TaggedPointer> _entry;

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


  static Type value_from_raw_bits(TaggedPointer &tp_desc) {
    return value_from_raw_bits(tp_desc._raw_bits)
  }

  static TaggedPointer raw_tp_ptr(Type value) {
    uint64_t some_raw_bits = KCASEntry<Type>::to_raw_bits(value);
    TaggedPointer a_desc{some_raw_bits};
    return a_desc;
  }

  // put the value in a value in the region above the bottom two bits (which may hold status)
  // return the tagged pointer associated with it.
  static TaggedPointer raw_tp_shift_ptr(Type value) {
    uint64_t descr_bits = KCASEntry<Type>::to_descriptor_bits(value);  // shift
    TaggedPointer a_desc{descr_bits};
    return a_desc;
  }


  // Atomic ref


  atomic<TaggedPointer> *entry_ref() {
    return &_entry;
  }


  // Atomic operations ...

  inline TaggedPointer atomic_load(std::memory_order fail = std::memory_order_seq_cst) {
    return _entry.load(fail)
  }

  inline void atomic_store(TaggedPointer desc, const std::memory_order memory_order = std::memory_order_seq_cst) {
    _entry.store(desc, const std::memory_order memory_order = std::memory_order_seq_cst);
  }

  inline bool compare_exchange_weak(TaggedPointer &expected, const TaggedPointer &desired,
                                                          std::memory_order success, std::memory_order fail) {
    return _entry.compare_exchange_weak(expected, desired, success, fail);
  }

  inline bool compare_exchange_strong(TaggedPointer &expected, const TaggedPointer &desired,
                                                          std::memory_order success, std::memory_order fail) {
    return _entry.compare_exchange_strong(expected, desired, success, fail);
  }

};




// DESCRIPTOR :: KCASDescriptor
//
// contains size ... 

template<typename N>
struct KCASDescMembers {
    size_t                        _num_entries;              // incremented in ADD methods
    atomic<KCASDescriptorStatus>  _status;
    DescriptorEntry               _entries[N];  // N - template parameter ---- 
};

template<typename N>
#define KCASMEMSIZE (sizeof(concurrent_data_structures::KCASDescMembers<N>))
constexpr bool DO_ADD_KCAS_MEM_SIZE() { return (KCASMEMSIZE%(sizeof(uint64_t)) != 0); }


template<typename N>
class KCASDescriptor : public KCASDescMembers<N> {

  public:

    KCASDescriptor() : _num_entries(0), _status(KCASDescriptorStatus{}) {}
    // can only call the basic constructor....
    KCASDescriptor(const KCASDescriptor &rhs) = delete;
    KCASDescriptor &operator=(const KCASDescriptor &rhs) = delete;
    KCASDescriptor &operator=(KCASDescriptor &&rhs) = delete;

  public:

    inline KCASDescriptorStatus load_status(std::memory_order prefered = std::memory_order_acquire) {
      return _status.load(prefered);
    }

    // KCASDescriptor method -- increment_sequence
    //
    void increment_sequence() {
      // make use of the existing status value (load it) for the sake of its sequence number
      KCASDescriptorStatus current_status = load_status(std::memory_order_relaxed);
      // now reset the status (UNDECIDED) and >> add to the seq number... making this different than it was before 
      _status.store(KCASDescriptorStatus{KCASDescStat::UNDECIDED, current_status._sequence_number + 1},  // values bless the addressed memory with KCASDescriptorStatus
                              std::memory_order_release);
    }

    // status_compare_exchange
    // if you know what the existing status is, you can set your own status {UNDECIDED,SUCEEDED, FAILED} and sequence_number
    inline void status_compare_exchange(KCASDescriptorStatus &expected_status,uint64_t status, uint64_t sequence_number) {
      _status.compare_exchange_strong(expected_status,KCASDescriptorStatus{status,sequence_number},
                  std::memory_order_relaxed, std::memory_order_relaxed);
    }

    inline void sort_kcas_entries() {
        sort(&_entries[0], &_entries[_num_entries],   // begin,end (of entries)
                    [](const DescriptorEntry &lhs, const DescriptorEntry &rhs) -> bool {
                      return lhs._location_atm < rhs._location_atm;
                    });
    }



    // Adds an entry to the descriptor for both the pointer and value methods. 
    // 
    template <class T>
    void _adder(KCASEntry<T> *location,TaggedPointers before_desc,TaggedPointers desired_desc) {
      const size_t cur_entry = _num_entries++;
      _entries[cur_entry]._before = before_desc;
      _entries[cur_entry]._desired = desired_desc;
      _entries[cur_entry]._location_atm = location->entry_ref();  // from the address of the KCASEntry to its member _entry
    }


    // Add a value into the descriptor's entries. 
    // Called by the application.
    // saves up the value. There is no contention for a spot a this point. The descriptor belongs to a single thread/proc.
    // places values before, desired (which there is hope that the value will reside in the tagged pointer at the location's _entry cell)
    // and location into an entry
    //
    template <class ValType>
    void add_value(KCASEntry<ValType> *location, const ValType &before, const ValType &desired) {
      TaggedPointers before_desc = raw_tp_shift_ptr(before);         assert(TaggedPointer::is_bits(before_desc));
      TaggedPointers desired_desc = raw_tp_shift_ptr(desired);       assert(TaggedPointer::is_bits(desired_desc));
      _adder(location,before_desc,desired_desc);
    }

    template <class PtrType>
    void add_ptr(const KCASEntry<PtrType> *location, const PtrType &before, const PtrType &desired) {
      TaggedPointers before_desc = raw_tp_ptr(before);       assert(TaggedPointer::is_bits(before_desc));
      TaggedPointers desired_desc = raw_tp_ptr(desired);     assert(TaggedPointer::is_bits(desired_desc));
      _adder(location,before_desc,desired_desc);
    }

    

    void update_entries(TaggedPointer tagptr,bool succeeded) {
      size_t num_entries = _num_entries;
      for ( size_t i = 0; i < num_entries; i++ ) {
        TaggedPointer new_value = succeeded ? this->_entries[i]._desired : this->_entries[i]._before;
        {
          TaggedPointer expected = tagptr;
          this->_entries[i].descr_location_update(expected, new_value);
        }
      }
    }


    bool _copy_entries_from(KCASDescriptor *snapshot_target,uint64_t sequence_number) {
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
        return this->_copy_entries_from(snapshot_target,sequence_number);
      }
    }


  private:

    if constexpr(DO_ADD_KCAS_MEM_SIZE()) {
#define KCASMEMSIZE (sizeof(concurrent_data_structures::KCASDescMembers<N>))
      constexpr bool ADDED_KCAS_MEM_SIZE() { return KCASMEMSIZE%(sizeof(uint64_t)); }
      uint8_t extra_bytes[ADDED_KCAS_MEM_SIZE()];
    }


};




struct RDCSSDescMembers {
    atomic_size_t                 _sequence_bits;           // seq or counter... atomic
    atomic<TaggedPointer>         *_data_location;
    atomic<KCASDescriptorStatus>  *_status_location;
    //
    TaggedPointer                 _before;
    TaggedPointer                 _kcas_tagptr;   // should be 40 bytes (otherwise 32 bytes)
};


#define RDCSSMEMSIZE (sizeof(concurrent_data_structures::RDCSSDescMembers))
constexpr bool DO_ADD_RDCSS_MEM_SIZE() { return (RDCSSMEMSIZE%(sizeof(uint64_t)) != 0); }



// 3. RDCSSDescriptor -- double compare single swap... descriptor    PRIVATE
// atomics e.g. state information and shared value location which the compiler will use CAS type ops to manipulate
struct RDCSSDescriptor : public RDCSSDescMembers {

    // -----

    inline size_t increment_sequence() {   // inline understood 
        return _sequence_bits.fetch_add(1, std::memory_order_acq_rel);
    }

    inline size_t atomic_load_sequence() {
        return _sequence_bits.load();
    }

    inline TaggedPointer atomic_load_data() {
        return _data_location->load(std::memory_order_relaxed);
    }

    inline KCASDescriptorStatus atom_load_refd_status() {
        return _status_location->load(std::memory_order_relaxed);
    }

    // -----
    //
    /**
     * copy_from_kcas_entry
    */
    template<typename N>
    void copy_from_kcas_entry(KCASDescriptor<N> *from,size_t i,KCASDescriptor<N> *original_desc, const TaggedPointer tagptr) {
        this->increment_sequence();
        this->_data_location = from->_entries[i]._location_atm;  // copy pointer ... reference
        this->_before = from->_entries[i]._before;
        this->_kcas_tagptr = tagptr;
        this->_status_location = &(original_desc->_status);
    }

    //
    /**
     * copy_from_rdcss_descriptor
    */
    void copy_from_rdcss_descriptor(RDCSSDescriptor *from,const size_t before_sequence) {
        this->_sequence_bits.store(before_sequence, std::memory_order_relaxed);  // atomic store
        this->_data_location = from->_data_location;
        this->_before = from->_before;
        this->_kcas_tagptr = from->_kcas_tagptr;
        this->_status_location = from->_status_location;
    }


    /**
     * location_update
    */
    inline bool location_update(TaggedPointer &expected, TaggedPointer new_value) {
        return _data_location->compare_exchange_strong(expected, new_value, std::memory_order_relaxed,std::memory_order_relaxed);
    }

    /**
     * try_snapshot
    */
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
        // (this) has been allocated on the stack and basically contains nothing .. this line initializes it
        this->copy_from_rdcss_descriptor(snapshot_target,before_sequence);   // put the existing values into this descriptor
        // should now have a grasp of reality

        // Check our sequence number again.
        const size_t after_sequence = snapshot_target->atomic_load_sequence();  // check if control is lost
        if ( after_sequence != sequence_number ) {
            return false;
        }

        return true;  // we have some agreement on the sequence number...
    }

private:

    if constexpr(DO_ADD_RDCSS_MEM_SIZE())  {
#define RDCSSMEMSIZE (sizeof(concurrent_data_structures::RDCSSDescMembers))
      constexpr bool ADDED_RDCSS_MEM_SIZE() { return RDCSSMEMSIZE%(sizeof(uint64_t)); }
      uint8_t extra_bytes[ADDED_RDCSS_MEM_SIZE()];
    }

};


}



#endif