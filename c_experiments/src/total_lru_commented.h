//nothing





// 		LRU_cache(void *region,size_t record_size,size_t region_size,size_t reserve_size,bool am_initializer,uint16_t proc_max,uint8_t tier) {
// 			//
//          _lb_time = (atomic<uint32_t> *)(region);
//          _ub_time = _lb_time + 1;
// 			//
// 			_reason = "OK";
// 			_region = (uint8_t *)region;
// 			_record_size = record_size;
// 			_step = (sizeof(LRU_element) + record_size);
// 			_hmap_i[0] = nullptr;  //
// 			_hmap_i[1] = nullptr;  //
// 			if ( (4*_step) >= region_size ) {
// 				_reason = "(constructor): regions is too small";
// 				_status = false;
// 			} else {
// 				_region_size = region_size;
// 				_max_count = (region_size/_step) - 3;
// 				_count_free = 0;
// 				_count = 0; 
// 				//
// 				if ( am_initializer ) {
// 					setup_region(record_size);
// 				} else {
// 					_count_free = this->_walk_free_list();
// 					_count = this->_walk_allocated_list(1);
// 				}
// 			}

// 			// reserve
// 			_reserve = _region + _region_size;  // the amount of memory v.s. reserve determined by config
// 			_end_reserve = _reserve + reserve_size;

// 			_reserve_size = reserve_size;
// 			_max_reserve_count = (reserve_size/_step) - 3;
// 			_count_reserve_free = 0;
// 			_count_reserve = 0; 
// 			//

// 			if ( am_initializer ) {
// 				setup_region(record_size);
// 				setup_reserve_section(record_size);
// 			} else {
// 				_count_free = this->_walk_free_list();
// 				_count = this->_walk_allocated_list(1);
// 			}
// 			//

// 			// time lower bound and upper bound for a tier...
// 			_lb_time->store(UINT32_MAX);
// 			_ub_time->store(UINT32_MAX);

// 			pair<uint32_t,uint32_t> *primary_storage = (pair<uint32_t,uint32_t> *)(_region + _region_size);
// 			pair<uint32_t,uint32_t> *shared_queue = primary_storage + _max_count;

// 			_timeout_table = new KeyValueManager(primary_storage, _max_count, shared_queue, proc_max);
// 			_configured_tier_cooling = ONE_HOUR;
// 			_configured_shrinkage = 0.3333;

// 			_tier = tier;
// 		}


// 		/**
// 		 * called by exposed method 
// 		 * must be called for initialization
// 		*/

// 		void initialize_header_sizes(uint32_t super_header_size,uint32_t N_tiers,uint32_t words_for_mutex_and_conditions) {
// 			_SUPER_HEADER = super_header_size;
// 			_NTiers = N_tiers;
// 			_INTER_PROC_DESCRIPTOR_WORDS = words_for_mutex_and_conditions;		// initialized by exposed method called by coniguration.
// 			_beyond_entries_for_tiers_and_mutex = (_SUPER_HEADER*_NTiers);
// 		}


// 		void set_configured_tier_cooling_time(uint32_t delay = ONE_HOUR) {
// 			_configured_tier_cooling = delay;
// 		}

// 		void set_configured_shrinkage(double fractional) {
// 			if ( fractional > 0 && fractional < 0.5 ) {
// 				_configured_shrinkage = fractional;
// 			}
// 		}

// 		void set_sharing_tiers(LRU_cache *tiers[4]) {
// 			for ( int i = 0; i < 4; i++ ) {
// 				_sharing_tiers[i] = tiers[i];
// 			}
// 		}



// 		uint16_t setup_region_free_list(size_t record_size, uint8_t *start, size_t step, size_t region_size) {

// 			uint16_t free_count = 0;

// 			LRU_element *ctrl_hdr = (LRU_element *)start;
// 			ctrl_hdr->_prev = UINT32_MAX;
// 			ctrl_hdr->_next = step;
// 			ctrl_hdr->_hash = 0;
// 			ctrl_hdr->_when = 0;
			
// 			LRU_element *ctrl_tail = (LRU_element *)(start + step);
// 			ctrl_tail->_prev = 0;
// 			ctrl_tail->_next = UINT32_MAX;
// 			ctrl_tail->_hash = 0;
// 			ctrl_tail->_when = 0;

// 			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);
// 			ctrl_free->_prev = UINT32_MAX;
// 			ctrl_free->_next = 3*step;
// 			ctrl_free->_hash = 0;
// 			ctrl_free->_when = 0;

// 			//
// 			size_t curr = ctrl_free->_next;
// 			size_t next = 4*step;
			
// 			while ( curr < region_size ) {   // all the ends are in the first three elements ... the rest is either free or part of the LRU
// 				free_count++;
// 				LRU_element *next_free = (LRU_element *)(start + curr);
// 				next_free->_prev = UINT32_MAX;  // singly linked free list
// 				next_free->_next = next;
// 				if ( next >= region_size ) {
// 					next_free->_next = UINT32_MAX;
// 				}
// 				next_free->_hash = UINT64_MAX;
// 				next_free->_when = 0;
// 				//
// 				curr += step;
// 				next += step;
// 			}

// 			ctrl_free->_hash = free_count;   // how many free elements avaibale
// 			ctrl_hdr->_hash = 0;

// 			return free_count;
// 		}


// 		// setup_region -- part of initialization if the process is the intiator..
// 		void setup_region(size_t record_size) {
// 			uint8_t *start = this->start();
// 			size_t region_size = _region_size;
// 			_count_free = setup_region_free_list(record_size,start,_step,region_size);
// 		}

