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

#include "primitives/cache_utils.h"
//
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>


using namespace std;


namespace concurrent_data_structures {


// N ia number of entries... 
// three stuffed data type
// 1. k-CAS descriptor |-> KCASDescriptorStatus
// 2. data descriptor |-> TaggedPointer
// 3. double compare single swap... descriptor |-> RDCSSDescriptor
// 4. 

template <class Allocator, size_t N> class Brown_KCAS {
  //
private:

  //
  static const size_t S_NUM_THREADS = 144;

  // 2 bits
  static const intptr_t S_NO_TAG = 0x0;
  static const intptr_t S_KCAS_TAG = 0x1;
  static const intptr_t S_RDCSS_TAG = 0x2;

  // 8 Bits
  static const intptr_t S_THREAD_ID_SHIFT = 2;
  static const intptr_t S_THREAD_ID_MASK = (std::intptr_t(1) << 8) - 1;

  // 54 bits
  static const intptr_t S_SEQUENCE_SHIFT = 10;
  static const intptr_t S_SEQUENCE_MASK = (std::intptr_t(1) << 54) - 1;


  // 1. TaggedPointer - data descriptor   PRIVATE
  //
  struct TaggedPointer {

    intptr_t raw_bits;

    static bool is_rdcss(const TaggedPointer tagptr) {
      return (tagptr.raw_bits & S_RDCSS_TAG) == S_RDCSS_TAG;
    }

    static bool is_kcas(const TaggedPointer tagptr) {
      return (tagptr.raw_bits & S_KCAS_TAG) == S_KCAS_TAG;
    }

    static bool is_bits(const TaggedPointer tagptr) {
      return !is_kcas(tagptr) && !is_rdcss(tagptr);
    }

    explicit TaggedPointer() noexcept : raw_bits(0) {}

    explicit TaggedPointer(const intptr_t raw_bits) : raw_bits(raw_bits) {
      assert(is_bits(*this));
    }

    explicit TaggedPointer(const intptr_t tag_bits, const intptr_t thread_id, const intptr_t sequence_number)
        : raw_bits(tag_bits | (thread_id << S_THREAD_ID_SHIFT) | sequence_number << S_SEQUENCE_SHIFT)) {}

    static const intptr_t get_thread_id(const TaggedPointer tagptr) {
      assert(!is_bits(tagptr));
      return (tagptr.raw_bits >> S_THREAD_ID_SHIFT) & S_THREAD_ID_MASK;
    }

    static const intptr_t get_sequence_number(const TaggedPointer tagptr) {
      assert(!is_bits(tagptr));
      return (tagptr.raw_bits >> S_SEQUENCE_SHIFT) & S_SEQUENCE_MASK;
    }

    static TaggedPointer make_tagged(const intptr_t tag_bits, const TaggedPointer tagptr) {
      return TaggedPointer{tag_bits, get_thread_id(tagptr), get_sequence_number(tagptr)};
    }

    static TaggedPointer mask_bits(const TaggedPointer tagptr) {
      return make_tagged(S_NO_TAG, tagptr);
    }

    static TaggedPointer make_rdcss(const intptr_t thread_id, const intptr_t sequence_number) {
      return TaggedPointer{S_RDCSS_TAG, thread_id, sequence_number};
    }

    static TaggedPointer make_rdcss(const TaggedPointer tagptr) {
      return make_tagged(S_RDCSS_TAG, tagptr);
    }

    static TaggedPointer make_kcas(const intptr_t thread_id, const intptr_t sequence_number) {
      return TaggedPointer{S_KCAS_TAG, thread_id, sequence_number};
    }

    static TaggedPointer make_kcas(const TaggedPointer tagptr) {
      return make_tagged(S_KCAS_TAG, tagptr);
    }

    static TaggedPointer make_bits(const intptr_t raw_bits) {
      return make_bits(TaggedPointer{raw_bits});
    }

    static TaggedPointer make_bits(const TaggedPointer tagptr) {
      return TaggedPointer{tagptr.raw_bits};
    }
  };


  public:


  struct DescriptorEntry {
    atomic<TaggedPointer>   *location;
    TaggedPointer           before;
    TaggedPointer           desired;
  };

  private:

  // 2. KCASDescriptorStatus - k-CAS descriptor ...   PRIVATE
  // 
  struct KCASDescriptorStatus {
    //
    static const uintptr_t UNDECIDED = 0, SUCCEEDED = 1, FAILED = 2;
    uintptr_t status : 8;
    uintptr_t sequence_number : 54;
    //
    explicit KCASDescriptorStatus() noexcept : status(UNDECIDED), sequence_number(0) {}
    explicit KCASDescriptorStatus(const uintptr_t status, const uintptr_t sequence_number) 
                                                                  : status(status), sequence_number(sequence_number) {}
  };


