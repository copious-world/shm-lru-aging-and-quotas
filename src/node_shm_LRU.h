#ifndef _H_HOPSCOTCH_HASH_LRU_
#define _H_HOPSCOTCH_HASH_LRU_

// node_shm_LRU.h

#include "errno.h"

#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

#include <iostream>
#include <sstream>


#include <map>
#include <unordered_map>
#include <list>
#include <chrono>
#include <atomic>


using namespace node;
using namespace v8;
using namespace std;


#include "hmap_interface.h"
#include "holey_buffer.h"
#include "atomic_proc_rw_state.h"


using namespace std::chrono;


#define MAX_BUCKET_FLUSH 12


#define ONE_HOUR 	(60*60*1000);


/**
 * The 64 bit key stores a 32bit hash (xxhash or other) in the lower word.
 * The top 32 bits stores a structured bit array. The top 1 bit will be
 * the selector of the hash region where the key match will be found. The
 * bottom 20 bits will store the element offset (an element number) to the position the
 * element data is stored. 
*/



/*
auto ms_since_epoch(std::int64_t m){
  return std::chrono::system_clock::from_time_t(time_t{0})+std::chrono::milliseconds(m);
}

uint64_t timeSinceEpochMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()
).count();


int main()
{
    using namespace std::chrono;
 
    uint64_t ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::cout << ms << " milliseconds since the Epoch\n";
 
    uint64_t sec = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    std::cout << sec << " seconds since the Epoch\n";
 
    return 0;
}


milliseconds ms = duration_cast< milliseconds >(
    system_clock::now().time_since_epoch()
);

*/



inline uint64_t epoch_ms(void) {
	uint64_t ms;
	ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
	return ms;
}


template<typename T>
inline string joiner(list<T> &jlist) {
	if ( jlist.size() == 0 ) {
		return("");
	}
	stringstream ss;
	for ( auto v : jlist ) {
		ss << v;
		ss << ',';
	}
	string out = ss.str();
	return(out.substr(0,out.size()-1));
}

template<typename K,typename V>
inline string map_maker_destruct(map<K,V> &jmap) {
	if ( jmap.size() == 0 ) {
		return "{}";
	}
	stringstream ss;
	char del = 0;
	ss << "{";
	for ( auto p : jmap ) {
		if ( del ) { ss << del; }
		del = ',';
		K h = p.first;
		V v = p.second;
		ss << "\""  << h << "\" : \""  << v << "\"";
		delete p.second;
	}
	ss << "}";
	string out = ss.str();
	return(out.substr(0,out.size()));
}


template<typename K,typename V>
inline void js_map_maker_destruct(map<K,V> &jmap,Local<Object> &jsObject) {
	if ( jmap.size() > 0 ) {
		for ( auto p : jmap ) {
			stringstream ss;
			ss << p.first;
			string key = ss.str();
			//
			Local<String> propName = Nan::New(key).ToLocalChecked();
			Local<String> propValue = Nan::New(p.second).ToLocalChecked();
			//
			Nan::Set(jsObject, propName, propValue);
			delete p.second;
		}
		jmap.clear();
	}
}


typedef struct LRU_ELEMENT_HDR {
	uint32_t	_prev;
	uint32_t	_next;
	uint64_t 	_hash;
	time_t		_when;
	uint32_t	_share_key;
} LRU_element;



const uint32_t MAX_MESSAGE_SIZE = 128;
//
const uint32_t OFFSET_TO_MARKER = 0;					// in bytes
const uint32_t OFFSET_TO_OFFSET = sizeof(uint32_t);		// 
const uint32_t OFFSET_TO_HASH = (OFFSET_TO_OFFSET + sizeof(uint32_t));   // start of 64bits

const uint32_t TOTAL_ATOMIC_OFFSET = (OFFSET_TO_HASH + sizeof(uint64_t));	
//
const uint32_t DEFAULT_MICRO_TIMEOUT = 2; // 2 seconds



//
//	LRU_cache --
//
//	Interleaved free memory is a stack -- fixed sized elements
//

class LRU_cache {
	//
	public:
		// LRU_cache -- constructor
		LRU_cache(void *region,size_t record_size,size_t region_size,bool am_initializer,uint16_t proc_max) {
			//
			_SUPER_HEADER = 0;
			_NTiers = 0;
			_INTER_PROC_DESCRIPTOR_WORDS = 0;		// initialized by exposed method called by coniguration.
			//
			_reason = "OK";
			_region = (uint8_t *)region;
			_record_size = record_size;
			_status = true;
			_step = (sizeof(LRU_element) + record_size);
			_hmap_i[0] = nullptr;  //
			_hmap_i[1] = nullptr;  //
			if ( (4*_step) >= region_size ) {
				_reason = "(constructor): regions is too small";
				_status = false;
			} else {
				_region_size = region_size;
				_max_count = (region_size/_step) - 3;
				_count_free = 0;
				_count = 0; 
				//
				if ( am_initializer ) {
					setup_region(record_size);
				} else {
					_count_free = this->_walk_free_list();
					_count = this->_walk_allocated_list(1);
				}
			}

			lb_time->store(UINT32_MAX);
			ub_time->store(UINT32_MAX);

			pair<uint32_t,uint32_t> *primary_storage = (pair<uint32_t,uint32_t> *)(_region + _region_size);
			pair<uint32_t,uint32_t> *shared_queue = primary_storage + _max_count;

			_timeout_table = new KeyValueManager(primary_storage, _max_count, shared_queue, proc_max);
			_configured_tier_cooling = ONE_HOUR;
			_configured_shrinkage = 0.3333;
		}


		/**
		 * called by exposed method 
		 * must be called for initialization
		*/

		void initialize_header_sizes(uint32_t super_header_size,uint32_t N_tiers,uint32_t words_for_mutex_and_conditions) {
			_SUPER_HEADER = super_header_size;
			_NTiers = N_tiers;
			_INTER_PROC_DESCRIPTOR_WORDS = words_for_mutex_and_conditions;		// initialized by exposed method called by coniguration.
			_beyond_entries_for_tiers_and_mutex = (_SUPER_HEADER*_NTiers);
		}


		void set_configured_tier_cooling_time(uint32_t delay = ONE_HOUR) {
			_configured_tier_cooling = delay;
		}

		void set_configured_shrinkage(double fractional) {
			if ( fractional > 0 && fractional < 0.5 ) {
				_configured_shrinkage = fractional;
			}
		}