// 				// setup_region -- part of initialization if the process is the intiator..
// 		void setup_reserve_section(size_t record_size) {
// 			uint8_t *start = _reserve;
// 			size_t region_size = _region_size;
// 			_count_free = setup_region_free_list(record_size,start,_step);
// 		}


// 		// add_el
// 		// 		data -- data that will be stored at the end of the free list
// 		//		hash64 -- a large hash of the data. The hash will be processed for use in the hash table.
// 		//	The hash table will provide reverse lookup for the new element being added by allocating from the free list.
// 		//	The hash table stores the index of the element in the managed memory. 
// 		// 	So, to retrieve the element later, the hash will fetch the offset of the element in managed memory
// 		//	and then the element will be retrieved from its loction.
// 		//	The number of elements in the free list may be the same or much less than the number of hash table elements,
// 		//	the hash table can be kept sparse as a result, keeping its operation fairly efficient.
// 		//	-- The add_el method can tell that it has no more room for elements by looking at the free list.
// 		//	-- When the free list is empty, add_el returns UINT32_MAX. When the free list is empty,
// 		//	-- some applications may want to extend share memory by adding more hash slabs or by enlisting secondary processors, 
// 		//	-- or by running evictions on the tail of the list of allocated elements.
// 		//	Given there is a free slot for the element, add_el puts the elements offset into the hash table.
// 		//	add_el moves the element from the free list to the list of allocated positions. These are doubly linked lists.
// 		//	Each element of the list as a time of insertion, which add_el records.
// 		//

// 		uint32_t add_el(char *data,uint64_t hash64) {
// 			uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
// 			uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
// 			return add_el(data, full_hash, hash_bucket);
// 		}


// 		uint32_t add_el(char *data,uint32_t full_hash,uint32_t hash_bucket) {
// 			//
// 			uint8_t *start = this->start();
// 			size_t step = _step;

// 			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);
// 			if (  ctrl_free->_next == UINT32_MAX ) {
// 				_status = false;
// 				_reason = "out of free memory: free count == 0";
// 				return(UINT32_MAX);
// 			}
// 			//
// 			uint32_t new_el_offset = ctrl_free->_next;
// 			// in rare cases the hash table may be frozen even if the LRU is not full
// 			//
// 			uint64_t store_stat = this->store_in_hash(full_hash,hash_bucket,new_el_offset);  // STORE
// 			//
// 			if ( store_stat == UINT64_MAX ) {
// 				return(UINT32_MAX);
// 			}

// 			LRU_element *new_el = (LRU_element *)(start + new_el_offset);
// 			ctrl_free->_next = new_el->_next;
//         	//
// 			LRU_element *header = (LRU_element *)(start);
// 			LRU_element *first = (LRU_element *)(start + header->_next);
// 			new_el->_next = header->_next;
// 			first->_prev = new_el_offset;
// 			new_el->_prev = 0; // offset to header
// 			header->_next = new_el_offset;
// 			//
// 			new_el->_when = epoch_ms();
// 			uint64_t hash64 = (((uint64_t)full_hash << HALF) | (uint64_t)hash_bucket);	
// 			new_el->_hash = hash64;
// 			char *store_el = (char *)(new_el + 1);
// 			//
// 			memset(store_el,0,this->_record_size);
// 			size_t cpsz = min(this->_record_size,strlen(data));
// 			memcpy(store_el,data,cpsz);
// 			//
// 			_count++;
// 			if ( _count_free > 0 ) _count_free--;
// 			return(new_el_offset);
// 	    }


// 		// get_el
// 		uint8_t get_el(uint32_t offset,char *buffer) {
// 			if ( !this->check_offset(offset) ) return(2);
// 			//
// 			uint8_t *start = this->start();
// 			LRU_element *stored = (LRU_element *)(start + offset);
// 			char *store_data = (char *)(stored + 1);
// 			memcpy(buffer,store_data,this->_record_size);
// 			if ( this->touch(stored,offset) ) return(0);
// 			return(1);
// 		}

// 		// get_el_untouched
// 		uint8_t get_el_untouched(uint32_t offset,char *buffer) {
// 			if ( !this->check_offset(offset) ) return(2);
// 			//
// 			uint8_t *start = this->start();
// 			LRU_element *stored = (LRU_element *)(start + offset);
// 			char *store_data = (char *)(stored + 1);
// 			memcpy(buffer,store_data,this->_record_size);
// 			return(0);
// 		}

// 		// update_el
// 		bool update_el(uint32_t offset,char *buffer) {
// 			if ( !this->check_offset(offset) ) return(false);
// 			//
// 			uint8_t *start = this->start();
// 			LRU_element *stored = (LRU_element *)(start + offset);
// 			if ( this->touch(stored,offset) ) {
// 				char *store_data = (char *)(stored + 1);
// 				memcpy(store_data,buffer,this->_record_size);
// 				return(true);
// 			}
// 			_reason = "deleted";
// 			return(false);
// 		}

// 		//
// 		// del_el
// 		bool del_el(uint32_t offset) {
// 			if ( !this->check_offset(offset) ) return(false);
// 			uint8_t *start = this->start();
// 			//
// 			LRU_element *stored = (LRU_element *)(start + offset);
// 			//
// 			uint32_t prev_off = stored->_prev;
// 			uint64_t hash = stored->_hash;
// 	//cout << "del_el: " << offset << " hash " << hash << " prev_off: " << prev_off << endl;
// 			//
// 			if ( (prev_off == UINT32_MAX) && (hash == UINT32_MAX) ) {
// 				_reason = "already deleted";
// 				return(false);
// 			}
// 			uint32_t next_off = stored->_next;
// 	//cout << "del_el: " << offset << " next_off: " << next_off << endl;

