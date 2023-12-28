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

// Used interchangeably are shapshots and descriptor. Really the decriptor 


// THhe RDCSS class provides method used by just KCAS 

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
    // Most likely this will be another thread's (proc's) descriptor, being rcdss has a snapshot of some place in memory in which we are interested.
    // This as opposed to "own" rdcss, but may be it is.
    //
    inline RDCSSDescriptor *_rdcss_descriptor_from(TaggedPointer ptr) {
      const uint64_t thread_id = TaggedPointer::get_thread_id(ptr);
      RDCSSDescriptor *snapshot_target = (_rdcss_descs + thread_id);
      return snapshot_target;
    }


    // Calling thread (proc) will have a pointer to its very own rdcss descriptor...
    inline RDCSSDescriptor *theads_own_rdcss_descs() {
      return _own_rdcss_descs;
    }
    
    inline TaggedPointer _construct_rdcss_ptr(size_t new_sequence) {
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
    inline void _rdcss_complete(RDCSSDescriptor *snapshot, TaggedPointer ptr) {
      // Old 1 and address 1.
      const uint64_t sequence_number = TaggedPointer::get_sequence_number(snapshot->_kcas_tagptr);
      const KCASDescriptorStatus status = snapshot->atom_load_kcas_descr();     // STATUS
      //
      if ( status.seq_same(sequence_number) && status.still_undecided() ) {  // UNDECIDED
        snapshot->location_update(ptr,snapshot->_kcas_tagptr);   // put in the kcas control word, no contention for this position
      } else {
        snapshot->location_update(ptr,snapshot->_before);
      }
    }

    // TRY SNAPSHOT

    void rdcss_try_snapshot(const TaggedPointer tagptr, bool help = false) {
        //
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


}