		// setup_region -- part of initialization if the process is the intiator..
		void setup_region(size_t record_size) {
			
			uint8_t *start = _region;
			size_t step = _step;

			LRU_element *ctrl_hdr = (LRU_element *)start;
			ctrl_hdr->_prev = UINT32_MAX;
			ctrl_hdr->_next = step;
			ctrl_hdr->_hash = 0;
			ctrl_hdr->_when = 0;
			
			LRU_element *ctrl_tail = (LRU_element *)(start + step);
			ctrl_tail->_prev = 0;
			ctrl_tail->_next = UINT32_MAX;
			ctrl_tail->_hash = 0;
			ctrl_tail->_when = 0;

			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);
			ctrl_free->_prev = UINT32_MAX;
			ctrl_free->_next = 3*step;
			ctrl_free->_hash = 0;
			ctrl_free->_when = 0;

			size_t region_size = this->_region_size;
			//
			size_t curr = ctrl_free->_next;
			size_t next = 4*step;
			
			while ( curr < region_size ) {   // all the ends are in the first three elements ... the rest is either free or part of the LRU
				_count_free++;
				LRU_element *next_free = (LRU_element *)(start + curr);
				next_free->_prev = UINT32_MAX;  // singly linked free list
				next_free->_next = next;
				if ( next >= region_size ) {
					next_free->_next = UINT32_MAX;
				}
				next_free->_hash = UINT64_MAX;
				next_free->_when = 0;
				//
				curr += step;
				next += step;
			}