// 			LRU_element *prev = (LRU_element *)(start + prev_off);
// 			LRU_element *next = (LRU_element *)(start + next_off);
// 			//
// 	//cout << "del_el: " << offset << " prev->_next: " << prev->_next  << " next->_prev " << next->_prev  << endl;
// 			prev->_next = next_off;
// 			next->_prev = prev_off;
// 			//
// 			stored->_hash = UINT64_MAX;
// 			stored->_prev = UINT32_MAX;
// 			//
// 			LRU_element *ctrl_free = (LRU_element *)(start + 2*(this->_step));
// 			stored->_next = ctrl_free->_next;
// 	//cout << "del_el: " << offset << " ctrl_free->_next: " << ctrl_free->_next << endl;
// 			ctrl_free->_next = offset;
// 			//
// 			this->remove_key(hash);
// 			_count_free++;
// 			//
// 			return(true);
// 		}


// 		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// 		// HASH TABLE USAGE
// 		void remove_key(uint64_t hash) {
// 			//
// 			uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
// 			HMap_interface *T = _hmap_i[selector];

// 			if ( T == nullptr ) {   // no call to set_hash_impl
// 				_local_hash_table.erase(hash);
// 			} else {
// 				T->del(hash);
// 			}
// 		}

// 		void clear_hash_table(uint8_t selector) {
// 			HMap_interface *T = _hmap_i[selector];
// 			if ( T == nullptr ) {   // no call to set_hash_impl
// 				_local_hash_table.clear();
// 			} else {
// 				T->clear();
// 			}

// 		}



// 		// check_for_hash
// 		// either returns an offset to the data or returns the UINT32_MAX.  (4,294,967,295)
// 		uint32_t check_for_hash(uint64_t key) {
// 			uint8_t selector = ((key & HH_SELECT_BIT) == 0) ? 0 : 1;
// 			HMap_interface *T = _hmap_i[selector];
// 			if ( T == nullptr ) {   // no call to set_hash_impl -- means the table was not initialized to use shared memory
// 				if ( _local_hash_table.find(key) != _local_hash_table.end() ) {
// 					return(_local_hash_table[key]);
// 				}
// 			} else {
// 				uint32_t result = T->get(key);
// 				if ( result != 0 ) return(result);
// 			}
// 			return(UINT32_MAX);
// 		}

// 		// check_for_hash
// 		// either returns an offset to the data or returns the UINT32_MAX.  (4,294,967,295)
// 		uint32_t check_for_hash(uint32_t key,uint32_t bucket) {    // full_hash,hash_bucket  :: key == full_hash
// 			uint8_t selector = ((key & HH_SELECT_BIT) == 0) ? 0 : 1;
// 			HMap_interface *T = _hmap_i[selector];
// 			if ( TierAndProcManager == nullptr ) {   // no call to set_hash_impl -- means the table was not initialized to use shared memory
// 					uint64_t hash64 = (((uint64_t)key << HALF) | (uint64_t)bucket);
// 				if ( _local_hash_table.find(hash64) != _local_hash_table.end() ) {
// 					return(_local_hash_table[hash64]);
// 				}
// 			} else {
// 				uint32_t result = T->get(key,bucket);
// 				if ( result != 0 ) return(result);
// 			}
// 			return(UINT32_MAX);
// 		}

// 		// evict_least_used
// 		void evict_least_used(time_t cutoff,uint8_t max_evict,list<uint64_t> &ev_list) {
// 			uint8_t *start = this->start();
// 			size_t step = _step;
// 			LRU_element *ctrl_tail = (LRU_element *)(start + step);
// 			time_t test_time = 0;
// 			uint8_t ev_count = 0;
// 			do {
// 				uint32_t prev_off = ctrl_tail->_prev;
// 				if ( prev_off == 0 ) break; // no elements left... step?? instead of 0
// 				ev_count++;
// 				LRU_element *stored = (LRU_element *)(start + prev_off);
// 				uint64_t hash = stored->_hash;
// 				ev_list.push_back(hash);
// 				test_time = stored->_when;
// 				this->del_el(prev_off);
// 			} while ( (test_time < cutoff) && (ev_count < max_evict) );
// 		}

// 		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// 		void evict_least_used_to_value_map(time_t cutoff,uint8_t max_evict,map<uint64_t,char *> &ev_map) {
// 			uint8_t *start = this->start();
// 			size_t step = _step;
// 			LRU_element *ctrl_tail = (LRU_element *)(start + step);
// 			time_t test_time = 0;
// 			uint8_t ev_count = 0;
// 			uint64_t last_hash = 0;
// 			do {
// 				uint32_t prev_off = ctrl_tail->_prev;
// 				if ( prev_off == 0 ) break; // no elements left... step?? instead of 0
// 				ev_count++;
// 				LRU_element *stored = (LRU_element *)(start + prev_off);
// 				uint64_t hash = stored->_hash;
// 				test_time = stored->_when;
// 				char *buffer = new char[this->record_size()];
// 				this->get_el_untouched(prev_off,buffer);
// 				this->del_el(prev_off);
// 				ev_map[hash] = buffer;
// 			} while ( (test_time < cutoff) && (ev_count < max_evict) );
// 		}

