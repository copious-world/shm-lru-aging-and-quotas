#pragma once

/*
Implementation of fast K-CAS by
"Reuse, don't Recycle: Transforming Lock-Free Algorithms that Throw Away
Descriptors."

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

#define FOREVER_check  while (true)

//
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>


using namespace std;

#include "rdcss.h"

namespace concurrent_data_structures {




// strongly suggesting to do inlines... although it should be the default

// N ia number of entries... 
// three stuffed data type
// 1. k-CAS descriptor |-> KCASDescriptorStatus
// 2. data descriptor |-> TaggedPointer
// 3. double compare single swap... descriptor |-> RDCSSDescriptor
//

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

    // reuse_descriptor
    //
    KCASDescriptor *reuse_descriptor(void) {       // reuse descriptor
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
    bool _update_if_rdcss_kcas(TaggedPointer &before_desc, KCASEntry<ValType> *location, std::memory_order fail = std::memory_order_seq_cst) {
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
     * _loop_until_bits calls upone _update_if_rdcss_kcas (which MUST prevent an infinit loop)
    */
    template <class ValType>
    bool _loop_until_bits(ValType *expected, TaggedPointer &a_desc, KCASEntry<ValType> *location, std::memory_order fail = std::memory_order_seq_cst) {
      while ( TaggedPointer::not_bits(a_desc) ) {
        if ( _update_if_rdcss_kcas(a_desc,location,fail) == false) {
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
      desc->sort_kcas_entries();    // 
      desc->increment_sequence();   // Init descriptor...

      // ket a kcas marked tagged pointer (a value) from this Brown_KCASS descriptor (and using just the sequence number)
      TaggedPointer ptr =  kcas_ptr_from(desc->load_status(std::memory_order_relaxed)._sequence_number));
      return _cas_internal(desc,ptr);
    }

    // READ ---
    //
    // Brown_KCAS::read_value   -- a rdcss read
    //
    template <class ValType>
    ValType read_value(const KCASEntry<ValType> *app_entry,const std::memory_order memory_order = std::memory_order_seq_cst) {
                                  // static_assert(!std::is_pointer<ValType>::value, "Type is not a value.");
      // read
      // get the entry of the tagged ptr (_entry field), which may be a KCAS entry or not... 
      // 
      TaggedPointer tp_desc = this->rdcss_read(app_entry->entry_ref(), memory_order);   // rdcss is a 2-CASS always
      // test
      bool its_kcas = TaggedPointer::is_kcas(tp_desc);
      while ( its_kcas ) {   // Could still be a K-CAS descriptor.  (very likely this is never called in the hopscotch app)
        _cas_try_snapshot(tp_desc, true);
        its_kcas = TaggedPointer::is_kcas(tp_desc);
        if ( !its_kcas ) break;
        else { tp_desc = this->rdcss_read(app_entry->entry_ref(), memory_order); } // read again
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
      KCASDescriptor *snapshot_target = _kcas_descriptor_from(tagptr);   // offset to descriptor region from thread id
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
        // use rdcss to get the values for the entries..
        RDCSSDescriptor *rdcss_desc = this->theads_own_rdcss_descs();   // the rdcss descriptor for this process
        //
        uint64_t status = KCASDescStat::SUCCEEDED;    // status
        //
        for ( size_t i = help ? 1 : 0; ((i < num_entries) && succeeded(status) ); i++ ) {
          //
          bool its_kcas = true;
          TaggedPointer maybe_value_state;
          do {        // {(DO)}
            // get the next loaded entry                      ith entry
            rdcss_desc->copy_from_kcas_entry(kc_desc_snapshot,i,original_desc,tagptr);
            //
            TaggedPointer rdcss_ptr = _construct_rdcss_ptr( rdcss_desc->increment_sequence() ); // Make it initialised.
            maybe_value_state = _rdcss(rdcss_ptr);
            //
            its_kcas = TaggedPointer::is_kcas(maybe_value_state);
            if ( its_kcas && (maybe_value_state != tagptr) ) {      // possibly a tree of CAS (depth first lockdown)
              _cas_try_snapshot(maybe_value_state,true);
            }
            //
          } while ( its_kcas );   // w-{(OD)}
          // 
          if ( maybe_value_state != rdcss_desc->_before ) {   // don't have access to the raw bits governed by this tagptr
            status = KCASDescStat::FAILED;
          }
        }  // rof

        // Try change descriptor status.
        KCASDescriptorStatus expected_status = current_status;  // this entire CAS status before trying locks
        KCASDescriptorStatus original_status = original_desc->load_status(std::memory_order_relaxed);  // where the status is now
        //
        if ( original_status.seq_same(expected_status) && original_status.still_undecided() ) {  // this proc controls the status
          // set it to whatever we got locking down the entries ... the status word made of new value... 
          original_desc->status_compare_exchange(expected_status,status,current_status._sequence_number);
        }
      }
      // Phase 2.
      bool succeeded = false; 
      KCASDescriptorStatus new_status = original_desc->load_status(std::memory_order_seq_cst);  // where the status is now (once again)
      if ( new_status.seq_same(tagptr_sequence_number) ) {  // 'on the same page'
        succeeded = new_status.succeeded();                 // if we were able to set it, this is what we set it to
        kc_desc_snapshot->update_entries(tagptr,succeeded); // now make changes to the _entry at the _location (addr) of each entry set by the app
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