			ctrl_free->_hash = _count_free;   // how many free elements avaibale
			ctrl_hdr->_hash = 0;

		}

		// set_hash_impl - called by initHopScotch -- set two paritions servicing a random selection.
		//
		void set_hash_impl(HMap_interface *hmap1,HMap_interface *hmap2) {
			_hmap_i[0] = hmap1;
			_hmap_i[1] = hmap2;
		}

		// add_el
		// 		data -- data that will be stored at the end of the free list
		//		hash64 -- a large hash of the data. The hash will be processed for use in the hash table.
		//	The hash table will provide reverse lookup for the new element being added by allocating from the free list.
		//	The hash table stores the index of the element in the managed memory. 
		// 	So, to retrieve the element later, the hash will fetch the offset of the element in managed memory
		//	and then the element will be retrieved from its loction.
		//	The number of elements in the free list may be the same or much less than the number of hash table elements,
		//	the hash table can be kept sparse as a result, keeping its operation fairly efficient.
		//	-- The add_el method can tell that it has no more room for elements by looking at the free list.
		//	-- When the free list is empty, add_el returns UINT32_MAX. When the free list is empty,
		//	-- some applications may want to extend share memory by adding more hash slabs or by enlisting secondary processors, 
		//	-- or by running evictions on the tail of the list of allocated elements.
		//	Given there is a free slot for the element, add_el puts the elements offset into the hash table.
		//	add_el moves the element from the free list to the list of allocated positions. These are doubly linked lists.
		//	Each element of the list as a time of insertion, which add_el records.
		//

		uint32_t add_el(char *data,uint64_t hash64) {
			uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
			uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
			return add_el(data, full_hash, hash_bucket);
		}


		uint32_t add_el(char *data,uint32_t full_hash,uint32_t hash_bucket) {
			//
			uint8_t *start = _region;
			size_t step = _step;

			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);
			if (  ctrl_free->_next == UINT32_MAX ) {
				_status = false;
				_reason = "out of free memory: free count == 0";
				return(UINT32_MAX);
			}
			//
			uint32_t new_el_offset = ctrl_free->_next;
			// in rare cases the hash table may be frozen even if the LRU is not full
			//
			uint64_t store_stat = this->store_in_hash(full_hash,hash_bucket,new_el_offset);  // STORE
			//
			if ( store_stat == UINT64_MAX ) {
				return(UINT32_MAX);
			}

			LRU_element *new_el = (LRU_element *)(start + new_el_offset);
			ctrl_free->_next = new_el->_next;
        	//
			LRU_element *header = (LRU_element *)(start);
			LRU_element *first = (LRU_element *)(start + header->_next);
			new_el->_next = header->_next;
			first->_prev = new_el_offset;
			new_el->_prev = 0; // offset to header
			header->_next = new_el_offset;
			//
			new_el->_when = epoch_ms();
			uint64_t hash64 = (((uint64_t)full_hash << HALF) | (uint64_t)hash_bucket);	
			new_el->_hash = hash64;
			char *store_el = (char *)(new_el + 1);
			//
			memset(store_el,0,this->_record_size);
			size_t cpsz = min(this->_record_size,strlen(data));
			memcpy(store_el,data,cpsz);
			//
			_count++;
			if ( _count_free > 0 ) _count_free--;
			return(new_el_offset);
	    }


		// get_el
		uint8_t get_el(uint32_t offset,char *buffer) {
			if ( !this->check_offset(offset) ) return(2);
			//
			uint8_t *start = _region;
			LRU_element *stored = (LRU_element *)(start + offset);
			char *store_data = (char *)(stored + 1);
			memcpy(buffer,store_data,this->_record_size);
			if ( this->touch(stored,offset) ) return(0);
			return(1);
		}

		// get_el_untouched
		uint8_t get_el_untouched(uint32_t offset,char *buffer) {
			if ( !this->check_offset(offset) ) return(2);
			//
			uint8_t *start = _region;
			LRU_element *stored = (LRU_element *)(start + offset);
			char *store_data = (char *)(stored + 1);
			memcpy(buffer,store_data,this->_record_size);
			return(0);
		}

		// update_el
		bool update_el(uint32_t offset,char *buffer) {
			if ( !this->check_offset(offset) ) return(false);
			//
			uint8_t *start = _region;
			LRU_element *stored = (LRU_element *)(start + offset);
			if ( this->touch(stored,offset) ) {
				char *store_data = (char *)(stored + 1);
				memcpy(store_data,buffer,this->_record_size);
				return(true);
			}
			_reason = "deleted";
			return(false);
		}

		//
		// del_el
		bool del_el(uint32_t offset) {
			if ( !this->check_offset(offset) ) return(false);
			uint8_t *start = _region;
			//
			LRU_element *stored = (LRU_element *)(start + offset);
			//
			uint32_t prev_off = stored->_prev;
			uint64_t hash = stored->_hash;
	//cout << "del_el: " << offset << " hash " << hash << " prev_off: " << prev_off << endl;
			//
			if ( (prev_off == UINT32_MAX) && (hash == UINT32_MAX) ) {
				_reason = "already deleted";
				return(false);
			}
			uint32_t next_off = stored->_next;
	//cout << "del_el: " << offset << " next_off: " << next_off << endl;

			LRU_element *prev = (LRU_element *)(start + prev_off);
			LRU_element *next = (LRU_element *)(start + next_off);
			//
	//cout << "del_el: " << offset << " prev->_next: " << prev->_next  << " next->_prev " << next->_prev  << endl;
			prev->_next = next_off;
			next->_prev = prev_off;
			//
			stored->_hash = UINT64_MAX;
			stored->_prev = UINT32_MAX;
			//
			LRU_element *ctrl_free = (LRU_element *)(start + 2*(this->_step));
			stored->_next = ctrl_free->_next;
	//cout << "del_el: " << offset << " ctrl_free->_next: " << ctrl_free->_next << endl;
			ctrl_free->_next = offset;
			//
			this->remove_key(hash);
			_count_free++;
			//
			return(true);
		}


		// HASH TABLE USAGE
		void remove_key(uint64_t hash) {
			//
			uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
			HHash *T = _hmap_i[selector];

			if ( T == nullptr ) {   // no call to set_hash_impl
				_local_hash_table.erase(hash);
			} else {
				T->del(hash);
			}
		}

		void clear_hash_table(void) {
			uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
			HHash *T = _hmap_i[selector];
			if ( T == nullptr ) {   // no call to set_hash_impl
				_local_hash_table.clear();
			} else {
				T->clear();
			}

		}

		uint64_t store_in_hash(uint32_t full_hash,uint32_t hash_bucket,uint32_t new_el_offset) {
			uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
			HHash *T = _hmap_i[selector];
			if ( T == nullptr ) {   // no call to set_hash_impl
				uint64_t key64 = (((uint64_t)full_hash << HALF) | (uint64_t)hash_bucket);				
				_local_hash_table[key64] = new_el_offset;
			} else {
				uint64_t result = T->store(hash_bucket,full_hash,new_el_offset); //UINT64_MAX
				//count << "store_in_hash: " << result << endl;
				return result;
			}
			return 0;
		}


		uint64_t store_in_hash(uint64_t hash64,uint32_t new_el_offset) {
			uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
			uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
			return store_in_hash(full_hash,hash_bucket,new_el_offset);
		}


		// check_for_hash
		// either returns an offset to the data or returns the UINT32_MAX.  (4,294,967,295)
		uint32_t check_for_hash(uint64_t key) {
			uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
			HHash *T = _hmap_i[selector];
			if ( T == nullptr ) {   // no call to set_hash_impl -- means the table was not initialized to use shared memory
				if ( _local_hash_table.find(key) != _local_hash_table.end() ) {
					return(_local_hash_table[key]);
				}
			} else {
				uint32_t result = T->get(key);
				if ( result != 0 ) return(result);
			}
			return(UINT32_MAX);
		}

		// check_for_hash
		// either returns an offset to the data or returns the UINT32_MAX.  (4,294,967,295)
		uint32_t check_for_hash(uint32_t key,uint32_t bucket) {    // full_hash,hash_bucket  :: key == full_hash
			uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
			HHash *T = _hmap_i[selector];
			if ( TierAndProcManager == nullptr ) {   // no call to set_hash_impl -- means the table was not initialized to use shared memory
					uint64_t hash64 = (((uint64_t)key << HALF) | (uint64_t)bucket);
				if ( _local_hash_table.find(hash64) != _local_hash_table.end() ) {
					return(_local_hash_table[hash64]);
				}
			} else {
				uint32_t result = T->get(key,bucket);
				if ( result != 0 ) return(result);
			}
			return(UINT32_MAX);
		}

		// evict_least_used
		void evict_least_used(time_t cutoff,uint8_t max_evict,list<uint64_t> &ev_list) {
			uint8_t *start = _region;
			size_t step = _step;
			LRU_element *ctrl_tail = (LRU_element *)(start + step);
			time_t test_time = 0;
			uint8_t ev_count = 0;
			do {
				uint32_t prev_off = ctrl_tail->_prev;
				if ( prev_off == 0 ) break; // no elements left... step?? instead of 0
				ev_count++;
				LRU_element *stored = (LRU_element *)(start + prev_off);
				uint64_t hash = stored->_hash;
				ev_list.push_back(hash);
				test_time = stored->_when;
				this->del_el(prev_off);
			} while ( (test_time < cutoff) && (ev_count < max_evict) );
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		void evict_least_used_to_value_map(time_t cutoff,uint8_t max_evict,map<uint64_t,char *> &ev_map) {
			uint8_t *start = _region;
			size_t step = _step;
			LRU_element *ctrl_tail = (LRU_element *)(start + step);
			time_t test_time = 0;
			uint8_t ev_count = 0;
			uint64_t last_hash = 0;
			do {
				uint32_t prev_off = ctrl_tail->_prev;
				if ( prev_off == 0 ) break; // no elements left... step?? instead of 0
				ev_count++;
				LRU_element *stored = (LRU_element *)(start + prev_off);
				uint64_t hash = stored->_hash;
				test_time = stored->_when;
				char *buffer = new char[this->record_size()];
				this->get_el_untouched(prev_off,buffer);
				this->del_el(prev_off);
				ev_map[hash] = buffer;
			} while ( (test_time < cutoff) && (ev_count < max_evict) );
		}

		uint8_t evict_least_used_near_hash(uint32_t hash,time_t cutoff,uint8_t max_evict,map<uint64_t,char *> &ev_map) {
			//
			if ( _hmap_i == nullptr ) {   // no call to set_hash_impl
				return 0;
			} else {
				uint32_t xs[32];
				uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
				HHash *T = _hmap_i[selector];

				uint8_t count = T->get_bucket(hash, xs);
				//
				uint8_t *start = _region;
				size_t step = _step;
				LRU_element *ctrl_tail = (LRU_element *)(start + step);
				time_t test_time = 0;
				uint8_t ev_count = 0;
				uint64_t last_hash = 0;
				//
				uint8_t result = 0;
				if ( count > 0 ) {
					for ( uint8_t i = 0; i < count; i++ ) {
						uint32_t el_offset = xs[i];
						//
						LRU_element *stored = (LRU_element *)(start + el_offset);
						uint64_t hash = stored->_hash;
						test_time = stored->_when;
						if ( ((test_time < cutoff) || (ev_count < max_evict)) && (result < MAX_BUCKET_FLUSH) ) {
							char *buffer = new char[this->record_size()];
							this->get_el_untouched(el_offset,buffer);
							this->del_el(el_offset);
							ev_map[hash] = buffer;
							result++;
						}
					}
				}
				return result;
			}

		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		size_t _walk_allocated_list(uint8_t call_mapper,bool backwards = false) {
			if ( backwards ) {
				return(_walk_allocated_list_backwards(call_mapper));
			} else {
				return(_walk_allocated_list_forwards(call_mapper));
			}
		}

		size_t _walk_allocated_list_forwards(uint8_t call_mapper) {
			uint8_t *start = _region;
			LRU_element *header = (LRU_element *)(start);
			size_t count = 0;
			uint32_t next_off = header->_next;
			LRU_element *next = (LRU_element *)(start + next_off);
			if ( call_mapper > 2 )  _pre_dump();
			while ( next->_next != UINT32_MAX ) {   // this should be the tail
				count++;
				if ( count >= _max_count ) break;
				if ( call_mapper > 0 ) {
					if ( call_mapper == 1 ) {
						this->_add_map(next,next_off);
					} else if ( call_mapper == 2 ) {
						this->_add_map_filtered(next,next_off);
					} else {
						_console_dump(next);
					}
				}
				next_off =  next->_next;
				next = (LRU_element *)(start + next_off);
			}
			if ( call_mapper > 2 )  _post_dump();
			return(count - this->_count);
		}

		size_t _walk_allocated_list_backwards(uint8_t call_mapper) {
			uint8_t *start = _region;
			uint32_t step = _step;
			LRU_element *tail = (LRU_element *)(start + step);  // tail is one elemet after head
			size_t count = 0;
			uint32_t prev_off = tail->_prev;
			LRU_element *prev = (LRU_element *)(start + prev_off);
			if ( call_mapper > 2 )  _pre_dump();
			while ( prev->_prev != UINT32_MAX ) {   // this should be the tail
				count++;
				if ( count >= _max_count ) break;
				if ( call_mapper > 0 ) {
					if ( call_mapper == 1 ) {
						this->_add_map(prev,prev_off);
					} else if ( call_mapper == 2 ) {
						this->_add_map_filtered(prev,prev_off);
					} else {
						_console_dump(prev);
					}
				}
				prev_off =  prev->_prev;
				prev = (LRU_element *)(start + prev_off);
			}
			if ( call_mapper > 2 )  _post_dump();
			return(count - this->_count);
		}

		size_t _walk_free_list(void) {
			uint8_t *start = _region;
			LRU_element *ctrl_free = (LRU_element *)(start + 2*(this->_step));
			size_t count = 0;
			LRU_element *next_free = (LRU_element *)(start + ctrl_free->_next);
			while ( next_free->_next != UINT32_MAX ) {
				count++;
				if ( count >= _max_count ) break;
				next_free = (LRU_element *)(start + next_free->_next);
			}
			return(count - this->_count_free);
		}
		
		bool ok(void) {
			return(this->_status);
		}

		size_t record_size(void) {
			return(_record_size);
		}

		const char *get_last_reason(void) {
			const char *res = _reason;
			_reason = "OK";
			return(res);
		}

		void reload_hash_map(void) {
			this->clear_hash_table();
			_count = this->_walk_allocated_list(1);
		}

		void reload_hash_map_update(uint32_t share_key) {
			_share_key = share_key;
			_count = this->_walk_allocated_list(2);
		}

		bool set_share_key(uint32_t offset,uint32_t share_key) {
			if ( !this->check_offset(offset) ) return(false);
			uint8_t *start = _region;
			//
			LRU_element *stored = (LRU_element *)(start + offset);
			_share_key = share_key;
			stored->_share_key = share_key;
			return(true);
		}

		uint16_t max_count(void) {
			return(_max_count);
		}

		uint16_t current_count(void) {
			return(_count);
		}

		uint16_t free_count(void) {
			return(_count_free);
		}

	public:

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		// 		start of the new atomic code for memory stack...

		// LRU Methods


		/*
		Free memory is a stack with atomic var protection.
		If a basket of new entries is available, then claim a basket's worth of free nodes and return then as a 
		doubly linked list for later entry. Later, add at most two elements to the LIFO queue the LRU.
		*/
		// LRU_cache method
		//
		//
		uint32_t		free_mem_requested(void) {
			//
			uint8_t *start = _region;
			size_t step = _step;
			//
			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
			//
			auto requested = static_cast<atomic<uint32_t>*>(&(ctrl_free->_hash));
			uint32_t total_requested = requested->load(std::memory_order_relaxed);

			return total_requested;
		}


		// LRU_cache method
		void free_mem_claim_satisfied(uint32_t msg_count) {   // stop requesting the memory... 
			uint8_t *start = _region;
			size_t step = _step;
			//
			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
			auto requested = static_cast<atomic<uint32_t>*>(&(ctrl_free->_hash));
			if ( requested->load() <= msg_count ) {
				requested->store(0);
			} else {
				requested->fetch_sub(msg_count);
			}
		}


		// LRU_cache method
		void return_to_free_mem(LRU_element *el) {				// a versions of push
			uint8_t *start = _region;
			size_t step = _step;
			//
			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
			auto head = static_cast<atomic<uint32_t>*>(&(ctrl_free->_next));
			auto count_free = static_cast<atomic<uint32_t>*>(&(ctrl_free->_prev));
			//
			uint32_t el_offset = (uint32_t)(el - start);
			uint32_t h_offset = head->load(std::memory_order_relaxed);
			while(!head->compare_exchange_weak(h_offset, el_offset));
			count_free->fetch_add(1, std::memory_order_relaxed);
		}


		// LRU_cache method
		uint32_t claim_free_mem(uint32_t ready_msg_count,uint32_t *reserved_offsets) {
			LRU_element *first = NULL;
			//
			uint8_t *start = _region;
			size_t step = _step;
			//
			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
			//
			auto head = static_cast<atomic<uint32_t>*>(&(ctrl_free->_prev));
			uint32_t h_offset = head->load(std::memory_order_relaxed);
			if ( h_offset == UINT32_MAX ) {
				_status = false;
				_reason = "out of free memory: free count == 0";
				return(UINT32_MAX);
			}
			auto count_free = static_cast<atomic<uint32_t>*>(&(ctrl_free->_prev));
			//
			// POP as many as needed
			//
			uint32_t n = ready_msg_count;
			while ( n-- ) {  // consistently pop the free stack
				uint32_t next_offset = UINT32_MAX;
				uint32_t first_offset = UINT32_MAX;
				do {
					if ( h_offset == UINT32_MAX ) {
						_status = false;
						_reason = "out of free memory: free count == 0";
						return(UINT32_MAX);			/// failed memory allocation...
					}
					first_offset = h_offset;
					first = (LRU_element *)(start + first_offset);
					next_offset = first->_next;
				} while( !(head->compare_exchange_weak(h_offset, next_offset)) );  // link ctrl->next to new first
				//
				if ( next_offset < UINT32_MAX ) {
					reserved_offsets[n] = first_offset;  // h_offset should have changed
				}
			}
			//
			count_free->fetch_sub(ready_msg_count, std::memory_order_relaxed);
			free_mem_claim_satisfied(ready_msg_count);
			//
			return 0;
		}



		// LRU_cache method
		void done_with_tail(LRU_element *ctrl_tail,atomic<uint32_t> *flag_pos,bool set_high = false) {
			if ( set_high ) {
				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,0)
							&& (ctrl_tail->_share_key == UINT32_MAX));   // if some others have gone through OK
			} else {
				auto prev_val = flag_pos->load();
				if ( prev_val == 0 ) return;  // don't go below zero
				flag_pos->fetch_sub(ctrl_tail->_share_key,1);
			}
		}



		// LRU_cache method
		atomic<uint32_t> *wait_on_tail(LRU_element *ctrl_tail,bool set_high = false,uint32_t delay = 4) {
			//
			auto flag_pos = static_cast<atomic<uint32_t>*>(&(ctrl_tail->_share_key));
			if ( set_high ) {
				uint32_t check = UINT32_MAX;
				while ( check !== 0 ) {
					check = flag_pos->load(std::memory_order_relaxed);
					if ( check != 0 )  {			// waiting until everyone is done with it
						usleep(delay);
					}
				}
				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,UINT32_MAX)
							&& (ctrl_tail->_share_key) !== UINT32_MAX));
			} else {
				uint32_t check = UINT32_MAX;
				while ( check == UINT32_MAX ) {
					check = flag_pos->load(std::memory_order_relaxed);
					if ( check == UINT32_MAX )  {
						usleep(delay);
					}
				}
				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,(check+1))
							&& (ctrl_tail->_share_key < UINT32_MAX) );
			}
			return flag_pos;
		}


		/*
			Exclude tail pop op during insert, where insert can only be called 
			if there is enough room in the memory section. Otherwise, one evictor will get busy evicting. 
			As such, the tail op exclusion may not be necessary, but desirable.  Really, though, the tail operation 
			necessity will be discovered by one or more threads at the same time. Hence, the tail op exclusivity 
			will sequence evictors which may possibly share work.

			The problem to be addressed is that the tail might start removing elements while an insert is attempting to attach 
			to them. Of course, the eviction has to be happening at the same time elements are being added. 
			And, eviction is supposed to happen when memory runs out so the inserters are waiting for memory and each thread having 
			encountered the need for eviction has called for it and should simply be higher on the stack waiting for the 
			eviction to complete. After that they get free memory independently.
		*/


		/**
		 * Prior to attachment, the required space availability must be checked.
		*/
		void attach_to_lru_list(uint32_t *lru_element_offsets,uint32_t ready_msg_count) {
			//
			uint32_t last = lru_element_offsets[(ready_msg_count - 1)];  // freed and assigned to hashes...
			uint32_t first = lru_element_offsets[0];  // freed and assigned to hashes...
			//
			uint8_t *start = _region;
			size_t step = _step;
			//
			LRU_element *ctrl_hdr = (LRU_element *)start;
			//
			LRU_element *ctrl_tail = (LRU_element *)(start + step);
			auto tail_block = wait_on_tail(ctrl_tail,false); // WAIT :: stops if the tail hash is set high ... this call does not set it

			auto head = static_cast<atomic<uint32_t>*>(&(ctrl_hdr->_next));
			//
			uint32_t next = head->exchange(first);  // ensure that some next (another basket first perhaps) is available for buidling the LRU
			//
			LRU_element *old_first = (LRU_element *)(start + next);  // this has been settled
			//
			old_first->_prev = last;			// ---- ---- ---- ---- ---- ---- ---- ----
			//
			// thread the list
			for ( uint32_t i = 0; i < ready_msg_count; i++ ) {
				LRU_element *current_el = (LRU_element *)(start + lru_element_offsets[i]);
				current_el->_prev = ( i > 0 ) ? lru_element_offsets[i-1] : 0;
				current_el->_next = ((i+1) < ready_msg_count) ? lru_element_offsets[i+1] : next;
			}
			//

			done_with_tail(ctrl_tail,tail_block,false);
		}



		// LRU_cache method
		inline pair<uint32_t,uint32_t> lru_remove_last() {
			//
			uint8_t *start = _region;
			size_t step = _step;
			//
			LRU_element *ctrl_tail = (LRU_element *)(start + step);
			auto tail_block = wait_on_tail(ctrl_tail,true);   // WAIT :: usually this will happen only when memory becomes full
			//
			auto tail = static_cast<atomic<uint32_t>*>(&(ctrl_tail->_prev));
			uint32_t t_offset = tail->load();
			//
			LRU_element *leaving = (LRU_element *)(start + t_offset);
			LRU_element *staying = (LRU_element *)(start + leaving->_prev);
			//
			uint64_t hash = leaving->_hash;
			//
			staying->_next = step;  // this doesn't change
			ctrl_tail->_prev = leaving->_prev;
			//
			done_with_tail(ctrl_tail,tail_block,true);
			//
			pair<uint32_t,uint64_t> p(t_offset,hash);
			return p;
		}







		// HH_map method - calls get -- 
		//
		uint32_t 	partition_get(uint64_t key) {
			uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
			HHash *T = _hmap_i[selector];
			return get_hh_map(T, key);
		}



		// LRU_cache method - calls get -- 
		/**
		 * filter_existence_check
		 * 
		 * Both arrays, messages and accesses, contain references to hash words.. 
		 * These are the hash parameters left by the process requesting storage.
		 * 
		*/
		uint32_t		filter_existence_check(char **messages,char **accesses,uint32_t ready_msg_count) {
			uint32_t new_count = 0;
			while ( --ready_msg_count >= 0 ) {
				uint64_t *hash_loc = (uint64_t *)(messages[ready_msg_count] + OFFSET_TO_HASH);
				uint64_t hash = hash_loc[0];
				uint32_t data_loc = this->partition_get(hash);
				if ( data_loc != 0 ) {    // check if this message is already stored
					messages[ready_msg_count] = data_loc;
				} else {
					new_count++;
					accesses[ready_msg_count] = NULL;
				}
			}
			return new_count;
		}


	

		// LRU_cache method
		bool check_free_mem(uint32_t msg_count,bool add) {
			//
			uint8_t *start = _region;
			size_t step = _step;
			//
			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
			// using _prev to count the free elements... it is not updated in this check (just inspected)
			_count_free = ctrl_free->_prev;  // using the hash field as the counter  (prev usually indicates the end, but not its a stack)
			//
			auto requested = static_cast<atomic<uint32_t>*>(&(ctrl_free->_hash));
			uint32_t total_requested = requested->load(std::memory_order_relaxed);  // could start as zero
			if ( add ) {
				total_requested += msg_count;			// update public info about the amount requested 
				requested->fetch_add(msg_count);		// should be the amount of all curren requests
			}
			if ( _count_free < total_requested ) return false;
			return true;
		}


		// _timeout_table



		uint32_t timeout_table_evictions(list<uint32_t> &moving,uint32_t req_count) {
			//
			const auto now = system_clock::now();
    		const time_t t_c = system_clock::to_time_t(now);
			uint32_t min_max_time = (t_c - _configured_tier_cooling);
			uint32_t as_many_as = min((_max_count*_configured_shrinkage),(req_count*3));
			//
			_timeout_table->displace_lowest_value_threshold(moving,min_max_time,as_many_as);
			return moving.size();
		}



	private:

		bool touch(LRU_element *stored,uint32_t offset) {
			uint8_t *start = _region;
			//
			uint32_t prev_off = stored->_prev;
			if ( prev_off == UINT32_MAX ) return(false);
			uint32_t next_off = stored->_next;

			//
			LRU_element *prev = (LRU_element *)(start + prev_off);
			LRU_element *next = (LRU_element *)(start + next_off);
			//
			// out of list
			prev->_next = next_off; // relink
			next->_prev = prev_off;
			//
			LRU_element *header = (LRU_element *)(start);
			LRU_element *first = (LRU_element *)(start + header->_next);
			stored->_next = header->_next;
			first->_prev = offset;
			header->_next = offset;
			stored->_prev = 0;
			stored->_when = epoch_ms();
			//
			return(true);
		}

		bool check_offset(uint32_t offset) {
			_reason = "OK";
			if ( offset > this->_region_size ) {
				//cout << "check_offset: " << offset << " rsize: " << this->_region_size << endl;
				_reason = "OUT OF BOUNDS";
				return(false);
			} else {
				uint32_t step = _step;
				if ( (offset % step) != 0 ) {
					_reason = "ELEMENT BOUNDAY VIOLATION";
					return(false);
				}
			}
			return(true);
		}

		void _add_map(LRU_element *el,uint32_t offset) {
			if ( _hmap_i == nullptr ) {   // no call to set_hash_impl
				uint64_t hash = el->_hash;
				this->store_in_hash(hash,offset);
			}
		}

		void _add_map_filtered(LRU_element *el,uint32_t offset) {
			if ( _hmap_i == nullptr ) {   // no call to set_hash_impl
				if ( _share_key == el->_share_key ) {
					uint64_t hash = el->_hash;
					this->store_in_hash(hash,offset);
				}
			}
		}

		void _console_dump(LRU_element *el) {
			uint8_t *start = _region;
			uint64_t offset = (uint64_t)(el) - (uint64_t)(start);
			cout << "{" << endl;
			cout << "\t\"offset\": " << offset <<  ", \"hash\": " << el->_hash << ',' << endl;
			cout << "\t\"next\": " << el->_next << ", \"prev:\" " << el->_prev << ',' <<endl;
			cout << "\t\"when\": " << el->_when << ',' << endl;

			char *store_data = (char *)(el + 1);
			char buffer[this->_record_size];
			memset(buffer,0,this->_record_size);
			memcpy(buffer,store_data,this->_record_size-1);
			cout << "\t\"value: \"" << '\"' << buffer << '\"' << endl;
			cout << "}," << endl;
		}

		void _pre_dump(void) {
			cout << "[";
		}
		void _post_dump(void) {
			cout << "{\"offset\": -1 }]" << endl;
		}

		bool							_status;
		const char 						*_reason;
		uint8_t		 					*_region;
		size_t		 					_record_size;
		size_t							_region_size;
		uint32_t						_step;
		uint16_t						_count_free;
		uint16_t						_count;
		uint16_t						_max_count;  // max possible number of records
		uint32_t						_share_key;
		//
		atomic<uint32_t>				*lb_time;
		atomic<uint32_t>				*ub_time;
		//
		HMap_interface 					*_hmap_i[2];
		//

		KeyValueManager					*_timeout_table;
		uint32_t						_configured_tier_cooling;
		double							_configured_shrinkage;
		//
		unordered_map<uint64_t,uint32_t>			_local_hash_table;
		//
};