// 		uint8_t evict_least_used_near_hash(uint32_t hash,time_t cutoff,uint8_t max_evict,map<uint64_t,char *> &ev_map) {
// 			//
// 			if ( _hmap_i == nullptr ) {   // no call to set_hash_impl
// 				return 0;
// 			} else {
// 				uint32_t xs[32];
// 				uint8_t selector = ((hash & HH_SELECT_BIT) == 0) ? 0 : 1;
// 				HMap_interface *T = _hmap_i[selector];

// 				uint8_t count = T->get_bucket(hash, xs);
// 				//
// 				uint8_t *start = this->start();
// 				size_t step = _step;
// 				LRU_element *ctrl_tail = (LRU_element *)(start + step);
// 				time_t test_time = 0;
// 				uint8_t ev_count = 0;
// 				uint64_t last_hash = 0;
// 				//
// 				uint8_t result = 0;
// 				if ( count > 0 ) {
// 					for ( uint8_t i = 0; i < count; i++ ) {
// 						uint32_t el_offset = xs[i];
// 						//
// 						LRU_element *stored = (LRU_element *)(start + el_offset);
// 						uint64_t hash = stored->_hash;
// 						test_time = stored->_when;
// 						if ( ((test_time < cutoff) || (ev_count < max_evict)) && (result < MAX_BUCKET_FLUSH) ) {
// 							char *buffer = new char[this->record_size()];
// 							this->get_el_untouched(el_offset,buffer);
// 							this->del_el(el_offset);
// 							ev_map[hash] = buffer;
// 							result++;
// 						}
// 					}
// 				}
// 				return result;
// 			}

// 		}


// 		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// 		size_t _walk_allocated_list(uint8_t call_mapper,bool backwards = false) {
// 			if ( backwards ) {
// 				return(_walk_allocated_list_backwards(call_mapper));
// 			} else {
// 				return(_walk_allocated_list_forwards(call_mapper));
// 			}
// 		}

// 		size_t _walk_allocated_list_forwards(uint8_t call_mapper) {
// 			uint8_t *start = this->start();
// 			LRU_element *header = (LRU_element *)(start);
// 			size_t count = 0;
// 			uint32_t next_off = header->_next;
// 			LRU_element *next = (LRU_element *)(start + next_off);
// 			if ( call_mapper > 2 )  _pre_dump();
// 			while ( next->_next != UINT32_MAX ) {   // this should be the tail
// 				count++;
// 				if ( count >= _max_count ) break;
// 				if ( call_mapper > 0 ) {
// 					if ( call_mapper == 1 ) {
// 						this->_add_map(next,next_off);
// 					} else if ( call_mapper == 2 ) {
// 						this->_add_map_filtered(next,next_off);
// 					} else {
// 						_console_dump(next);
// 					}
// 				}
// 				next_off =  next->_next;
// 				next = (LRU_element *)(start + next_off);
// 			}
// 			if ( call_mapper > 2 )  _post_dump();
// 			return(count - this->_count);
// 		}

// 		size_t _walk_allocated_list_backwards(uint8_t call_mapper) {
// 			uint8_t *start = this->start();
// 			uint32_t step = _step;
// 			LRU_element *tail = (LRU_element *)(start + step);  // tail is one elemet after head
// 			size_t count = 0;
// 			uint32_t prev_off = tail->_prev;
// 			LRU_element *prev = (LRU_element *)(start + prev_off);
// 			if ( call_mapper > 2 )  _pre_dump();
// 			while ( prev->_prev != UINT32_MAX ) {   // this should be the tail
// 				count++;
// 				if ( count >= _max_count ) break;
// 				if ( call_mapper > 0 ) {
// 					if ( call_mapper == 1 ) {
// 						this->_add_map(prev,prev_off);
// 					} else if ( call_mapper == 2 ) {
// 						this->_add_map_filtered(prev,prev_off);
// 					} else {
// 						_console_dump(prev);
// 					}
// 				}
// 				prev_off =  prev->_prev;
// 				prev = (LRU_element *)(start + prev_off);
// 			}
// 			if ( call_mapper > 2 )  _post_dump();
// 			return(count - this->_count);
// 		}

// 		size_t _walk_free_list(void) {
// 			uint8_t *start = this->start();
// 			LRU_element *ctrl_free = (LRU_element *)(start + 2*(this->_step));
// 			size_t count = 0;
// 			LRU_element *next_free = (LRU_element *)(start + ctrl_free->_next);
// 			while ( next_free->_next != UINT32_MAX ) {
// 				count++;
// 				if ( count >= _max_count ) break;
// 				next_free = (LRU_element *)(start + next_free->_next);
// 			}
// 			return(count - this->_count_free);
// 		}
		
// 		bool ok(void) {
// 			return(this->_status);
// 		}

// 		size_t record_size(void) {
// 			return(_record_size);
// 		}

// 		const char *get_last_reason(void) {
// 			const char *res = _reason;
// 			_reason = "OK";
// 			return(res);
// 		}

// 		void reload_hash_map(void) {
// 			this->clear_hash_table();
// 			_count = this->_walk_allocated_list(1);
// 		}

// 		void reload_hash_map_update(uint32_t share_key) {
// 			_share_key = share_key;
// 			_count = this->_walk_allocated_list(2);
// 		}

// 		bool set_share_key(uint32_t offset,uint32_t share_key) {
// 			if ( !this->check_offset(offset) ) return(false);
// 			uint8_t *start = this->start();
// 			//
// 			LRU_element *stored = (LRU_element *)(start + offset);
// 			_share_key = share_key;
// 			stored->_share_key = share_key;
// 			return(true);
// 		}

// 		uint16_t max_count(void) {
// 			return(_max_count);
// 		}