  // 3. RDCSSDescriptor -- double compare single swap... descriptor    PRIVATE
  // atomics e.g. state information and shared value location which the compiler will use CAS type ops to manipulate
  struct RDCSSDescriptor { 
    //
    atomic_size_t         sequence_bits;           // seq or counter... atomic
    atomic<TaggedPointer> *data_location;
    atomic<KCASDescriptorStatus>  *status_location;
    //
    TaggedPointer         before;
    TaggedPointer         kcas_tagptr;

    //
    inline size_t increment_sequence() {   // inline understood 
      return sequence_bits.fetch_add(1, std::memory_order_acq_rel);
    }

    inline size_t atomic_load_sequence() {
      return sequence_bits.load();
    }

    inline TaggedPointer atomic_load_data() {
      return data_location->load(std::memory_order_relaxed);
    }

    inline KCASDescriptorStatus atom_load_kcas_descr() {
      return status_location->load(std::memory_order_relaxed);
    }

    //
    void copy_from_kcas(KCASDescriptor *descriptor_snapshot, const TaggedPointer tagptr,size_t i) {
      this->increment_sequence();
      this->data_location = descriptor_snapshot->m_entries[i].location;
      this->before = descriptor_snapshot->m_entries[i].before;
      this->kcas_tagptr = tagptr;
      this->status_location = &original_desc->m_status;
    }

    //
    void copy_from_rdss_descriptor(RDCSSDescriptor *from,const size_t before_sequence) {
      this->sequence_bits.store(before_sequence, std::memory_order_relaxed);
      this->data_location = from->data_location;
      this->before = from->before;
      this->kcas_tagptr = from->kcas_tagptr;
      this->status_location = from->status_location;
    }

  };


public:

  class KCASDescriptor;       // BELOW --

  // ---
  // KCASEntry

  // Class to wrap around the types being KCAS'd
  template <class Type>
  class KCASEntry {
  private:
    //
    union ItemBitsUnion {
      ItemBitsUnion() : raw_bits(0) {}
      Type item;
      intptr_t raw_bits;
    };
    //
    atomic<TaggedPointer> m_entry;

    // METHODS
    static Type from_raw_bits(intptr_t raw_bits) {
      ItemBitsUnion ibu;
      ibu.raw_bits = raw_bits;
      return ibu.item;
    }
    static intptr_t to_raw_bits(const Type &inner) {
      ItemBitsUnion ibu;
      ibu.item = inner;
      return ibu.raw_bits;
    }

  public:
    friend class Brown_KCAS;
    friend class KCASDescriptor;


  public:

    atomic<TaggedPointer> *entry_ref() {
      return &m_entry;
    }


    inline TaggedPointer atomic_load(std::memory_order fail) {
      return m_entry.load(fail)
    }

    inline void atomic_store(TaggedPointer desc, const std::memory_order memory_order = std::memory_order_seq_cst) {
      m_entry.store(desc, const std::memory_order memory_order = std::memory_order_seq_cst);
    }

    bool compare_exchange_weak(TaggedPointer &expected, const TaggedPointer &desired,
                                                            std::memory_order success, std::memory_order fail) {
      return m_entry.compare_exchange_weak(expected, desired, success, fail);
    }

    bool compare_exchange_strong(TaggedPointer &expected, const TaggedPointer &desired,
                                                            std::memory_order success, std::memory_order fail) {
      return m_entry.compare_exchange_strong(expected, desired, success, fail);
    }

    bool copy_entries_from(KCASDescriptorStatus * snapshot_target) {
        const size_t num_entries = snapshot_target->m_num_entries;
        for ( size_t i = 0; i < num_entries; i++ ) {
          this->m_entries[i] = snapshot_target->m_entries[i];
        }
        const KCASDescriptorStatus after_status = snapshot_target->load_status();
        //
        if ( after_status.sequence_number != sequence_number ) {
          return false;
        }
        this->m_num_entries = num_entries;
        return true;
    }

  };



  // DESCRIPTOR

  class KCASDescriptor {

    private:
    
      size_t                        m_num_entries;              // incremented in ADD methods
      atomic<KCASDescriptorStatus>  m_status;
      DescriptorEntry               m_entries[N];  // N - template parameter ---- 

      //