typedef struct TIER_TIME {
	atomic<uint32_t>	*lb_time;
	atomic<uint32_t>	*ub_time;
} Tier_time_bucket, *Tier_time_bucket_ref;




// b_search 
//
static inline uint32_t
time_interval_b_search(uint32_t timestamp, Tier_time_bucket_ref timer_table,uint32_t N) {
	Tier_time_bucket_ref beg = timer_table;
	Tier_time_bucket_ref end = timer_table + N;
	//
	if ( (beg->lb_time->load() <= timestamp )&& (timestamp < beg->ub_time->load()) ) return 0;
	beg++; N--;
	if ( ((end-1)->lb_time->load() <= timestamp )&& (timestamp < (end-1)->ub_time->load()) ) return N-1;
	end--; N--;
	//
	while ( beg < end ) {
		N = N >> 1;
		if ( N == 0 ) {
			while ( beg < end ) {
				if ( (beg->lb_time->load() <= timestamp )&& (timestamp < beg->ub_time->load()) ) return beg;
				beg++;
			}
			break;
		}
		if ( mid >= end ) break;
		Tier_time_bucket_ref mid = beg + N;
		//
		uint32_t mid_lb, mid_ub;
		mid_lb = mid->lb_time->load();
		mid_ub = mid->ub_time->load();
		//
		if ( (mid_lb <= timestamp ) && (timestamp < mid_ub) ) return mid;
		if ( timestamp > mid_ub ) beg = mid;
		else end = mid;
	}
	return UINT32_MAX;
}