// 		uint16_t current_count(void) {
// 			return(_count);
// 		}

// 		uint16_t free_count(void) {
// 			return(_count_free);
// 		}

// 	public:

// 		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// 		// 		start of the new atomic code for memory stack...

// 		// LRU Methods


// 		/*
// 		Free memory is a stack with atomic var protection.
// 		If a basket of new entries is available, then claim a basket's worth of free nodes and return then as a 
// 		doubly linked list for later entry. Later, add at most two elements to the LIFO queue the LRU.
// 		*/
// 		// LRU_cache method
// 		//
// 		//

// 		// LRU_cache method
// 		void return_to_free_mem(LRU_element *el) {				// a versions of push
// 			_atomic_stack_push(_region,_step);
// 		}


// 		// LRU_cache method
// 		uint32_t claim_free_mem(uint32_t ready_msg_count,uint32_t *reserved_offsets) {
// 			LRU_element *first = NULL;
// 			//
// 			uint8_t *start = this->start();
// 			size_t step = _step;
// 			//
// 			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
// 			//
// 			auto head = static_cast<atomic<uint32_t>*>(&(ctrl_free->_prev));
// 			uint32_t h_offset = head->load(std::memory_order_relaxed);
// 			if ( h_offset == UINT32_MAX ) {
// 				_status = false;
// 				_reason = "out of free memory: free count == 0";
// 				return(UINT32_MAX);
// 			}
// 			auto count_free = static_cast<atomic<uint32_t>*>(&(ctrl_free->_prev));
// 			//
// 			// POP as many as needed
// 			//
// 			uint32_t n = ready_msg_count;
// 			while ( n-- ) {  // consistently pop the free stack
// 				uint32_t next_offset = UINT32_MAX;
// 				uint32_t first_offset = UINT32_MAX;
// 				do {
// 					if ( h_offset == UINT32_MAX ) {
// 						_status = false;
// 						_reason = "out of free memory: free count == 0";
// 						return(UINT32_MAX);			/// failed memory allocation...
// 					}
// 					first_offset = h_offset;
// 					first = (LRU_element *)(start + first_offset);
// 					next_offset = first->_next;
// 				} while( !(head->compare_exchange_weak(h_offset, next_offset)) );  // link ctrl->next to new first
// 				//
// 				if ( next_offset < UINT32_MAX ) {
// 					reserved_offsets[n] = first_offset;  // h_offset should have changed
// 				}
// 			}
// 			//
// 			count_free->fetch_sub(ready_msg_count, std::memory_order_relaxed);
// 			free_mem_claim_satisfied(ready_msg_count);
// 			//
// 			return 0;
// 		}



// 		// LRU_cache method
// 		void done_with_tail(LRU_element *ctrl_tail,atomic<uint32_t> *flag_pos,bool set_high = false) {
// 			if ( set_high ) {
// 				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,0)
// 							&& (ctrl_tail->_share_key == UINT32_MAX));   // if some others have gone through OK
// 			} else {
// 				auto prev_val = flag_pos->load();
// 				if ( prev_val == 0 ) return;  // don't go below zero
// 				flag_pos->fetch_sub(ctrl_tail->_share_key,1);
// 			}
// 		}



// 		// LRU_cache method
// 		atomic<uint32_t> *wait_on_tail(LRU_element *ctrl_tail,bool set_high = false,uint32_t delay = 4) {
// 			//
// 			auto flag_pos = static_cast<atomic<uint32_t>*>(&(ctrl_tail->_share_key));
// 			if ( set_high ) {
// 				uint32_t check = UINT32_MAX;
// 				while ( check !== 0 ) {
// 					check = flag_pos->load(std::memory_order_relaxed);
// 					if ( check != 0 )  {			// waiting until everyone is done with it
// 						usleep(delay);
// 					}
// 				}
// 				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,UINT32_MAX)
// 							&& (ctrl_tail->_share_key) !== UINT32_MAX));
// 			} else {
// 				uint32_t check = UINT32_MAX;
// 				while ( check == UINT32_MAX ) {
// 					check = flag_pos->load(std::memory_order_relaxed);
// 					if ( check == UINT32_MAX )  {
// 						usleep(delay);
// 					}
// 				}
// 				while (!flag_pos->compare_exchange_weak(ctrl_tail->_share_key,(check+1))
// 							&& (ctrl_tail->_share_key < UINT32_MAX) );
// 			}
// 			return flag_pos;
// 		}


// 		/*
// 			Exclude tail pop op during insert, where insert can only be called 
// 			if there is enough room in the memory section. Otherwise, one evictor will get busy evicting. 
// 			As such, the tail op exclusion may not be necessary, but desirable.  Really, though, the tail operation 
// 			necessity will be discovered by one or more threads at the same time. Hence, the tail op exclusivity 
// 			will sequence evictors which may possibly share work.

// 			The problem to be addressed is that the tail might start removing elements while an insert is attempting to attach 
// 			to them. Of course, the eviction has to be happening at the same time elements are being added. 
// 			And, eviction is supposed to happen when memory runs out so the inserters are waiting for memory and each thread having 
// 			encountered the need for eviction has called for it and should simply be higher on the stack waiting for the 
// 			eviction to complete. After that they get free memory independently.
// 		*/


