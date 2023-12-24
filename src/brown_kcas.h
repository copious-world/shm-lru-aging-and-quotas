#pragma once

/*
Implementation of fast K-CAS by
"Reuse, don't Recycle: Transforming Lock-Free Algorithms that Throw Away
Descriptors."
Copyright (C) 2018  Robert Kelly
Changes by Richard Leddy 2023
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

#define FOREVER_check  while (true)

//
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>


using namespace std;


#include "descriptors.h"


namespace concurrent_data_structures {

// strongly suggesting to do inlines... although it should be the default


// N ia number of entries... 
// three stuffed data type
// 1. k-CAS descriptor |-> KCASDescriptorStatus
// 2. data descriptor |-> TaggedPointer
// 3. double compare single swap... descriptor |-> RDCSSDescriptor
// 4. 

template <class Allocator, size_t N>
class Brown_RDCSS {
  //
  public: 

    Brown_RDCSS(const size_t thread_id,RDCSSDescriptor *rdcss_area) : _thread_id(thread_id) {
        _rdcss_descs = rdcss_area;
        _own_rdcss_descs + _thread_id;
    }


  

  protected:

    // _rdcss_descriptor_from
    //
    RDCSSDescriptor *_rdcss_descriptor_from(TaggedPointer ptr) {
      const uint64_t thread_id = TaggedPointer::get_thread_id(ptr);
      RDCSSDescriptor *snapshot_target = _rdcss_descs + thread_id;
      return snapshot_target;
    }

    RDCSSDescriptor *rdcss_descs() {
      return _own_rdcss_descs;
    }
    
    TaggedPointer rdcss_ptr_from(size_t new_sequence) {
      return TaggedPointer::make_rdcss(_thread_id, new_sequence);  // set type flag
    }


    //  
    // rdcss_read PRIVATE
    //
    TaggedPointer rdcss_read(const atomic<TaggedPointer> *location, const std::memory_order memory_order = std::memory_order_seq_cst) {
      //
      TaggedPointer current = location->load(memory_order);
      bool is_rdcss = TaggedPointer::is_rdcss(current)
      while ( is_rdcss ) {
        rdcss_try_snapshot(current);
        current = location->load(memory_order);
        is_rdcss = TaggedPointer::is_rdcss(current)
      };
      return current;
    }

    //  _rdcss_complete PRIVATE
    //
    void _rdcss_complete(RDCSSDescriptor *snapshot, TaggedPointer ptr) {
      // Old 1 and address 1.
      const uint64_t sequence_number = TaggedPointer::get_sequence_number(snapshot->_kcas_tagptr);
      const KCASDescriptorStatus status = snapshot->atom_load_kcas_descr();     // STATUS
      //
      if ( status.seq_same(sequence_number) && status.still_undecided() ) {  // UNDECIDED
        snapshot->location_update(ptr,snapshot->_kcas_tagptr);
      } else {
        snapshot->location_update(ptr,snapshot->_before);
      }
    }



    // TRY SNAPSHOT

    void rdcss_try_snapshot(const TaggedPointer tagptr, bool help = false) {
      RDCSSDescriptor *snapshot_target = _rdcss_descriptor_from(ptr);  // offset to descriptor region from thread id
      //
      RDCSSDescriptor descriptor_snapshot;
      if ( descriptor_snapshot.try_snapshot(snapshot_target, tagptr) ) {
        _rdcss_complete(&descriptor_snapshot, tagptr);
      }
    }


    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    // 
    // rdcss PROTECTED
    //
    TaggedPointer _rdcss(TaggedPointer ptr) {  // assert(TaggedPointer::is_rdcss(ptr));
      //
      RDCSSDescriptor *rdcss_desc = _rdcss_descriptor_from(ptr);  // offset to descriptor region from thread id
      //
      bool success = false;
      while ( !success ) {
        //
        TaggedPointer current = rdcss_desc->atomic_load_data();  // current descriptor data
        // 
        if ( TaggedPointer::is_rdcss(current) ) {
          rdcss_try_snapshot(current);
        } else {
          if ( current != rdcss_desc->_before ) {
            return current;
          }
          success = rdcss_desc->location_update(current,ptr);
        }
        //
      }
      //
      _rdcss_complete(rdcss_desc, ptr);
      return rdcss_desc->_before;
    }


  protected:

    RDCSSDescriptor                   *_rdcss_descs;
    RDCSSDescriptor                   *_own_rdcss_descs;
    size_t                            _thread_id;

};


template <class Allocator, size_t N>
class Brown_KCAS : public Brown_RDCSS {
  //
  public: // CONSTRUCTOR

    Brown_KCAS(const size_t thread_id,KCASDescriptor *kcas_area, RDCSSDescriptor *rdcss_area,const size_t N_procs,bool am_initializer) {
      //
      super(thread_id,rdcss_area);
      //
      _kcas_descs = kcas_area;
      _own_kcas_descs = kcas_area + _thread_id;
      //
      if ( am_initializer ) {
        KCASDescriptor *end_kcas_area = kcas_area + N_procs;
        for ( ; kcas_area < end_kcas_area; kcas_area++ ) {
          kcas_area->_status.store(KCASDescriptorStatus{});
          kcas_area->_num_entries = 0;
        }
      }
      //
    }

    ~Brown_KCAS() {}


  public: // METHODS

    // create_descriptor
    //
    KCASDescriptor *create_descriptor(void) {
      KCASDescriptor *kcas_desc = _own_kcas_descs;
      kcas_desc->increment_sequence();   // Increment the status sequence number.
      kcas_desc->_num_entries = 0;
      return kcas_desc;
    }

  private:

    // _kcas_descriptor_from
    //
    KCASDescriptor *_kcas_descriptor_from(TaggedPointer ptr) {
      const uint64_t thread_id = TaggedPointer::get_thread_id(ptr);      // thread_id
      KCASDescriptor *snapshot_target = _kcas_descs + thread_id;
      return snapshot_target;
    }

    TaggedPointer kcas_ptr_from(size_t new_sequence) {
      return TaggedPointer::make_kcas(_thread_id, new_sequence);  // set type flag
    }



    template <class ValType>
    bool update_if_rdcss_kcas(TaggedPointer &before_desc, KCASEntry<ValType> *location, std::memory_order fail = std::memory_order_seq_cst) {
      //
      if ( TaggedPointer::is_rdcss(before_desc) ) {
        _cas_try_snapshot(before_desc, true);
        before_desc = location->atomic_load(fail);
        return true;
      }
      // Could still be a K-CAS descriptor.
      if ( TaggedPointer::is_kcas(before_desc) ) {
        _cas_try_snapshot(before_desc, true);
        before_desc = location->atomic_load(fail);
        return true;
      }
      //
      return false
    }

    /**
     * _loop_until_bits calls upone update_if_rdcss_kcas (which MUST prevent an infinit loop)
    */
    template <class ValType>
    bool _loop_until_bits(ValType *expected, TaggedPointer &a_desc, KCASEntry<ValType> *location, std::memory_order fail = std::memory_order_seq_cst) {
      while ( TaggedPointer::not_bits(a_desc) ) {
        if ( update_if_rdcss_kcas(a_desc,location,fail) == false) {
          *expected = KCASEntry<ValType>::value_from_raw_bits(a_desc);
          return;
        }
      }
      *expected = KCASEntry<ValType>::value_from_raw_bits(a_desc);
    }



  public: // METHODS

    // CAS
    // Brown_KCAS::cas
    //
    bool cas(KCASDescriptor *desc) {                //     CAS CAS CAS
      //
      desc->sort_kcas_entries();
      desc->increment_sequence();  // Init descriptor...

      TaggedPointer ptr =  kcas_ptr_from(desc->load_status(std::memory_order_relaxed)._sequence_number));
      return _cas_internal(ptr, desc);
    }

    // READ ---
    //
    // Brown_KCAS::read_value   -- a CAS read .. so it tries until it has all the locations
    //
    template <class ValType>
    ValType read_value(const KCASEntry<ValType> *location,const std::memory_order memory_order = std::memory_order_seq_cst) {
                                  // static_assert(!std::is_pointer<ValType>::value, "Type is not a value.");
      // read
      TaggedPointer tp_desc = this->rdcss_read(location->entry_ref(), memory_order);
      // test
      bool its_kcas = TaggedPointer::is_kcas(tp_desc);
      while ( its_kcas ) {   // Could still be a K-CAS descriptor.
        _cas_try_snapshot(tp_desc, true);
        its_kcas = TaggedPointer::is_kcas(tp_desc);
        if ( !its_kcas ) break;
        else { tp_desc = this->rdcss_read(location->entry_ref(), memory_order); } // read again
      }
      return KCASEntry<ValType>::value_from_raw_bits(tp_desc);
    }


    // Brown_KCAS::read_ptr    -- a CAS read .. so it tries until it has all the locations
    //
    template <class PtrType>
    PtrType read_ptr(const KCASEntry<PtrType> *location,const std::memory_order memory_order = std::memory_order_seq_cst) {
      // read
      TaggedPointer tp_desc = this->rdcss_read(location, memory_order);
      // test
      bool its_kcas = TaggedPointer::is_kcas(tp_desc);
      while ( its_kcas ) {   // Could still be a K-CAS descriptor.
        _cas_try_snapshot(tp_desc, true);
        its_kcas = TaggedPointer::is_kcas(tp_desc);
        if ( !its_kcas ) break;
        else { tp_desc = this->rdcss_read(location, memory_order); } // read again
      }
      return KCASEntry<PtrType>::from_raw_bits(tp_desc);
    }

    // WRITE
    //
    // Brown_KCAS::write_value
    template <class ValType>
    void write_value(KCASEntry<ValType> *location, const ValType &val, const memory_order memory_order = std::memory_order_seq_cst) {
                                                          //static_assert(!std::is_pointer<ValType>::value, "Type is not a value.");
      TaggedPointer desc = raw_tp_shift_ptr(val);        //assert(TaggedPointer::is_bits(desc));
      location->atomic_store(desc, memory_order);
    }


    // Brown_KCAS::write_ptr
    template <class PtrType>
    void write_ptr(KCASEntry<PtrType> *location, const PtrType &ptr, const memory_order memory_order = std::memory_order_seq_cst) {
                                                          // static_assert(std::is_pointer<PtrType>::value, "Type is not a pointer.");
      TaggedPointer desc = KCASEntry<PtrType>::raw_tp_ptr(ptr);              //assert(TaggedPointer::is_bits(desc));
      location->atomic_store(desc, memory_order);
    }


    // COMPARE AND EXCHANGE


    template <class ValType>
    bool compare_exchange_weak_value(KCASEntry<ValType> *location, ValType &expected, const ValType &desired,
                                                    std::memory_order success = std::memory_order_seq_cst,
                                                    std::memory_order fail = std::memory_order_seq_cst) {
      TaggedPointer before_desc = KCASEntry<ValType>::raw_tp_ptr(expected);
      TaggedPointer desired_desc = KCASEntry<ValType>::raw_tp_ptr(desired);
      //
      bool ret = location->compare_exchange_weak(before_desc, desired_desc, success, fail);
      if (!ret) {
        _loop_until_bits(&expected,before_desc,location,fail);
        return false;
      }
      return true;
    }


    template <class PtrType>
    bool compare_exchange_weak_ptr(KCASEntry<PtrType> *location, PtrType &expected, const PtrType &desired,
                                                    std::memory_order success = std::memory_order_seq_cst,
                                                    std::memory_order fail = std::memory_order_seq_cst) {
      TaggedPointer before_desc = KCASEntry<PtrType>::raw_tp_ptr(expected);
      TaggedPointer desired_desc = KCASEntry<PtrType>::raw_tp_ptr(desired);
      //
      bool ret = location->compare_exchange_weak(before_desc, desired_desc, success, fail);
      if (!ret) {
        _loop_until_bits(&expected,before_desc,location,fail);
        return false;
      }
      return true;
    }



    template <class ValType>
    bool compare_exchange_strong_value(KCASEntry<ValType> *location, ValType &expected, const ValType &desired,
                                                    std::memory_order success = std::memory_order_seq_cst,
                                                    std::memory_order fail = std::memory_order_seq_cst) {
      TaggedPointer before_desc = KCASEntry<ValType>::raw_tp_ptr(expected);
      TaggedPointer desired_desc = KCASEntry<ValType>::raw_tp_ptr(desired);
      //
      bool ret = location->compare_exchange_strong(before_desc, desired_desc, success, fail);
      if (!ret) {
        _loop_until_bits(&expected,before_desc,location,fail);
        return false
      }
      return true;
    }



  private:

    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    // TRY SNAPSHOT
    // snapshot - thread state (including locking)
    // ptr -- the annotated ptr telling the state of some other thread than the calling thread
    // my_thread_id -- local calling thread.


    void _cas_try_snapshot(const TaggedPointer tagptr, bool help = false) {
      KCASDescriptor *snapshot_target = _kcas_descriptor_from(ptr);   // offset to descriptor region from thread id
      //
      KCASDescriptor descriptor_snapshot;
      if ( descriptor_snapshot.try_snapshot(snapshot_target, tagptr) ) {
        _cas_internal(&descriptor_snapshot, tagptr,  help);
      }
    }



    // Brown_KCAS::_cas_internal -- PRIVATE called only by methods in this class...  see Brown_KCAS::cas
    //

    bool _cas_internal(KCASDescriptor *kc_desc_snapshot, const TaggedPointer tagptr, bool help = false) {
      //
      // Kelly: This is the descriptor we're trying to complete.
      // We also have a snapshot of it as an argument.
      KCASDescriptor *original_desc = _kcas_descriptor_from(tagptr);  // offset to descriptor region from thread id

      // We have two windows into the state:
      // The first is the tagptr which has a state embeded within it.
      // The second is the sequence bits inside the descriptor.
      // They need to match and if they do then the state extracted.
      //
      const uint64_t tagptr_sequence_number = TaggedPointer::get_sequence_number(tagptr);

      KCASDescriptorStatus current_status = original_desc->load_status();

      if ( current_status.seq_different(tagptr_sequence_number) ) {
        // This can only fail if we are helping another descriptor
        // since the owner is the only one who can update the sequence bits.
        // assert(help);
        return false;
      }
      //
      const size_t num_entries = kc_desc_snapshot->_num_entries;
      //
      if ( current_status.still_undecided() ) {   // UNDECIDED
        //
        RDCSSDescriptor *rdcss_desc = this->rdcss_descs();
        //
        uint64_t status = KCASDescStat::SUCCEEDED;    // status
        //
        for ( size_t i = help ? 1 : 0; ((i < num_entries) && succeeded(status) ); i++ ) {
          //
          bool its_kcas = true;
          TaggedPointer maybe_value{0};
          do {
            // Give us a new descriptor.
            rdcss_desc->copy_from_kcas(kc_desc_snapshot,original_desc,tagptr,i);
            // Make it initialised.
            size_t new_sequence = rdcss_desc->increment_sequence();
            //
            TaggedPointer rdcss_ptr = this->rdcss_ptr_from(new_sequence);
            maybe_value = _rdcss(rdcss_ptr);
            //
            its_kcas = TaggedPointer::is_kcas(maybe_value);
            if ( its_kcas && (maybe_value != tagptr) ) {
              _cas_try_snapshot(value,true);
            }
            //
          } while ( its_kcas );   // elihw od
          // 
          if ( maybe_value != rdcss_desc->_before ) {
            status = KCASDescStat::FAILED;
          }
        }  // rof

        // Try change descriptor status.
        KCASDescriptorStatus expected_status = current_status;
        KCASDescriptorStatus original_status = original_desc->load_status(std::memory_order_relaxed);
        //
        if ( original_status.seq_same(expected_status) && original_status.still_undecided() ) {
          original_desc->status_compare_exchange(expected_status,status,current_status._sequence_number);
        }
      }
      // Phase 2.
      bool succeeded = false; 
      KCASDescriptorStatus new_status = original_desc->load_status(std::memory_order_seq_cst);
      if ( new_status.seq_same(tagptr_sequence_number) ) {
        succeeded = new_status.succeeded();
        kc_desc_snapshot->update_entries(tagptr,succeeded);
      }
      //
      return succeeded;
    }


  private:  // DATA


    // TWO TYPES OF DESCRIPTORS

    KCASDescriptor                    *_kcas_descs;
    KCASDescriptor                    *_own_kcas_descs;


};

}