template<const uint8_t MAX_TIERS = 8>
class TierAndProcManager {

	public:

		TierAndProcManager(void *region[MAX_TIERS], size_t rc_sz, bool am_initializer, uint8_t num_tiers_in_use, uint32_t SUPER_HEADER, uint32_t INTER_PROC_DESCRIPTOR_WORDS) : _t_times(t_times) {
			_NTiers = num_tiers_in_use;
			for ( int i = 0; i < num_tiers_in_use; i++ ) {
				_tiers[i] = new LRU_cache(region[i], rc_sz, seg_sz, am_initializer);
				_tiers[i]->initialize_header_sizes(SUPER_HEADER,num_tiers_in_use,INTER_PROC_DESCRIPTOR_WORDS);
				_t_times[i].lb_time = _tiers[i]->lb_time;
				_t_times[i].ub_time = _tiers[i]->ub_time;
			}
			_com_buffer = nullptr;
		}

		// -- 
		bool		set_com_buffer(void *com_buffer) {
			if ( com_buffer == nullptr ) return false;
			_com_buffer = com_buffer;
			return true;
		}

		bool 		set_reader_atomic_tags() {
			if ( _com_buffer != nullptr ) {
				atomic_flag *af = (atomic_flag *)_com_buffer;
				for ( int i; i < _NTiers; i++ ) {
					_readerAtomicFlag[i] = atomic_flag;
					atomic_flag++;
				}
				return true;
			}
			return false
		}