// 		/**
// 		 * Prior to attachment, the required space availability must be checked.
// 		*/
// 		void attach_to_lru_list(uint32_t *lru_element_offsets,uint32_t ready_msg_count) {
// 			//
// 			uint32_t last = lru_element_offsets[(ready_msg_count - 1)];  // freed and assigned to hashes...
// 			uint32_t first = lru_element_offsets[0];  // freed and assigned to hashes...
// 			//
// 			uint8_t *start = this->start();
// 			size_t step = _step;
// 			//
// 			LRU_element *ctrl_hdr = (LRU_element *)start;
// 			//
// 			LRU_element *ctrl_tail = (LRU_element *)(start + step);
// 			auto tail_block = wait_on_tail(ctrl_tail,false); // WAIT :: stops if the tail hash is set high ... this call does not set it

// 			auto head = static_cast<atomic<uint32_t>*>(&(ctrl_hdr->_next));
// 			//
// 			uint32_t next = head->exchange(first);  // ensure that some next (another basket first perhaps) is available for buidling the LRU
// 			//
// 			LRU_element *old_first = (LRU_element *)(start + next);  // this has been settled
// 			//
// 			old_first->_prev = last;			// ---- ---- ---- ---- ---- ---- ---- ----
// 			//
// 			// thread the list
// 			for ( uint32_t i = 0; i < ready_msg_count; i++ ) {
// 				LRU_element *current_el = (LRU_element *)(start + lru_element_offsets[i]);
// 				current_el->_prev = ( i > 0 ) ? lru_element_offsets[i-1] : 0;
// 				current_el->_next = ((i+1) < ready_msg_count) ? lru_element_offsets[i+1] : next;
// 			}
// 			//

// 			done_with_tail(ctrl_tail,tail_block,false);
// 		}



// 		// LRU_cache method
// 		inline pair<uint32_t,uint32_t> lru_remove_last() {
// 			//
// 			uint8_t *start = this->start();
// 			size_t step = _step;
// 			//
// 			LRU_element *ctrl_tail = (LRU_element *)(start + step);
// 			auto tail_block = wait_on_tail(ctrl_tail,true);   // WAIT :: usually this will happen only when memory becomes full
// 			//
// 			auto tail = static_cast<atomic<uint32_t>*>(&(ctrl_tail->_prev));
// 			uint32_t t_offset = tail->load();
// 			//
// 			LRU_element *leaving = (LRU_element *)(start + t_offset);
// 			LRU_element *staying = (LRU_element *)(start + leaving->_prev);
// 			//
// 			uint64_t hash = leaving->_hash;
// 			//
// 			staying->_next = step;  // this doesn't change
// 			ctrl_tail->_prev = leaving->_prev;
// 			//
// 			done_with_tail(ctrl_tail,tail_block,true);
// 			//
// 			pair<uint32_t,uint64_t> p(t_offset,hash);
// 			return p;
// 		}





// 		// LRU_cache method - calls get -- 
// 		/**
// 		 * filter_existence_check
// 		 * 
// 		 * Both arrays, messages and accesses, contain references to hash words.. 
// 		 * These are the hash parameters left by the process requesting storage.
// 		 * 
// 		*/
// 		uint32_t		filter_existence_check(char **messages,char **accesses,uint32_t ready_msg_count) {
// 			uint32_t new_count = 0;
// 			while ( --ready_msg_count >= 0 ) {
// 				uint64_t *hash_loc = (uint64_t *)(messages[ready_msg_count] + OFFSET_TO_HASH);
// 				uint64_t hash = hash_loc[0];
// 				uint32_t data_loc = this->partition_get(hash);
// 				if ( data_loc != 0 ) {    // check if this message is already stored
// 					messages[ready_msg_count] = data_loc;
// 				} else {
// 					new_count++;
// 					accesses[ready_msg_count] = NULL;
// 				}
// 			}
// 			return new_count;
// 		}



// 		// LRU_cache method
// 		bool check_free_mem(uint32_t msg_count,bool add) {
// 			//
// 			uint8_t *start = this->start();
// 			size_t step = _step;
// 			//
// 			LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
// 			// using _prev to count the free elements... it is not updated in this check (just inspected)
// 			_count_free = ctrl_free->_prev;  // using the hash field as the counter  (prev usually indicates the end, but not its a stack)
// 			//
// 			auto requested = static_cast<atomic<uint32_t>*>(&(ctrl_free->_hash));
// 			uint32_t total_requested = requested->load(std::memory_order_relaxed);  // could start as zero
// 			if ( add ) {
// 				total_requested += msg_count;			// update public info about the amount requested 
// 				requested->fetch_add(msg_count);		// should be the amount of all curren requests
// 			}
// 			if ( _count_free < total_requested ) return false;
// 			return true;
// 		}


// 		// _timeout_table



// 		uint32_t timeout_table_evictions(list<uint32_t> &moving,uint32_t req_count) {
// 			//
// 			const auto now = system_clock::now();
//     		const time_t t_c = system_clock::to_time_t(now);
// 			uint32_t min_max_time = (t_c - _configured_tier_cooling);
// 			uint32_t as_many_as = min((_max_count*_configured_shrinkage),(req_count*3));
// 			//
// 			_timeout_table->displace_lowest_value_threshold(moving,min_max_time,as_many_as);
// 			return moving.size();
// 		}