      KCASDescriptor() : m_num_entries(0), m_status(KCASDescriptorStatus{}) {}
      KCASDescriptor(const KCASDescriptor &rhs) = delete;
      KCASDescriptor &operator=(const KCASDescriptor &rhs) = delete;
      KCASDescriptor &operator=(KCASDescriptor &&rhs) = delete;
 
      // KCASDescriptor method -- increment_sequence
      //
      void increment_sequence() {
        KCASDescriptorStatus current_status =  load_status(std::memory_order_relaxed);
        m_status.store(KCASDescriptorStatus{KCASDescriptorStatus::UNDECIDED, current_status.sequence_number + 1},
                                std::memory_order_release);
      }

    public:

      inline KCASDescriptorStatus load_status(std::memory_order prefered = std::memory_order_acquire) {
        return m_status.load(prefered);
      }

      inline void status_compare_exchange(KCASDescriptorStatus &expected_status,uintptr_t status, uintptr_t sequence_number) {
        m_status.compare_exchange_strong(expected_status,KCASDescriptorStatus{status,sequence_number},
                    std::memory_order_relaxed, std::memory_order_relaxed);
      }

      inline void sort_kcas_entries() {
         sort(&m_entries[0], &m_entries[m_num_entries],   // begin,end (of entries)
                      [](const DescriptorEntry &lhs, const DescriptorEntry &rhs) -> bool {
                        return lhs.location < rhs.location;
                      });
      }

      template <class ValType>
      void add_value(KCASEntry<ValType> *location, const ValType &before, const ValType &desired) {
        //
        TaggedPointers before_desc = raw_tps_shift_ptr(before);         assert(TaggedPointer::is_bits(before_desc));
        TaggedPointers desired_desc = raw_tps_shift_ptr(desired);       assert(TaggedPointer::is_bits(desired_desc));
        //
        const size_t cur_entry = m_num_entries++;
        assert(cur_entry < N);
        //
        m_entries[cur_entry].before = before_desc;
        m_entries[cur_entry].desired = desired_desc;
        m_entries[cur_entry].location = location->entry_ref();
        //
      }

      template <class PtrType>
      void add_ptr(const KCASEntry<PtrType> *location, const PtrType &before, const PtrType &desired) {
        //        
        TaggedPointers before_desc = raw_tps_ptr(before);       assert(TaggedPointer::is_bits(before_desc));
        TaggedPointers desired_desc = raw_tps_ptr(desired);     assert(TaggedPointer::is_bits(desired_desc));
        //
        const size_t cur_entry = m_num_entries++;
        assert(cur_entry < N);
        //
        m_entries[cur_entry].before = before_desc;
        m_entries[cur_entry].desired = desired_desc;
        m_entries[cur_entry].location = location->entry_ref();
        //
      }

      void update_entries(bool succeeded,TaggedPointer tagptr) {
        size_t num_entries = m_num_entries;
        for ( size_t i = 0; i < num_entries; i++ ) {
          TaggedPointer new_value = succeeded ? descriptor_snapshot->m_entries[i].desired
                                              : descriptor_snapshot->m_entries[i].before;
          TaggedPointer expected = tagptr;
          this->m_entries[i].location->compare_exchange_strong(expected, new_value,std::memory_order_relaxed,std::memory_order_relaxed);
        }
      }



      friend class Brown_KCAS;
      template <class T> friend class CachePadded;
    };




  // 

  void rdcss_try_snapshot(const TaggedPointer tagptr, bool help = false) {
    RDCSSDescriptor descriptor_snapshot;
    if ( try_snapshot(&descriptor_snapshot, tagptr) ) {
      rdcss_complete(&descriptor_snapshot, tagptr);
    }
  }


  void cas_try_snapshot(const TaggedPointer tagptr, bool help = false) {
    KCASDescriptor descriptor_snapshot;
    if ( try_snapshot(&descriptor_snapshot, tagptr) ) {
      cas_internal(&descriptor_snapshot, tagptr,  help);
    }
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
        *expected = KCASEntry<ValType>::from_raw_bits(a_desc.raw_bits >> KCASShift);
        return false;
      }
      // a_desc updates
      if ( update_if_rdcss_kcas(a_desc,location,fail) == false) {
        *expected = KCASEntry<ValType>::from_raw_bits(a_desc.raw_bits >> KCASShift);
        return false;
      }
    }
  }

  RDCSSDescriptor *descriptor_from(TaggedPointer ptr) {
    const uintptr_t thread_id = TaggedPointer::get_thread_id(ptr);
    RDCSSDescriptor *snapshot_target = &m_rdcss_descs[thread_id];
    return snapshot_target;
  }

  KCASDescriptor *kcas_descriptor_from(TaggedPointer ptr) {
    const uintptr_t thread_id = TaggedPointer::get_thread_id(ptr);      // thread_id
    KCASDescriptor *snapshot_target = &m_kcas_descs[thread_id];
    return snapshot_target;
  }