		LRU_cache	*access_tier(uint8_t tier) {
			if ( (0 <= tier) && (tier < MAX_TIERS) ) {
				return _tiers[tier];
			}
			return nullptr;
		}

		LRU_cache	*from_time(uint32_t timestamp) {
			uint32_t index = time_interval_b_search(timestamp, _t_times, num_tiers_in_use);
			if ( index < num_tiers_in_use ) {
				return _tiers[index];
			}
			return nullptr;
		}




		// TierAndProcManager
		bool run_evictions(LRU_cache *lru,uint32_t source_tier) {
			//
			// lru - is a source tier
			uint32_t req_count = lru->free_mem_requested();
			if ( req_count == 0 ) return true;	// for some reason this was invoked, but no one actually wanted free memory.

			list<uint32_t> moving;
			uint32_t reclaimed_stamps = lru->timeout_table_evictions(moving,req_count);
			if ( lru->has_reserve() ) {
				LRU_cache *next_tier = this->access_tier(source_tier+1);
				if ( next_tier == nullptr ) {
					// crisis mode...				elements will have to be discarded or sent to another machine
				} else {
					// use a secondary free list for new req_count elements 
					// and at the same time, yield the old position to a second tier that shares this tier's primary.
					list<LRU_element *> free_reserve;
					lru->from_reserve(free_reserve,req_count);
					list<LRU_element *>::iterator *lit = free_reserve.begin();
					for ( ; lit !=  free_reserve.end(); lit++ ) {
						LRU_element *reserve_el = *lit;
						lru->return_to_free_mem(reserve_el);
					}
					next_tier->claim_hashes(moving);
					lru->relinquish_hashes(moving);
					// have to wakeup a secondary process that will move data from reserve to primary
					// and move relinquished data to the secondary... (free up reserve again... need it later)
				}
			} else {
				transfer_to_source_tier(_record_size,moving,reclaimed_stamps,source_tier);   // also copy data...
			}
			//
			return true;
		}