// 		//
// 		//
// 		/**
// 		 * move_mismatch_list_storage
// 		 * 
// 		 * 
// 		 * called by a background thread..
// 		 * 
// 		 * When the primary process claims free memory out of reserve,
// 		 * it also plans the eviction of data in order to move data from 
// 		 * occupied primary storage to secondary storage, so as to free up the 
// 		 * storage in primary. But, the process requesting the eviction will
// 		 * not move the data itself. Instead, it is moved by a secondary process
// 		 * at a later time. 
// 		 * 
// 		 * For a while the key to the storage offset will indicate the original storage
// 		 * location of the data in its offset. But, the key will have been moved already 
// 		 * to the hash table of the new element. (New keys are in the hash table.)
// 		 * 
// 		 * Also, the timestamp will have been moved to the new tier ('this' tier).
// 		 * Similarly, the elements offsets will be refer to the other tier's primary memory.
// 		 * 
// 		 * The stored elemens, stored in the other primary memories, will be detached from the other tier's
// 		 * LRU free list. But, current searches that find the element in the current tier will take their 
// 		 * data from the other tier's primary. That will be the case until this method runs.
// 		 * 
// 		 * When this method runs, the LRU, 'this', looks into the `_timeout_table` for the offsets that 
// 		 * refer to the `source_tier`. It then claims as much of its own free memory to store the 
// 		 * moving data objects. Then, with each claimed memory offset, the LRU copies the object from
// 		 * the offset in the other tier's primary memory to its own primary memory, where the new memory position 
// 		 * in the LRU is one of the claimed free memory offsets.
// 		 * 
// 		 * In this method, the LRU then alters enters the new offset into the hash, doing an update of the hash value, the offset.
// 		 * Given the offset lands appropriately in the hash table, the LRU updates the timestamp entry for the element with 
// 		 * the new offset.
// 		 * 
// 		 * Finally, original copy of the object is discarded by returning the free memory element header to the free storage 
// 		 * of its orignal keeper, the preceeding tier LRU.
// 		 * 
// 		 * Once the aged elements have been removed from the source tier, there is an opportunity to move
// 		 * elements from its reserve back into the storage areas newly returned to the free memory list. This method
// 		 * does not perform that update. But, the caller may signal another worker to begin reclaiming the reserve.
// 		*/
// 		void move_mismatch_list_storage(uint32_t source_tier) {
// 			//
// 			LRU_cache *a_tier = _sharing_tiers[source_tier];
// 			if ( a_tier == nullptr ) return;

// 			// get the entries (set during eviction) that refer to another tier primary storage
// 			list<uint32_t> movables;
// 			this->_timeout_table->mismatch_list(movables); 
// 			// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// 			auto count = movables.size();
// 			uint32_t reserved_offsets[count];    // going to pull out the offsets into the other storage

// 			this->claim_free_mem(count,reserved_offsets);
// 			uint32_t *tmp = &reserved_offsets[0];

// 			list<uint32_t>::iterator *lit = movables.begin();
// 			for ( auto value : movables ) {
// 				//
// 				uint8_t match_shared_tier_mem = (value & TIER_MATCH_MASK);
// 				uint32_t new_offset = *tmp++;
// 				//
// 				if ( (a_tier != nullptr) && (source_tier == match_shared_tier_mem) ) {
// 					uint32_t offset = value & (~TIER_MATCH_MASK);
// 					LRU_element *el_to_move = (LRU_element *)(a_tier->_region + offet);  // coming from the other LRU
// 					LRU_element *el_move_to = (LRU_element *)(_region + new_offset);
// 					memcpy((void *)(el_move_to + 1),(void *)(el_to_move + 1),128);
// 					//
// 					uint64_t hash = el_to_move->_hash;
// 					time_t when = el_to_move->_when;
// 					//
// 					el_move_to->_hash = hash;		// new storage location
// 					el_move_to->_when = when;
// 					//
// 					uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
// 					uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
// 					//
// 					new_offset = (new_offset & (~TIER_MATCH_MASK)) | this->_tier;
// 					//
// 					uint64_t store_stat = this->store_in_hash(full_hash,hash_bucket,new_offset);  // STORE (really an UPDATE)
// 					if ( store_stat != UINT64_MAX ) {
// 						_timeout_table.update_entry(when,when,new_offset);
// 					} // else  need some way to cure this at this point...
// 					//
// 					a_tier->return_to_free_mem(el_to_move);
// 					// a_tier->remove_key(hash);  // key has already been removed...(moved)
// 				}
// 			}
// 		}



// 		/**
// 		 * move_from_reserve_to_primary
// 		 * 
// 		 * This method cleans out the reserve that would have been tapped at a busy moment. The end result of this call
// 		 * is that the primary memory of a tier will be utilized as the storage or the elements put into the reseve, where the
// 		 * primary storage has been previously cleared of evicted objects.
// 		 * 
// 		 * As a result this is the final stage of a four stage process in which the reserve takes steps as follows: first, used to quickly store
// 		 * elements under keys accessible to the application; second, older elements are evicted from the keyed tables, 
// 		 * the hash table and the timer table, and then placed in the hashes of the next tier; third, the moved hashes leave their objects
// 		 * behind with offset values referencing them from the next tier until the process runs to copy out the objects and free
// 		 * the primary memory;
// 		 * 
// 		*/

// 		void move_from_reserve_to_primary() {
// 			LRU_element *el_to_move = (LRU_element *)_reserve;
// 			LRU_element *end_reserve = el_to_move + _max_reserve;

// 			LRU_element *records_in_use[_max_reserve];

// 			uint rec_count = 0;
// 			while ( _N_reserved && (el_to_move < end_reserve) ) {
// 				if ( el_to_move->_share_key & IS_RESERVE_IN_USE ) {
// 					records_in_use[rec_count++] = el_to_move;
// 				}
// 				el_to_move++;
// 			}

// 			// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

// 			uint32_t primary_offsets[rec_count];    // going to pull out the offsets into the other storage
// 			this->claim_free_mem(count,primary_offsets);		// get the new offset
// 			uint32_t *tmp = &primary_offsets[0];