public:

  // METHODS - Brown_KCAS

  // try_snapshot PRIVATE
  // snapshot - thread state (including locking)
  // ptr -- the annotated ptr telling the state of some other thread than the calling thread
  // my_thread_id -- local calling thread.
  bool try_snapshot(RDCSSDescriptor *snapshot, TaggedPointer ptr) {
    //
    const uintptr_t sequence_number = TaggedPointer::get_sequence_number(ptr);   // sequence_number - what we think it is

    // Based on thread id ... get addresss of the rdcss descriptor for the particular thread... 
    // If this is to be part of a thread by thread table, then this array will be interleaved... hence rdcss in thread def region.
    RDCSSDescriptor *snapshot_target = this->descriptor_from(ptr);

    // load the bits in the sequece...
    const size_t before_sequence = snapshot_target->atomic_load_sequence(); // what it seems to be in actuality...
    //
    if ( before_sequence != sequence_number ) {   // someone else changed this while we started inspecting it
      return false;       // no control over the sequence ... so leave
    }

    // Snapshot - the snapshot passed (whose is it?) that thread now gets the bits stored by the other thread
    // so ... copy all fields
    snapshot->copy_from_rdss_descriptor(snapshot_target,before_sequence);
    // should now have a grasp of reality

    // Check our sequence number again.
    const size_t after_sequence = snapshot_target->atomic_load_sequence();
    if ( after_sequence != sequence_number ) {
      return false;
    }

    return true;
  }

  //
  // Brown_KCAS::try_snapshot -- PRIVATE called only by methods in this class... 
  //
  bool try_snapshot(KCASDescriptor *snapshot, TaggedPointer ptr) {
    //
    const uintptr_t sequence_number = TaggedPointer::get_sequence_number(ptr);
    //
    KCASDescriptor *snapshot_target = this->kcas_descriptor_from(ptr);
    //
    const KCASDescriptorStatus before_status = snapshot_target->load_status();
    //
    if ( before_status.sequence_number != sequence_number ) {
      return false;
    } else {
      return snapshot->copy_entries_from(snapshot_target);
    }
  }

  // 
  // rdcss PRIVATE
  //
  TaggedPointer rdcss(TaggedPointer ptr) {  // assert(TaggedPointer::is_rdcss(ptr));
    //
    RDCSSDescriptor *rdcss_desc = descriptor_from(ptr);
    //
    bool success = false;
    while ( !succes ) {
      //
      TaggedPointer current = rdcss_desc->atomic_load_data();
      if ( TaggedPointer::is_rdcss(current) ) {
        rdcss_try_snapshot(current);
      } else {
        //
        if ( current.raw_bits != rdcss_desc->before.raw_bits ) {
          return current;
        }
        //
        success = rdcss_desc->data_location->compare_exchange_strong(current, ptr, 
                                                                            std::memory_order_relaxed,
                                                                            std::memory_order_relaxed);
      }
      //
    }
    //
    rdcss_complete(rdcss_desc, ptr);
    return rdcss_desc->before;
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
    const uintptr_t sequence_number = TaggedPointer::get_sequence_number(snapshot->kcas_tagptr);
    const KCASDescriptorStatus status = snapshot->atom_load_kcas_descr();
    if ( (status.sequence_number == sequence_number) && (status.status == KCASDescriptorStatus::UNDECIDED) ) {  // UNDECIDED
      snapshot->data_location->compare_exchange_strong(ptr, snapshot->kcas_tagptr, std::memory_order_relaxed, std::memory_order_relaxed);
    } else {
      snapshot->data_location->compare_exchange_strong(ptr, snapshot->before, std::memory_order_relaxed, std::memory_order_relaxed);
    }
    //
  }


  // ----

  // DescriptorEntry

  static const size_t KCASShift = 2;
  const size_t        m_num_threads;
 

