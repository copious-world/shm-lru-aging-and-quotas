#pragma once

/*
Implementation of fast K-CAS by
"Reuse, don't Recycle: Transforming Lock-Free Algorithms that Throw Away
Descriptors."
Copyright (C) 2018  Robert Kelly
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

#define FOREVER  while (true)

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


static const size_t KCASShift = 2;

// N ia number of entries... 
// three stuffed data type
// 1. k-CAS descriptor |-> KCASDescriptorStatus
// 2. data descriptor |-> TaggedPointer
// 3. double compare single swap... descriptor |-> RDCSSDescriptor
// 4. 

template <class Allocator, size_t N>
class Brown_KCAS {
  //
  public: // CONSTRUCTOR

    Brown_KCAS(const size_t thread_id,const size_t N_procs,bool am_initializer) : _thread_id(thread_id) {
      //
      if ( am_initializer ) {
        for ( size_t i = 0; i < N_procs; i++ ) {
          _kcas_descs[i]._status.store(KCASDescriptorStatus{});
          _kcas_descs[i]._num_entries = 0;
        }
      }
      //
    }

    ~Brown_KCAS() {}


  public: // METHODS


    KCASDescriptor *create_descriptor(void) {
      _kcas_descs[_thread_id].increment_sequence();   // Increment the status sequence number.
      _kcas_descs[_thread_id]._num_entries = 0;
      return &_kcas_descs[_thread_id];
    }


    // CAS
    // Brown_KCAS::cas
    //
    bool cas(KCASDescriptor *desc) {
      desc->sort_kcas_entries();
      desc->increment_sequence();  // Init descriptor...
      TaggedPointer ptr = TaggedPointer::make_kcas(_thread_id,
                                              (desc->load_status(std::memory_order_relaxed)._sequence_number));
      return cas_internal(ptr, desc);
    }

    // READ ---
    //
    // Brown_KCAS::read_value
    //
    template <class ValType>
    ValType read_value(const KCASEntry<ValType> *location,const std::memory_order memory_order = std::memory_order_seq_cst) {
                                  // static_assert(!std::is_pointer<ValType>::value, "Type is not a value.");
      FOREVER {
        TaggedPointer tp_desc = this->rdcss_read(location->entry_ref(), memory_order);
        // Could still be a K-CAS descriptor.
        if ( TaggedPointer::is_kcas(tp_desc) ) {
          cas_try_snapshot(tp_desc, true);
        } else {                                      // assert(TaggedPointer::is_bits(tp_desc));
          return KCASEntry<ValType>::from_raw_bits(tp_desc._raw_bits >> KCASShift);
        }
      }
    }


    // Brown_KCAS::read_ptr
    template <class PtrType>
    PtrType read_ptr(const KCASEntry<PtrType> *location,const std::memory_order memory_order = std::memory_order_seq_cst) {
                                  // static_assert(std::is_pointer<PtrType>::value, "Type is not a pointer.");
      FOREVER {
        TaggedPointer tp_desc = this->rdcss_read(location, memory_order);
        // Could still be a K-CAS descriptor.
        if ( TaggedPointer::is_kcas(tp_desc) ) {
          cas_try_snapshot(tp_desc, true);
        } else {
          assert(TaggedPointer::is_bits(desc));
          return KCASEntry<PtrType>::from_raw_bits(desc);
        }
      }
    }



    // WRITE
    // Brown_KCAS::write_value
    template <class ValType>
    void write_value(KCASEntry<ValType> *location, const ValType &val, const memory_order memory_order = std::memory_order_seq_cst) {
                                                          //static_assert(!std::is_pointer<ValType>::value, "Type is not a value.");
      TaggedPointer desc = raw_tps_shift_ptr(val);        //assert(TaggedPointer::is_bits(desc));
      location->atomic_store(desc, memory_order);
    }


    // Brown_KCAS::write_ptr
    template <class PtrType>
    void write_ptr(KCASEntry<PtrType> *location, const PtrType &ptr, const memory_order memory_order = std::memory_order_seq_cst) {
                                                          // static_assert(std::is_pointer<PtrType>::value, "Type is not a pointer.");
      TaggedPointer desc = raw_tps_ptr(ptr);              //assert(TaggedPointer::is_bits(desc));
      location->atomic_store(desc, memory_order);
    }


    // COMPARE AND EXCHANGE


    template <class ValType>
    bool compare_exchange_weak_value(KCASEntry<ValType> *location, ValType &expected, const ValType &desired,
                                                  std::memory_order success = std::memory_order_seq_cst,
                                                  std::memory_order fail = std::memory_order_seq_cst) {
      TaggedPointer before_desc = raw_tps_ptr(expected);
      TaggedPointer desired_desc = raw_tps_ptr(desired);
      //
      bool ret = location->compare_exchange_weak(before_desc, desired_desc, success, fail);
      if (!ret) {
        return loop_until_bits(&expected,before_desc,location,fail);
      }
      return true;
    }


    template <class PtrType>
    bool compare_exchange_weak_ptr(KCASEntry<PtrType> *location, PtrType &expected, const PtrType &desired,
                                                    std::memory_order success = std::memory_order_seq_cst,
                                                    std::memory_order fail = std::memory_order_seq_cst) {
      TaggedPointer before_desc = raw_tps_ptr(expected);
      TaggedPointer desired_desc = raw_tps_ptr(desired);
      //
      bool ret = location->compare_exchange_weak(before_desc, desired_desc, success, fail);
      if (!ret) {
        return loop_until_bits(&expected,before_desc,location,fail);
      }
      return true;
    }



    template <class ValType>
    bool compare_exchange_strong_value(KCASEntry<ValType> *location, ValType &expected, const ValType &desired,
                                            std::memory_order success = std::memory_order_seq_cst,
                                            std::memory_order fail = std::memory_order_seq_cst) {
      TaggedPointer before_desc = raw_tps_ptr(expected);
      TaggedPointer desired_desc = raw_tps_ptr(desired);
      //
      bool ret = location->compare_exchange_strong(before_desc, desired_desc, success, fail);
      if (!ret) {
        return loop_until_bits(&expected,before_desc,location,fail);
      }
      return true;
    }


  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  private:

    RDCSSDescriptor *rdcss_descs() {
      return &m_rdcss_descs[_thread_id];
    }
    
    TaggedPointer rdcss_ptr_from(size_t new_sequence) {
      return TaggedPointer::make_rdcss(_thread_id, new_sequence);  // set type flag
    }

    //  
    // rdcss_read PRIVATE
    //
    TaggedPointer rdcss_read(const atomic<TaggedPointer> *location, const std::memory_order memory_order = std::memory_order_seq_cst) {
      FOREVER {
        TaggedPointer current = location->load(memory_order);
        if ( TaggedPointer::is_rdcss(current) ) {
          rdcss_try_snapshot(current);
        } else {
          return current;
        }
      }
    }

    //  rdcss_complete PRIVATE
    //
    void rdcss_complete(RDCSSDescriptor *snapshot, TaggedPointer ptr) {
      // Old 1 and address 1.
      const uint64_t sequence_number = TaggedPointer::get_sequence_number(snapshot->_kcas_tagptr);
      const KCASDescriptorStatus status = snapshot->atom_load_kcas_descr();
      //
      if ( status.seq_same(sequence_number) && status.still_undecided() ) {  // UNDECIDED
        snapshot->location_update(ptr,snapshot->_kcas_tagptr);
      } else {
        snapshot->location_update(ptr,snapshot->_before);
      }
    }




    // Brown_KCAS::cas_internal -- PRIVATE called only by methods in this class...  see Brown_KCAS::cas
    //

    bool cas_internal(KCASDescriptor *descriptor_snapshot, const TaggedPointer tagptr, bool help = false) {
      //
      // Kelly: This is the descriptor we're trying to complete.
      // We also have a snapshot of it as an argument.
      KCASDescriptor *original_desc = _kcas_descriptor_from(tagptr);

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
      const size_t num_entries = descriptor_snapshot->_num_entries;
      //
      if ( current_status.still_undecided() ) {   // UNDECIDED
        //
        RDCSSDescriptor *rdcss_desc = this->rdcss_descs();
        //
        uint64_t status = KCASDescStat::SUCCEEDED;    // status
        //
        for ( size_t i = help ? 1 : 0; ((i < num_entries) && succeeded(status) ); i++ ) {
          //
          FOREVER {
            // Give us a new descriptor.
            rdcss_desc->copy_from_kcas(descriptor_snapshot,original_desc,tagptr,i);
            // Make it initialised.
            size_t new_sequence = rdcss_desc->increment_sequence();
            //
            TaggedPointer rdcss_ptr = this->rdcss_ptr_from(new_sequence);
            TaggedPointer value = this->rdcss(rdcss_ptr);
            //
            if ( !(TaggedPointer::is_kcas(value)) ) {
              if ( value._raw_bits != rdcss_desc->_before._raw_bits ) {
                status = KCASDescStat::FAILED;
              }
              break;
            } else if ( value._raw_bits != tagptr._raw_bits ) {
              cas_try_snapshot(value,true);
            }
            //
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
        descriptor_snapshot->update_entries(tagptr,succeeded);
      }
      return succeeded;
    }


    template <class ValType>
    bool update_if_rdcss_kcas(TaggedPointer &before_desc, KCASEntry<ValType> *location, std::memory_order fail = std::memory_order_seq_cst) {
      //
      if ( TaggedPointer::is_rdcss(before_desc) ) {
        cas_try_snapshot(before_desc, true);
        before_desc = location->atomic_load(fail);
        return true;
      }
      // Could still be a K-CAS descriptor.
      if ( TaggedPointer::is_kcas(before_desc) ) {
        cas_try_snapshot(before_desc, true);
        before_desc = location->atomic_load(fail);
        return true;
      }
      //
      return false
    }


    template <class PtrType>
    TaggedPointers raw_tps_ptr(PtrType value) {
      intptr_t some_raw_bits = KCASEntry<PtrType>::to_raw_bits(value);
      TaggedPointer a_desc{some_raw_bits};
      return a_desc;
    }

    template <class ValType>
    TaggedPointers raw_tps_shift_ptr(ValType value) {
      intptr_t some_raw_bits = KCASEntry<ValType>::to_raw_bits(value);
      TaggedPointer a_desc{some_raw_bits << KCASShift};
      return a_desc;
    }



    /**
     * loop_until_bits calls upone update_if_rdcss_kcas (which MUST prevent an infinit loop)
    */

    template <class ValType>
    bool loop_until_bits(ValType *expected, TaggedPointer &a_desc, KCASEntry<ValType> *location, std::memory_order fail = std::memory_order_seq_cst) {
      FOREVER {
        if ( TaggedPointer::is_bits(a_desc) ) {
          *expected = KCASEntry<ValType>::from_raw_bits(a_desc._raw_bits >> KCASShift);
          return false;
        }
        // a_desc updates
        if ( update_if_rdcss_kcas(a_desc,location,fail) == false) {
          *expected = KCASEntry<ValType>::from_raw_bits(a_desc._raw_bits >> KCASShift);
          return false;
        }
      }
    }

    RDCSSDescriptor *_descriptor_from(TaggedPointer ptr) {
      const uint64_t thread_id = TaggedPointer::get_thread_id(ptr);
      RDCSSDescriptor *snapshot_target = &m_rdcss_descs[thread_id];
      return snapshot_target;
    }

    KCASDescriptor *_kcas_descriptor_from(TaggedPointer ptr) {
      const uint64_t thread_id = TaggedPointer::get_thread_id(ptr);      // thread_id
      KCASDescriptor *snapshot_target = &_kcas_descs[thread_id];
      return snapshot_target;
    }

   
    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    // TRY SNAPSHOT
    // snapshot - thread state (including locking)
    // ptr -- the annotated ptr telling the state of some other thread than the calling thread
    // my_thread_id -- local calling thread.


    // TRY SNAPSHOT

    void rdcss_try_snapshot(const TaggedPointer tagptr, bool help = false) {
      RDCSSDescriptor descriptor_snapshot;
      RDCSSDescriptor *snapshot_target = _descriptor_from(ptr);
      if ( descriptor_snapshot.try_snapshot(snapshot_target, tagptr) ) {
        rdcss_complete(&descriptor_snapshot, tagptr);
      }
    }


    void cas_try_snapshot(const TaggedPointer tagptr, bool help = false) {
      KCASDescriptor descriptor_snapshot;
      KCASDescriptor *snapshot_target = _kcas_descriptor_from(ptr);
      if ( descriptor_snapshot.try_snapshot(snapshot_target, tagptr) ) {
        cas_internal(&descriptor_snapshot, tagptr,  help);
      }
    }


    // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

    // 
    // rdcss PRIVATE
    //
    TaggedPointer rdcss(TaggedPointer ptr) {  // assert(TaggedPointer::is_rdcss(ptr));
      //
      RDCSSDescriptor *rdcss_desc = _descriptor_from(ptr);
      //
      bool success = false;
      while ( !success ) {
        //
        TaggedPointer current = rdcss_desc->atomic_load_data();
        if ( TaggedPointer::is_rdcss(current) ) {
          rdcss_try_snapshot(current);
        } else {
          //
          if ( current._raw_bits != rdcss_desc->_before._raw_bits ) {
            return current;
          }
          //
          success = rdcss_desc->location_update(current,ptr);
        }
        //
      }
      //
      rdcss_complete(rdcss_desc, ptr);
      return rdcss_desc->_before;
    }

  private:


    // TWO TYPES OF DESCRIPTORS

    KCASDescriptor                    *_kcas_descs;
    RDCSSDescriptor                   *m_rdcss_descs;
    size_t                            _thread_id;


};

}