	/**
	 * reader_operation
	 * 
	 * The reader operation is launched from a new thread during initialization.
	 * A number of readers may occur among cores for handling insertion and expulsion of data from a tier.
	 * So, a core may handle one or more tiers, launching a thread for each tier it manages.
	 * 
	 * Each read operation is assigned a tier for which it reads. 
	 * Data shared with the writer is negotiated between cores within areas set aside for each tier.
	 * So, for each tier there will be atomics that operate specifically for the tier in question.
	 * 
	*/

	// At the app level obtain the LRU for the tier and work from there
	//
	int 		reader_operation(uint16_t proc_count, char **messages_reserved, char **duplicate_reserved, uint8_t assigned_tier) {
		//
		if ( _com_buffer == NULL  ) {    // com buffer not intialized
			return -5; // plan error numbers: this error is huge problem cannot operate
		}
		if ( (proc_count > 0) && (assigned_tier < _NTiers) ) {
			//
			LRU_cache *lru = access_tier(assigned_tier);
			if ( lru == NULL ) {
				return(-1);
			}
			//
			char **messages = messages_reserved;  // get this many addrs if possible...
			char **accesses = duplicate_reserved;
			//
			_readerAtomicFlag[assigned_tier]->wait(false);  // this tier's LRU shares this read flag
				// 
			uint32_t ready_msg_count = 0;
			// 		OP on com buff
			// FIRST: gather messages that are aready for addition to shared data structures.
			//
			// go through all the processes that might have written to this tier.
			//
			uint32_t tier_atomics = assigned_tier*TOTAL_ATOMIC_OFFSET;  // same offset for all procs
			//
			for ( uint32_t proc = 0; (proc < proc_count); proc++ ) {
				//
				uint32_t next_proc_table = _INTER_PROC_DESCRIPTOR_WORDS*(proc);   // each proc might be writing to this tier
				uint32_t p_offset = _beyond_entries_for_tiers_and_mutex + next_proc_table + tier_atomics;
				//
				char *access_point = ((char *)_com_buffer) + p_offset;
				atomic<COM_BUFFER_STATE> *read_marker = (atomic<COM_BUFFER_STATE> *)(access_point + OFFSET_TO_MARKER);
				//
				if ( read_marker->load() == CLEARED_FOR_ALLOC ) {
					//
					claim_for_alloc(read_marker); // This is the atomic update of the write state
					messages[ready_msg_count] = access_point;
					accesses[ready_msg_count] = access_point;
					ready_msg_count++;
					//
				}
				//
			}
			// rof; 
			//
			//
			// 		OP on com buff
			// SECOND: If duplicating, free the message slot, otherwise gather memory for storing new objecs
			//
			if ( ready_msg_count > 0 ) {  // a collection of message this process/thread will enque
				// 	-- FILTER - only allocate for new objects
				uint32_t additional_locations = lru->filter_existence_check(messages,accesses,ready_msg_count);
				//
				char **end_dups = accesses + (ready_msg_count - additional_locations);
				char **tmp = messages;
				while ( accesses < end_dups ) {
					char *dup_access = *accesses++;
					if ( dup_access != nullptr ) {
						uint32_t data_loc = (uint32_t)(tmp[0]);
						tmp[0] = nullptr;
						uint32_t *write_offset_here = (access_point + dup_access);
						write_offset_here[0] = data_loc;
						// now get the control word location
						atomic<COM_BUFFER_STATE> *read_marker = (atomic<COM_BUFFER_STATE> *)(dup_access + OFFSET_TO_MARKER);
						clear_for_copy(read_marker);  // tells the requesting process to go ahead and write data.
					}
					tmp++;
				}
				//
				if ( additional_locations > 0 ) {
					//
					// Is there enough memory?
					bool add = true;
					while ( !(lru->check_free_mem(ready_msg_count,add)) ) {
						if ( !run_evictions(lru,assigned_tier) ) {
							return(-1);
						}
						add = false;
					}
					// GET LIST FROM FREE MEMORY 
					//
					uint32_t lru_element_offsets[ready_msg_count+1];  // should be on stack
					memset((void *)lru_element_offsets,0,sizeof(uint32_t)*(additional_locations+1)); // clear the buffer

					// the next thing off the free stack.
					//
					bool mem_claimed = (UINT32_MAX != lru->claim_free_mem(additional_locations,lru_element_offsets)); // negotiate getting a list from free memory
					//
					// if there are elements, they are already removed from free memory and this basket belongs to this process..
					if ( mem_claimed ) {
						//
						uint32_t *current = lru_element_offsets;   // offset to new elemnents in the regions
						uint8_t *start = lru->_region;
						uint32_t offset = 0;
						//
						uint32_t N = ready_msg_count;
						char **tmp = messages;
						char **end_m = messages + N;
						//
						// map hashes to the offsets
						//
						while ( tmp < end_m ) {   // only as many elements as proc placing data into the tier (parameter)
							// read from com buf
							char *access_point = *tmp++;
							if ( access_point != nullptr ) {
								//
								offset = *current++;
								//
								uint32_t *write_offset_here = (access_point + OFFSET_TO_OFFSET);
								uint64_t *hash_parameter = (uint64_t *)(access_point + OFFSET_TO_HASH);
								uint64_t hash64 = hash_parameter[0];
								//
								if ( lru->add_key_value(hash64,offset) ) { // add to the hash table...
									write_offset_here[0] = offset;
									//
									atomic<COM_BUFFER_STATE> *read_marker = (atomic<COM_BUFFER_STATE> *)(access_point + OFFSET_TO_MARKER);
									clear_for_copy(read_marker);  // release the proc, allowing it to emplace the new data
								}		
							}
						}
						//
						lru->attach_to_lru_list(lru_element_offsets,ready_msg_count);  // attach to an LRU as a whole bucket...
					}
				}
			}

			return 0;
		}

		return(-1)
	}