private:

          // TWO TYPS OF DESCRIPTORS
  CachePadded<KCASDescriptor>       m_kcas_descs[S_NUM_THREADS];
  CachePadded<RDCSSDescriptor>      m_rdcss_descs[S_NUM_THREADS];

  size_t _thread_id;


  RDCSSDescriptor *rdcss_descs() {
    return &m_rdcss_descs[_thread_id];
  }
  
  TaggedPointer rdcss_ptr_from(size_t new_sequence) {
    return TaggedPointer::make_rdcss(_thread_id, new_sequence);  // set type flag
  }


  // Brown_KCAS::cas_internal -- PRIVATE called only by methods in this class...  see Brown_KCAS::cas
  //

  bool cas_internal(KCASDescriptor *descriptor_snapshot, const TaggedPointer tagptr, bool help = false) {
    //
    // Kelly: This is the descriptor we're trying to complete.
    // We also have a snapshot of it as an argument.
    KCASDescriptor *original_desc = kcas_descriptor_from(tagptr);

    // We have two windows into the state:
    // The first is the tagptr which has a state embeded within it.
    // The second is the sequence bits inside the descriptor.
    // They need to match and if they do then the state extracted.
    //
    const size_t tagptr_sequence_number = TaggedPointer::get_sequence_number(tagptr);

    KCASDescriptorStatus current_status = original_desc->load_status();

    if ( tagptr_sequence_number != current_status.sequence_number ) {
      // This can only fail if we are helping another descriptor
      // since the owner is the only one who can update the sequence bits.
      // assert(help);
      return false;
    }
    //
    const size_t num_entries = descriptor_snapshot->m_num_entries;
    //
    if (current_status.status == KCASDescriptorStatus::UNDECIDED) {   // UNDECIDED
      //
      RDCSSDescriptor *rdcss_desc = this->rdcss_descs();
      //
      uintptr_t status = KCASDescriptorStatus::SUCCEEDED;
      for ( size_t i = help ? 1 : 0; ((i < num_entries) && (status == KCASDescriptorStatus::SUCCEEDED)); i++ ) {
        //
        FOREVER {
          // Give us a new descriptor.
          rdcss_desc->copy_from_kcas(descriptor_snapshot,tagptr,i);
          // Make it initialised.
          size_t new_sequence = rdcss_desc->increment_sequence();
          //
          TaggedPointer rdcss_ptr = this->rdcss_ptr_from(new_sequence);
          TaggedPointer value = this->rdcss(rdcss_ptr);
          //
          if ( !(TaggedPointer::is_kcas(value)) ) {
            if ( value.raw_bits != rdcss_desc->before.raw_bits ) {
              status = KCASDescriptorStatus::FAILED;
            }
            break;
          } else if ( value.raw_bits != tagptr.raw_bits ) {
            cas_try_snapshot(value,true);
          }
          //
        }

      }  // rof

      // Try change descriptor status.
      KCASDescriptorStatus expected_status = current_status;
      KCASDescriptorStatus original_status = original_desc->load_status(std::memory_order_relaxed);
      //
      if ( (original_status.sequence_number == expected_status.sequence_number) && (original_status.status == KCASDescriptorStatus::UNDECIDED) ) {
        original_desc->status_compare_exchange(expected_status,status,current_status.sequence_number);
      }
    }

    // Phase 2.
    KCASDescriptorStatus new_status = original_desc->load_status(std::memory_order_seq_cst);
    if ( new_status.sequence_number != tagptr_sequence_number ) {
      return false;
    } else {
      //
      bool succeeded = (new_status.status == KCASDescriptorStatus::SUCCEEDED);
      descriptor_snapshot->update_entries(succeeded,tagptr);
      //
    }
    return succeeded;
  }

public:

  // CONSTRUCTOR

  Brown_KCAS(const size_t threads) : m_num_threads(threads) {
    for ( size_t i = 0; i < m_num_threads; i++ ) {
      m_kcas_descs[i].m_status.store(KCASDescriptorStatus{});
      m_kcas_descs[i].m_num_entries = 0;
    }
  }

  ~Brown_KCAS() {}

  KCASDescriptor *create_descriptor(const size_t thread_id,const size_t descriptor_size) {
    // Increment the status sequence number.
    m_kcas_descs[thread_id].increment_sequence();
    m_kcas_descs[thread_id].m_num_entries = 0;
    return &m_kcas_descs[thread_id];
  }

  // CAS
  // Brown_KCAS::cas
  //
  bool cas(const size_t thread_id, KCASDescriptor *desc) {
    desc->sort_kcas_entries();
    desc->increment_sequence();  // Init descriptor...
    TaggedPointer ptr = TaggedPointer::make_kcas(thread_id,
                                            (desc->load_status(std::memory_order_relaxed).sequence_number));
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
        return KCASEntry<ValType>::from_raw_bits(tp_desc.raw_bits >> KCASShift);
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

};

}