// 			for ( int i = 0; i < rec_count; i++ ) {
// 				//
// 				int8_t *start = (int8_t *)_region;
// 				LRU_element *el = records_in_use[i];
// 				auto new_offset = primary_offsets[i];
// 				//
// 				LRU_element *el_move_to = (LRU_element *)(start + new_offset);
// 				el_move_to->_hash = el->_hash;
// 				el_move_to->_when = el->_when;
// 				el_move_to->_share = el->_share_key;
// 				//
// 				memcpy((void *)(el_move_to + 1),(void *)(el + 1),128);

// 				auto hash64 = el->_hash;

// 				uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
// 				uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
// 				//
// 				uint64_t store_stat = this->store_in_hash(full_hash,hash_bucket,new_offset);  // STORE (really an UPDATE)
// 				if ( store_stat != UINT64_MAX ) {
// 					_timeout_table.update_entry(el->_when,el->_when,new_offset);
// 				} // else  need some way to cure this at this point...
// 				//
// 				this->return_to_free_mem(el_to_move);
// 				_N_reserved--;
// 			}

// 		}


// 	private:

// 		bool touch(LRU_element *stored,uint32_t offset) {
// 			uint8_t *start = this->start();
// 			//
// 			uint32_t prev_off = stored->_prev;
// 			if ( prev_off == UINT32_MAX ) return(false);
// 			uint32_t next_off = stored->_next;

// 			//
// 			LRU_element *prev = (LRU_element *)(start + prev_off);
// 			LRU_element *next = (LRU_element *)(start + next_off);
// 			//
// 			// out of list
// 			prev->_next = next_off; // relink
// 			next->_prev = prev_off;
// 			//
// 			LRU_element *header = (LRU_element *)(start);
// 			LRU_element *first = (LRU_element *)(start + header->_next);
// 			stored->_next = header->_next;
// 			first->_prev = offset;
// 			header->_next = offset;
// 			stored->_prev = 0;
// 			stored->_when = epoch_ms();
// 			//
// 			return(true);
// 		}

// 		bool check_offset(uint32_t offset) {
// 			_reason = "OK";
// 			if ( offset > this->_region_size ) {
// 				//cout << "check_offset: " << offset << " rsize: " << this->_region_size << endl;
// 				_reason = "OUT OF BOUNDS";
// 				return(false);
// 			} else {
// 				uint32_t step = _step;
// 				if ( (offset % step) != 0 ) {
// 					_reason = "ELEMENT BOUNDAY VIOLATION";
// 					return(false);
// 				}
// 			}
// 			return(true);
// 		}

// 		void _add_map(LRU_element *el,uint32_t offset) {
// 			if ( _hmap_i == nullptr ) {   // no call to set_hash_impl
// 				uint64_t hash = el->_hash;
// 				this->store_in_hash(hash,offset);
// 			}
// 		}

// 		void _add_map_filtered(LRU_element *el,uint32_t offset) {
// 			if ( _hmap_i == nullptr ) {   // no call to set_hash_impl
// 				if ( _share_key == el->_share_key ) {
// 					uint64_t hash = el->_hash;
// 					this->store_in_hash(hash,offset);
// 				}
// 			}
// 		}

// 		void _console_dump(LRU_element *el) {
// 			uint8_t *start = this->start();
// 			uint64_t offset = (uint64_t)(el) - (uint64_t)(start);
// 			cout << "{" << endl;
// 			cout << "\t\"offset\": " << offset <<  ", \"hash\": " << el->_hash << ',' << endl;
// 			cout << "\t\"next\": " << el->_next << ", \"prev:\" " << el->_prev << ',' <<endl;
// 			cout << "\t\"when\": " << el->_when << ',' << endl;

// 			char *store_data = (char *)(el + 1);
// 			char buffer[this->_record_size];
// 			memset(buffer,0,this->_record_size);
// 			memcpy(buffer,store_data,this->_record_size-1);
// 			cout << "\t\"value: \"" << '\"' << buffer << '\"' << endl;
// 			cout << "}," << endl;
// 		}

// 		void _pre_dump(void) {
// 			cout << "[";
// 		}
// 		void _post_dump(void) {
// 			cout << "{\"offset\": -1 }]" << endl;
// 		}

// 	public:


// 		const char 						*_reason;
// 		uint8_t		 					*_region;
// 		//
// 		uint8_t							*_reserve; // some offset in the region (by configuration)
// 		uint8_t							*_end_reserve;  // end by configuration (end of total region)
// 		//
// 		size_t		 					_record_size;
// 		uint32_t						_step;
// 		//
// 		size_t							_region_size;
// 		uint16_t						_count_free;
// 		uint16_t						_count;
// 		uint16_t						_max_count;  // max possible number of records

// 		size_t							_reserve_size;
// 		uint16_t						_count_reserve_free;
// 		uint16_t						_count_reserve;
// 		uint16_t						_max_reserve_count;

// 		uint32_t						_share_key;

// 		//
// 		atomic<uint32_t>				*_lb_time;
// 		atomic<uint32_t>				*_ub_time;
// 		//
// 		HMap_interface 					*_hmap_i[2];
// 		//

// 		KeyValueManager					*_timeout_table;
// 		uint32_t						_configured_tier_cooling;
// 		double							_configured_shrinkage;
// 		LRU_cache 						*_sharing_tiers[4];
// 		uint32_t						_tier;
// 		//
// 		unordered_map<uint64_t,uint32_t>			_local_hash_table;
// 		//
// };