		/**
		 * Waking up any thread that waits on input into the tier.
		 * Any number of processes may place a message into a tier. 
		 * If the tier is full, the reader has the job of kicking off the eviction process.
		*/
		bool wake_up_readers(uint32_t tier) {
			_readerAtomicFlag[tier] = true;
			_readerAtomicFlag[tier]->notify_all();
			return true;
		}


		/**
		 * put_method
		 * 
		 * Initiates the process by which the system find a place to write data. This method waits on the position to write data.
		 * 
		 * This method first waits on access only if its entry has already been breached by itself or a process looking to pick up
		 * hash parameters. Once a process or thread is servicing the search for the next location to return for a previous request.
		 * 
		 * This puts to a tier. Most often, this will be called with tier 0 for a new piece of data.
		 * But, it may be invoked for a list of values being moved to an older tier during tier evictions.
		 * 
		*/



		int 		put_method(uint32_t process,uint32_t hash_bucket,uint32_t full_hash,bool updating,char* buffer,unsigned int size,uint32_t timestamp,uint32_t tier,void (delay_func)()) {
			//
			if ( _com_buffer == nullptr ) return -1;  // has not been initialized
			if ( (buffer == nullptr) || (size <= 0) ) return -1;  // might put a limit on size lower and uppper
			//
			//
			LRU_cache *lru = from_time(timestamp);   // this is being accessed in more than one place...

			if ( lru == nullptr ) {  // has not been initialized
				return -1;
			}
			//
			// offset to the table storing shared terms belonging to 'process'. (Process or thread id)
			uint32_t next_proc_table = _INTER_PROC_DESCRIPTOR_WORDS*(proc);
			uint32_t tier_atomics = tier*TOTAL_ATOMIC_OFFSET;
			//
			uint32_t p_offset = _beyond_entries_for_tiers_and_mutex + next_proc_table + tier_atomics; 

			//
			char *access_point = ((char *)_com_buffer) + p_offset;   // start of shared atomics... (for the given tier)
			// particular atomics
			unsigned char *read_marker =	(access_point + OFFSET_TO_MARKER);		// this is for other processes to wake up...
			uint32_t *hash_parameter =		(uint32_t *)(access_point + OFFSET_TO_HASH);	// parameters for the call
			uint32_t *offset_offset =		(uint32_t *)(access_point + OFFSET_TO_OFFSET);	// the new data offset should be returned here

			//
			// Writing will take place after a place in the LRU has been given to this writer...
			// 
			// WAIT - a reader may be still taking data out of our slot.
			// let it finish before puting in the new stuff.
			if ( wait_to_write(read_marker,delay_func) ) {	// will wait (spin lock style) on an atomic indicating the read state of the process
				// 
				// tell a reader to get some free memory
				hash_parameter[0] = hash_bucket; // put in the hash so that the read can see if this is a duplicate
				hash_parameter[1] = full_hash;
				//
				cleared_for_alloc(read_marker);   // allocators can now claim this process request
				//
				// will sigal just in case this is the first writer done and a thread is out there with nothing to do.
				// wakeup a conditional reader if it happens to be sleeping and mark it for reading, 
				// which prevents this process from writing until the data is consumed
				bool status = wake_up_readers(tier);
				if ( !status ) {
					return -2;
				}
				// the write offset should come back to the process's read maker
				offset_offset[0] = updating ? UINT32_MAX : 0;
				//					
				if ( await_write_offset(read_marker,MAX_WAIT_LOOPS,delay_func) ) {
					uint32_t write_offset = offset_offset[0];
					if ( (write_offset == UINT32_MAX) && !(updating) ) {	// a duplicate has been found
						clear_for_write(read_marker);   // next write from this process can now proceed...
						return -1;
					}
					//
					char *m_insert = lru->data_location(write_offset);
					memcpy(m_insert,buffer,min(size,MAX_MESSAGE_SIZE));  // COPY IN NEW DATA HERE...
					//
					clear_for_write(read_marker);   // next write from this process can now proceed...
				} else {
					clear_for_write(read_marker);   // next write from this process can now proceed...
					return -1;
				}
			} else {
				// something went wrong ... perhaps a frozen reader...
				return -1;
			}
			//
			return 0;
		}


	protected:


		/**
		 * 
		// raise the lower bound on the times allowed into an LRU 
		// this operation does not run evictions. 
		// but processes running evictions may use it.
		//
		// This is using atomics ... not certain that is the future with this...
		//
		// returns: the old lower bound on time. the lower bound may become the new upper bound of an
		// older tier.
		*/

		void raise_lru_lb_time_bounds(uint32_t lb_timestamp) {
			uint32_t index = time_interval_b_search(timestamp, _t_times, num_tiers_in_use);
			if ( index == 0 ) {
				Tier_time_bucket *ttbr = Tier_time_bucket[0];
				ttbr->ub_time->store(UINT32_MAX);
				uint32_t lbt = ttbr->lb_time->load();
				if ( lbt < lb_timestamp ) {
					ttbr->lb_time->store(lb_timestamp);
					return lbt;
				}
				return 0;
			}
			if ( index < num_tiers_in_use ) {
				Tier_time_bucket *ttbr = Tier_time_bucket[index];
				uint32_t lbt = ttbr->lb_time->load();
				if ( lbt < lb_timestamp ) {
					uint32_t ubt = ttbr->ub_time->load();
					uint32_t delta = (lb_timestamp - lbt);
					ttbr->lb_time->store(lb_timestamp);
					ubt -= delta;
					ttbr->ub_time->store(lb_timestamp);					
					return lbt;
				}
				return 0;
			}
		}

	protected:

		void					*_com_buffer;
		LRU_cache 				*_tiers[MAX_TIERS];		// local process storage
		Tier_time_bucket		_t_times[MAX_TIERS];	// shared mem storage


	protected:


		uint32_t _SUPER_HEADER;
		uint32_t _NTiers;
		uint32_t _INTER_PROC_DESCRIPTOR_WORDS;
		uint32_t _beyond_entries_for_tiers_and_mutex;

	//
		atomic_flag *_readerAtomicFlag[MAX_TIERS];


}




#endif // _H_HOPSCOTCH_HASH_LRU_