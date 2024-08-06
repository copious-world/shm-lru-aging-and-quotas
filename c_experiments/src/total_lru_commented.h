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


/*

    // not using this... (just to recall what was considered)
    * 
    // raise the lower bound on the times allowed into an LRU 
    // this operation does not run evictions. 
    // but processes running evictions may use it.
    //
    // This is using atomics ... not certain that is the future with this...
    //
    // returns: the old lower bound on time. the lower bound may become the new upper bound of an
    // older tier.

    uint32_t	raise_lru_lb_time_bounds(uint32_t lb_timestamp) {
        //
        uint32_t index = time_interval_b_search(lb_timestamp, _t_times, _NTiers);
        if ( index == 0 ) {
            Tier_time_bucket *ttbr = &_t_times[0];
            ttbr->_ub_time->store(UINT32_MAX);
            uint32_t lbt = ttbr->_lb_time->load();
            if ( lbt < lb_timestamp ) {
                ttbr->_lb_time->store(lb_timestamp);
                return lbt;
            }
            return 0;
        }
        if ( index < _NTiers ) {
            Tier_time_bucket *ttbr = &_t_times[index];
            uint32_t lbt = ttbr->_lb_time->load();
            if ( lbt < lb_timestamp ) {
                uint32_t ubt = ttbr->_ub_time->load();
                uint32_t delta = (lb_timestamp - lbt);
                ttbr->_lb_time->store(lb_timestamp);
                ubt -= delta;
                ttbr->_ub_time->store(lb_timestamp);					
                return lbt;
            }
            return 0;
        }
        //
    }

*/



// SNIPPETS AS WELL



/*

    vector<thread> v;
    for (int n = 0; n < 10; ++n)
        v.emplace_back(f, n);
    for (auto& t : v)
        t.join();

    cout << "sizeof hh_element: " << sizeof(hh_element) << endl;

    uint16_t my_uint = (1 << 7);
    cout << my_uint << " " << (HOLD_BIT_SET & my_uint) << "   "  << bitset<16>(HOLD_BIT_SET) << "   "  << bitset<16>(my_uint) << endl;

    uint16_t a = (my_uint<<1);
    uint16_t b = (my_uint>>1);
    
    cout << countr_zero(my_uint) << " " << countr_zero(a)<< " " << countr_zero(b) << endl;
#ifdef FFS
      cout << FFS(my_uint) << " " << FFS(a)<< " " << FFS(b) << endl;
#endif

    for (const uint8_t i : {0, 0b11111111, 0b00011100, 0b00011101})
        cout << "countr_zero( " << bitset<8>(i) << " ) = "
              << countr_zero(i) << '\n';

#ifdef FFS
    for (const uint8_t i : {0, 0b11111111, 0b00011100, 0b00011101})
        cout << "countr_zero( " << bitset<8>(i) << " ) = "
              << FFS(i) << '\n';
#endif



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


/*
template<typename T, typename OP>
T manipulate_bit(std::atomic<T> &a, unsigned n, OP bit_op) {
    static_assert(std::is_integral<T>::value, "atomic type not integral");

    T val = a.load();
    while (!a.compare_exchange_weak(val, bit_op(val, n)));

    return val;
}

auto set_bit = [](auto val, unsigned n) { return val | (1 << n); };
auto clr_bit = [](auto val, unsigned n) { return val & ~(1 << n); };
auto tgl_bit = [](auto val, unsigned n) { return val ^ (1 << n); };

int main() {
    std::atomic<int> a{0x2216};
    manipulate_bit(a, 3, set_bit);  // set bit 3
    manipulate_bit(a, 7, tgl_bit);  // toggle bit 7
    manipulate_bit(a, 13, clr_bit);  // clear bit 13
    bool isset = (a.load() >> 5) & 1;  // testing bit 5
}
*/






/*



// 7500000 @ 44(+/-) sec. | 75000 @ 0.44 | 7500 @ 0.035 sec | 750000 @ 3.9 sec 
// no threads...

bool dataReady{false};

mutex mutex_;
condition_variable condVar1;          // (1)
condition_variable condVar2;          // (2)

atomic<uint32_t> counter{};
//uint32_t counter;
constexpr uint32_t countlimit = 10000000; // 1'000'000; // 10000000;   70000000;

void ping() {

    while( counter <= countlimit ) {
        {
            unique_lock<mutex> lck(mutex_);
            condVar1.wait(lck, []{return dataReady == false;});
            dataReady = true;
        }
        ++counter; 
        condVar2.notify_one();              // (3)
  }
}

void pong() {

    while( counter < countlimit ) {
        {
            unique_lock<mutex> lck(mutex_);
            condVar2.wait(lck, []{return dataReady == true;});
            dataReady = false;
        }
        condVar1.notify_one();            // (3)
  }

}



atomic_flag condAtomicFlag{};
atomic_flag g_ping_lock = ATOMIC_FLAG_INIT;


constexpr bool noisy_prints = false; 

void f_hit(bool ab_caller,uint32_t count) {
  //
  if ( noisy_prints ) {
    if ( ab_caller ) {
      cout << "P1-a: " << count << " is diff: " << (countlimit - count) << endl;
    } else {
      cout << "P1-b: " << count << " is diff: " << (countlimit - count) << endl;
    }
  }
}

void a_ping_1() {
  //
#ifndef __APPLE__

  while ( counter < countlimit ) {
      while ( g_ping_lock.test(memory_order_relaxed) ) ;
      //g_ping_lock.wait(true);
      ++counter;
      f_hit(true,counter);
      g_ping_lock.test_and_set();   // set the flag to true
      g_ping_lock.notify_one();
  }
  g_ping_lock.test_and_set();
  g_ping_lock.notify_one();
#endif

  cout << "P1: " << counter << " is diff: " << (countlimit - counter) << endl;
  //
}

void a_pong_1() {
  //
#ifndef __APPLE__
  while ( counter <= countlimit ) {
      while ( !(g_ping_lock.test(memory_order_relaxed)) ) g_ping_lock.wait(false);
      uint32_t old_counter = counter;
      f_hit(false,counter);
      g_ping_lock.clear(memory_order_release);
      g_ping_lock.notify_one();
      if ( counter == countlimit ) {
        if ( old_counter < counter ) {
          while ( !(g_ping_lock.test_and_set()) ) usleep(1);
          f_hit(false,counter);
        }
        break;
      }
  }
  //
  cout << "P1-b: " << counter << " is diff: " << (countlimit - counter) << endl;
#endif
  //
}



atomic_flag g_lock = ATOMIC_FLAG_INIT;


void f(int n)
{
    for (int cnt = 0; cnt < 40; ++cnt)
    {
        while ( g_lock.test_and_set(memory_order_acquire) ) // acquire lock
        {
            // Since C++20, it is possible to update atomic_flag's
            // value only when there is a chance to acquire the lock.
            // See also: https://stackoverflow.com/questions/62318642
        #if defined(__cpp_lib_atomic_flag_test)
            while (g_lock.test(memory_order_relaxed)) // test lock
        #endif
                ; // spin
        }
        static int out{};
        cout << n << ((++out % 40) == 0 ? '\n' : ' ');
        g_lock.clear(memory_order_release); // release lock
    }
}


void time_bucket_test() {
  // ----
  uint32_t timestamp = 100;
  uint32_t N = 32; // 300000;
  Tier_time_bucket timer_table[N]; // ----

  atomic<uint32_t> atom_ints[N*2];

  for ( uint32_t i = 0; i < N; i++ ) {
    timer_table[i]._lb_time = &atom_ints[i*2];
    timer_table[i]._ub_time = &atom_ints[i*2 + 1];
    timer_table[i]._lb_time->store(i*5);
    timer_table[i]._ub_time->store((i+1)*5);
  }

  timer_table[N-1]._ub_time->store((UINT32_MAX - 2));

  // for ( uint32_t i = 0; i < N; i++ ) {
  //     auto lb = timer_table[i]._lb_time->load();
  //     auto ub = timer_table[i]._ub_time->load();
  //     cout << i << ". (lb,ub) = (" << lb << "," << ub << ") ..";
  //     cout.flush();
  // }
  // cout << endl;

  auto NN = N*5;
  uint32_t found_1 = 0;
  uint32_t found_3 = 0;
  uint32_t nowish = 0; 

  const auto right_now = std::chrono::system_clock::now();
  nowish = std::chrono::system_clock::to_time_t(right_now);

  for ( uint32_t i = 0; i < NN; i++ ) {

    // found_1 = time_interval_b_search(i,timer_table,N);
    // if ( found_1 == UINT32_MAX ) {
    //   cout << i << " broken at " << endl;
    // }


    found_3 = time_interval_b_search(nowish,timer_table,N);

  }

  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;


  // test 2
	auto start = chrono::system_clock::now();  

  for ( uint32_t i = 0; i < NN; i++ ) {
    //
    found_1 = time_interval_b_search(i,timer_table,N);
    //
  }

  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  found_1 = time_interval_b_search(timestamp,timer_table,N);
  uint32_t found_2 = time_interval_b_search(0,timer_table,N);


  //
  cout << "found: " << found_2 << endl;
  cout << "found: " << found_1 << endl;
  cout << "found: " << found_3 << endl;
  //
  cout << "found 3: (" << nowish << ") " << timer_table[found_3]._lb_time->load() << "," << timer_table[found_3]._ub_time->load() << endl;
  //
  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;

}


void ping_pong_test() {

  uint32_t nowish = 0; 
  const auto right_now = std::chrono::system_clock::now();
  nowish = std::chrono::system_clock::to_time_t(right_now);

  chrono::duration<double> dur_t1 = chrono::system_clock::now() - right_now;

  // test 2
	auto start = chrono::system_clock::now();
  //

  cout << "starting a test" << endl;
#ifndef __APPLE__

  g_ping_lock.clear();
  
#endif

  thread t1(a_ping_1);
  thread t2(a_pong_1);
  //
  start = chrono::system_clock::now();

#ifndef __APPLE__
  g_ping_lock.notify_all();
#endif

  
  t1.join();
  t2.join();

  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;

  cout << "Duration test 1: " << dur_t1.count() << " seconds" << endl;
  cout << "Duration test 2: " << dur_t2.count() << " seconds" << endl;
}


void mutex_ping_pong() {
		thread t1(ping);
		thread t2(pong);
		//
		t1.join();
		t2.join();
}

void capability_test() {
  #if defined(__cpp_lib_atomic_flag_test)
      cout << "THERE REALLY ARE ATOMIC FLAGS" << endl;
  #endif

  #if defined(_GLIBCXX_HAVE_LINUX_FUTEX)
        cout << "There really is a platform wait" << endl;
  #endif

  cout << "size of unsigned long: " << sizeof(unsigned long) << endl;
}


void random_bits_test(Random_bits_generator<> &bs) {
  for ( uint32_t i = 0; i < 200000; i++ ) {
    bs.pop_bit();
//    cout << (bs->pop_bit() ? '1' : '0');
//    cout.flush();
  }
  //cout << endl;
}


// chrono::system_clock::time_point shared_random_bits_test() {

//   auto bs = new Random_bits_generator<65000,8>();
//   //random_bits_test(*bs);

//   for ( int i = 0; i < 8; i++ ) {
//     uint32_t *bits_for_test = new uint32_t[bs->_bits.size()+ 4*sizeof(uint32_t)];
//     bs->set_region(bits_for_test,i);
//     bs->regenerate_shared(i);
//   }
//   //

// 	auto start = chrono::system_clock::now();

//   for ( uint32_t j = 0; j < 1000; j++ ) {
//     for ( uint32_t i = 0; i < 65000; i++ ) {
//       bs->pop_shared_bit();
//     }
//     bs->swap_prepped_bit_regions();
//   }

//   return start;
// }

const char *paths[4] = {
  "/Users/richardalbertleddy/Documents/GitHub/universal-content/shm-lru-aging-and-quotas/c_experiments/data/bits_buffer.txt",
  "/Users/richardalbertleddy/Documents/GitHub/universal-content/shm-lru-aging-and-quotas/c_experiments/data/com_buffer.txt",
  "/Users/richardalbertleddy/Documents/GitHub/universal-content/shm-lru-aging-and-quotas/c_experiments/data/lru_buffer.txt",
  "/Users/richardalbertleddy/Documents/GitHub/universal-content/shm-lru-aging-and-quotas/c_experiments/data/hh_buffer.txt"
};



void shared_mem_test_initialization_components() {

  int status = 0;
  SharedSegmentsManager *ssm = new SharedSegmentsManager();


  uint32_t max_obj_size = 128;
  uint32_t num_procs = 4;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;


  key_t com_key = ftok(paths[0],0);
  status = ssm->initialize_com_shm(com_key, true, num_procs, num_tiers);
  cout << status << endl;

  cout << "Com Buf size: " << ssm->get_seg_size(com_key) << endl;


  key_t key = ftok(paths[1],0);
  status = ssm->initialize_randoms_shm(key,true);
  cout << status << endl;

  //
  bool yes = false;
  cin >> yes;
  cout << yes << endl;


  cout << "Randoms size: " << ssm->get_seg_size(key) << endl;
  cout << "Total size: " << ssm->total_mem_allocated() << endl;

  // 

  list<uint32_t> lru_keys;
  list<uint32_t> hh_keys;

  for ( uint8_t i = 0; i < num_tiers; i++ ) {
    key_t t_key = ftok(paths[2],i);
    key_t h_key = ftok(paths[3],i);
    lru_keys.push_back(t_key);
    hh_keys.push_back(h_key);
  }

  cout << lru_keys.size() << ":: " << hh_keys.size() << endl;
  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  status = ssm->tier_segments_initializers(true,lru_keys,hh_keys,max_obj_size,num_procs,els_per_tier);

  for ( auto p : ssm->_ids_to_seg_sizes ) {
    cout << "ID TO SEG SIZE: " << p.first << ", " << p.second << endl;
  }

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}




void shared_mem_test_initialization_one_call() {

  int status = 0;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
  //
  SharedSegmentsManager *ssm = new SharedSegmentsManager();

  uint32_t max_obj_size = 128;
  uint32_t els_per_tier = 1024;
  uint8_t num_tiers = 3;
  uint32_t num_procs = 4;

  // ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

  key_t com_key = ftok(paths[0],0);
  key_t randoms_key = ftok(paths[1],0);

  list<uint32_t> lru_keys;
  list<uint32_t> hh_keys;

  for ( uint8_t i = 0; i < num_tiers; i++ ) {
    key_t t_key = ftok(paths[2],i);
    key_t h_key = ftok(paths[3],i);
    lru_keys.push_back(t_key);
    hh_keys.push_back(h_key);
  }

  status = ssm->region_intialization_ops(lru_keys, hh_keys, true,
                                  num_procs, num_tiers, els_per_tier, max_obj_size,  com_key, randoms_key);
  //
  //
  cout << "All buffers initialized: ... continue: "; cout.flush();
  bool yes = false;
  cin >> yes;
  cout << yes << endl;

  for ( auto p : ssm->_ids_to_seg_sizes ) {
    cout << "ID TO SEG SIZE: " << p.first << ", " << p.second << endl;
  }
  cout << endl;

  auto check_lru_sz = LRU_cache::check_expected_lru_region_size(max_obj_size, els_per_tier,num_procs);
  auto check_hh_sz = HH_map<>::check_expected_hh_region_size(els_per_tier,num_procs);
  cout << "LRU Expected Buf size: "  << check_lru_sz << endl;
  cout << " HH Expected Buf size: "  << check_hh_sz << endl;

  for ( auto p : ssm->_seg_to_lrus ) {
    cout << "LRU SEG SIZE: " <<  ssm->_ids_to_seg_sizes[p.first] << ", " << check_lru_sz << endl;
  }

  for ( auto p : ssm->_seg_to_hh_tables ) {
    cout << " HH SEG SIZE: " <<  ssm->_ids_to_seg_sizes[p.first] << ", " << check_hh_sz << endl;
  }

  auto check_com_sz = TierAndProcManager<>::check_expected_com_region_size(num_procs,num_tiers);
  cout << "Com Buf size: " << ssm->get_seg_size(com_key) << " check_com_sz: " << check_com_sz << endl;
  //
  //
  auto rsize =  ssm->get_seg_size(randoms_key);
  size_t predicted_rsize = Random_bits_generator<>::check_expected_region_size;   //sizeof(uint32_t)*256*4;  // default sizes
  //
  cout << "Randoms size: " << rsize << " same size: " << (rsize == predicted_rsize) << " should be: " << predicted_rsize << endl;
  cout << "Possible random bits size <1024,16>: " << Random_bits_generator<1024,16>::check_expected_region_size << endl;

  cout << "Total size: " << ssm->total_mem_allocated() << endl;

  //
  pair<uint16_t,size_t> p = ssm->detach_all(true);
  cout << p.first << ", " << p.second << endl;

}





template<uint32_t arg_N>
struct val {
    static constexpr auto N = arg_N;
};

template<template <uint32_t> typename T, uint32_t N>
constexpr auto extract(const T<N>&) -> val<N>;

template<typename T>
constexpr auto extract_N = decltype(extract(std::declval<T>()))::N;



template<template <uint32_t,uint8_t> typename T, uint32_t N>
constexpr auto extract2(const T<N,4>&) -> val<N>;

template<typename T>
constexpr auto extract_N2 = decltype(extract2(std::declval<T>()))::N;



void butter_bug_nothing() {

  uint32_t nowish_1 = 0;
  const auto right_now_1 = std::chrono::system_clock::now();
  nowish_1 = std::chrono::system_clock::to_time_t(right_now_1);
  // ----
  uint32_t k = 0;
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    k++;
  }
  // ----
  chrono::duration<double> dur_1 = chrono::system_clock::now() - right_now_1;
  cout << "butter_bug test 1: " << k << "     " << dur_1.count() << " seconds" << endl;

}


void butter_bug_something() {

  uint8_t butter_bug[10000];

  uint32_t nowish_1 = 0;
  const auto right_now_1 = std::chrono::system_clock::now();
  nowish_1 = std::chrono::system_clock::to_time_t(right_now_1);
  // ----
  uint32_t k = 0;
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    k++;
    butter_bug[k%10000] = k;
  }
  // ----
  chrono::duration<double> dur_1 = chrono::system_clock::now() - right_now_1;
  cout << "butter_bug test 2: " << butter_bug[k%10000] << "     " << dur_1.count() << " seconds" << endl;

}



void butter_bug_walk_only() {

  uint8_t butter_bug[10000];

  uint8_t *bb_tmp = butter_bug;
  uint8_t *bb_end = butter_bug + 10000;
  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    bb_tmp++; if ( bb_tmp >= bb_end ) bb_tmp = butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test 3: " << (int)(bb_tmp - butter_bug) << "     " << dur_2.count() << " seconds" << endl;

}



void butter_bug_walk_n_store() {

  uint8_t butter_bug[10000];

  uint8_t *bb_tmp = butter_bug;
  uint8_t *bb_end = butter_bug + 10000;
  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    *bb_tmp++ = ii; if ( bb_tmp >= bb_end ) bb_tmp = butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_n_store: " << (int)(bb_tmp - butter_bug) << "     " << dur_2.count() << " seconds" << endl;

}


void butter_bug_walk_step_n_store() {

  uint8_t butter_bug[10000];

  uint8_t *bb_tmp = butter_bug;
  uint8_t *bb_end = butter_bug + 10000;

  uint32_t step = sizeof(uint32_t);

  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {
    *bb_tmp = ii; bb_tmp += step; if ( bb_tmp >= bb_end ) bb_tmp = butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_step_n_store: " << (int)bb_tmp[0] << "     " << dur_2.count() << " seconds" << endl;

}


void butter_bug_walk_struct() {

  uint8_t butter_bug[10000];
  memset(butter_bug,0,10000);

  hh_element *bb_tmp = (hh_element *)butter_bug;
  hh_element *bb_end = (hh_element *)(butter_bug + 10000);

  //uint32_t step = sizeof(uint32_t);

  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {               // bb_tmp->key = ii;
    bb_tmp++;  if ( bb_tmp >= bb_end ) bb_tmp = (hh_element *)butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_struct: " << (int)bb_tmp->c.key << "     " << dur_2.count() << " seconds" << endl;

}



void butter_bug_walk_struct_n_store() {

  uint8_t butter_bug[10000];

  hh_element *bb_tmp = (hh_element *)butter_bug;
  hh_element *bb_end = (hh_element *)(butter_bug + 10000) - 1;

  //uint32_t step = sizeof(uint32_t);

  uint32_t nowish_2 = 0;
  const auto right_now_2 = std::chrono::system_clock::now();
  nowish_2 = std::chrono::system_clock::to_time_t(right_now_2);
  // ----
  for ( uint32_t ii = 0; ii < 4000000000L; ii++ ) {               // bb_tmp->key = ii;
    bb_tmp->c.key = ii;
    bb_tmp++;
    if ( bb_tmp >= bb_end ) bb_tmp = (hh_element *)butter_bug;
  }
  // ----
  chrono::duration<double> dur_2 = chrono::system_clock::now() - right_now_2;
  cout << "butter_bug test walk_struct_n_store: " << bb_tmp->c.key << "     " << dur_2.count() << " seconds" << endl;

}



void butter_bug_test() {
  //
  butter_bug_nothing();
  butter_bug_something();
  butter_bug_walk_only();
  butter_bug_walk_n_store();
  butter_bug_walk_step_n_store();
  butter_bug_walk_struct();
  butter_bug_walk_struct_n_store();
  //
}


void calc_prob_limis_experiment(uint64_t tab_sz,uint64_t num_keys) {
  double alpha = (double)(num_keys)/tab_sz;
  double items_per_bucket = 1.0 + (exp(2*alpha) - 1 - 2*alpha)/4;
  cout << "items_per_bucket: " << items_per_bucket << endl;
}



void calc_prob_limis() {
  uint64_t tab_sz = 4000000000L;     // m
  uint64_t num_keys = 400000000L; //3000000000L;   // n
  calc_prob_limis_experiment(tab_sz,num_keys);

  tab_sz = 4000000L;     // m
  num_keys = 3000000L; //3000000000L;   // n
  calc_prob_limis_experiment(tab_sz,num_keys);

  tab_sz = 4000000L;     // m
  num_keys = 1000000L; //3000000000L;   // n
  calc_prob_limis_experiment(tab_sz,num_keys);


  tab_sz = 4000000L;     // m
  num_keys = 4000000L; //3000000000L;   // n
  calc_prob_limis_experiment(tab_sz,num_keys);


  cout << endl;
  cout << "Probability of probe length > 32" << endl;

  double alpha = (double)(4000000000L)/4000000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;

  alpha = (double)(3500000L)/4000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;
  
  alpha = (double)(3000000L)/4000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;

  alpha = (double)(2000000L)/4000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;

  alpha = (double)(1000000L)/4000000L;
  cout << "Alpha = " << alpha << " -> "  << pow(alpha,32) << endl;

  cout << endl;
  cout << "Probability of more than 32 keys in a bucket is 1/factorial_32:" << endl;

  double factorial_32 = 1.0;
  for( int i = 1; i <= 32; i++ ) factorial_32 = factorial_32*i;
  cout << "factorial_32: " << factorial_32 <<  endl;
  cout << "1/factorial_32: " << 1.0/factorial_32 << endl;
  cout << endl;




 
  struct S
  {
      // will usually occupy 2 bytes:
      unsigned char b1 : 3; // 1st 3 bits (in 1st byte) are b1
      unsigned char    : 2; // next 2 bits (in 1st byte) are blocked out as unused
      unsigned char b2 : 6; // 6 bits for b2 - doesn't fit into the 1st byte => starts a 2nd
      unsigned char b3 : 2; // 2 bits for b3 - next (and final) bits in the 2nd byte
  };
  

    std::cout << sizeof(S) << '\n'; // usually prints 2
 
    S s;
    // set distinguishable field values
    s.b1 = 0b111;
    s.b2 = 0b101111;
    s.b3 = 0b11;
 
    // show layout of fields in S
    auto i = (uint16_t)(*((uint16_t *)(&s)));
    // usually prints 1110000011110111
    // breakdown is:  \_/\/\_/\____/\/
    //                 b1 u a   b2  b3
    // where "u" marks the unused :2 specified in the struct, and
    // "a" marks compiler-added padding to byte-align the next field.
    // Byte-alignment is happening because b2's type is declared unsigned char;
    // if b2 were declared uint16_t there would be no "a", b2 would abut "u".
    for (auto b = i; b; b >>= 1) // print LSB-first
        std::cout << (b & 1);
    std::cout << '\n';

    int n = sizeof(S)*8;
    for (int b = 0; b < n; b++ ) // print LSB-first
        std::cout << ((((0x0001 << b) & i) >> b) & 0x1);
    std::cout << '\n';
    cout << bitset<16>{i} << endl;


  control_bits cb;

  cb.busy = 1;
  cb._even.busy = 0;
  cb._even.count = 7;
  cb._odd.count = 15;
  cb._odd.busy = 1;


  cout << "control_bits" << endl;

  cout << sizeof(cb) << endl;
  //
  auto j =  ((uint32_t *)(&cb));
  control_bits *ref = (control_bits *)j;

  cout << bitset<32>{*j} << endl;

  ref->busy = 0;
  cout << bitset<32>{*j} << endl;

  ref->_even.count++;
  cout << bitset<32>{*j} << endl;

  ref->_even.count--;
  cout << bitset<32>{*j} << endl;

  ref->count = 63;
  ref->busy = 1;
  //ref->mod = 1;
  ref->_even.count = 31;
  ref->_even.busy = 1;
  ref->_even.memb = 1;
  ref->_even.mod = 1;

  cout << bitset<32>{*j} << endl;


  cout << "<< control_bits" << endl;


  cout << "---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----" << endl;
  cout << endl;

  cout << "MASK VALUES" << endl;

  // n = sizeof(uint32_t)*8;
  // uint32_t tst_i = DOUBLE_COUNT_MASK_BASE;
  // cout << "DOUBLE_COUNT_MASK_BASE:  " << bitset<32>{tst_i} << endl;
  // tst_i = DOUBLE_COUNT_MASK;
  // cout << "DOUBLE_COUNT_MASK:       " << bitset<32>{tst_i} << endl;
  // cout << std::hex << DOUBLE_COUNT_MASK_BASE << endl;
  // cout << std::hex << DOUBLE_COUNT_MASK << endl;
  // tst_i = HOLD_BIT_SET;
  // cout << "HOLD_BIT_SET:            " << bitset<32>{tst_i} << endl;
  // cout << std::hex << HOLD_BIT_SET << endl;
  // tst_i = FREE_BIT_MASK;
  // cout << "FREE_BIT_MASK:           " << bitset<32>{tst_i} << endl;
  // cout << std::hex << HOLD_BIT_SET << endl;

  cout << std::dec;
  cout << endl;
  cout << endl;

}



void test_some_bit_patterns(void) {

  uint32_t A = 0b10110110101011101011011010101110;
  uint32_t B = 0b10111110101111101111111011101111;

  uint32_t D = 1;
  while ( D != 0 ) {
    uint32_t C = A | B;
    D = ~C;
    uint32_t E = A ^ B;

    cout << bitset<32>(A) << " A"  << endl;
    cout << bitset<32>(B) << " B"  << endl;
    cout << bitset<32>(C) << "  A | B" << endl;
    cout << bitset<32>(D) << "  ~(A | B)" << endl;
    cout << bitset<32>(E) << " A ^ B ... " << countr_one(E) << " ... " << countr_zero((A ^ B) & ~1) << endl;
    //
    cout << countr_zero(D)  <<  " : "  << countl_zero(D) << endl;
    bitset<32> d(D);
    cout << "any: " << d.any() <<  " count: " << d.count() << endl;
    uint8_t shft = countr_zero(D);
    A |= (1 << shft);
    B |= (1 << shft);
  }
  // cout << bitset<32>(E) << endl;
}



void test_zero_above(void) {
  //
  for ( uint8_t i = 0; i < 32; i ++ ) {
    cout << "test_zero_above:\t" << bitset<32>(zero_levels[i]) << endl;
  }

  uint32_t test_pattern =  1  | (1 << 4) | (1 << 7) | (1 << 11) | (1 << 16) | (1 << 17) | (1 << 20) | (1 << 21) | (1 << 22) | 
                          (1 << 23) | (1 << 24) | (1 << 26) | (1 << 27) | (1 << 29) | (1 << 30) | (1 << 31);

  cout << "test_pattern:\t\t" << bitset<32>(test_pattern) << endl;

  uint32_t a = test_pattern;
  a &= (~(uint32_t)1);

  while ( a ) {
    cout << countr_zero(a) << endl;
    uint8_t shift =  countr_zero(a);
    auto masked = a & zero_above(shift);
    cout << "test_mask   :\t\t" << bitset<32>(masked) << endl;
    a &= (~(uint32_t)(1 << shift));
  }

  a = test_pattern;
  a &= (~(uint32_t)1);

  while ( a ) {
    cout << countr_zero(a) << endl;
    uint8_t shift =  countr_zero(a);
    auto masked = test_pattern & zero_above(shift);
    cout << "test_masked :\t\t" << bitset<32>(masked) << endl;
    a &= (~(uint32_t)(1 << shift));
  }

  cout << " ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----" << endl;

  uint32_t b = 0b11101101111100110000101110111111;
  cout << "test_taken  :\t\t" << bitset<32>(b) << endl;

  uint64_t vb_probe_base[40];
  uint64_t *vb_probe = &vb_probe_base[0];
  for ( uint8_t i = 0; i < 32; i++ ) {
    *vb_probe++ = (i+1);
  }

  uint64_t v_passed = (((uint64_t)0b1111) << 34) | (0b10101010101010101010101010101);
  uint8_t hole = countr_one(b);
  uint32_t hbit = (1 << hole);
  a = test_pattern | hbit;
  b = b | hbit;
  // ----

  cout << "test_patt(b):\t\t" << bitset<32>(b) << endl;
  cout << "test_pattern:\t\t" << bitset<32>(test_pattern) << endl;
  cout << "test_patt(a):\t\t" << bitset<32>(a) << endl;

  //vb_probe->c.bits = a;
  //vb_probe->tv.taken = b | hbit;
  cout << "test_hole(a):\t\t" << bitset<32>(zero_above(hole)) << endl;
  a = a & zero_above(hole);
  cout << "test_patt(a):\t\t" << bitset<32>(a) << endl;
  //
  // unset the first bit (which indicates this position starts a bucket)
  a = a & (~((uint32_t)0x1));
  while ( a ) {
    vb_probe = &vb_probe_base[0];
    cout << "test_patt(a):\t\t" << bitset<32>(a) << endl;
    auto offset = countr_zero(a);
    a = a & (~((uint32_t)0x1 << offset));
    vb_probe += offset;
    swap(v_passed,*vb_probe);
    cout << "test_patt(a):\t\t" << bitset<32>(a) << " ++ " << v_passed << endl;
    //swap(time,vb_probe->tv.taken);  // when the element is not a bucket head, this is time... 
  }
  //
  cout << "v_passed: " << bitset<64>(v_passed) << endl;
  vb_probe = &vb_probe_base[0];
  for ( uint8_t i = 0; i < 32; i++ ) {
    cout << "state(" << (int)i << "): " << *vb_probe++ << endl;
  }
}


/*
const uint32_t DOUBLE_COUNT_MASK_BASE = 0xFF;  // up to (256-1)
const uint32_t DOUBLE_COUNT_MASK = (DOUBLE_COUNT_MASK_BASE<<16);

const uint32_t COUNT_MASK = 0x3F;  // up to (64-1)
const uint32_t HI_COUNT_MASK = (COUNT_MASK<<8);
//
const uint32_t HOLD_BIT_SET = (0x1 << 23);
const uint32_t FREE_BIT_MASK = ~HOLD_BIT_SET;
const uint32_t LOW_WORD = 0xFFFF;

const uint32_t HOLD_BIT_ODD_SLICE = (0x1 << (7+8));
const uint32_t FREE_BIT_ODD_SLICE_MASK = ~HOLD_BIT_ODD_SLICE;

const uint32_t HOLD_BIT_EVEN_SLICE = (0x1 << (7));
const uint32_t FREE_BIT_EVEN_SLICE_MASK = ~HOLD_BIT_EVEN_SLICE;


const uint32_t HH_SELECT_BIT = (1 << 24);
const uint32_t HH_SELECT_BIT_MASK = (~HH_SELECT_BIT);
const uint64_t HH_SELECT_BIT64 = (1 << 24);
const uint64_t HH_SELECT_BIT_MASK64 = (~HH_SELECT_BIT64);





uint32_t my_zero_count[256][2048];
uint32_t my_false_count[256][2048];







void print_values_elems(hh_element *elems,uint8_t N,uint8_t start_at = 0) {
  for ( int i = 0; i < N; i++ ) {
    cout << "p values: " << elems->tv.value << " .. (" << (start_at + i) << ")" << endl;
    elems++;
  }
}





void test_some_bit_patterns_2(void) {   //  ----  ----  ----  ----  ----  ----  ----
  //
  uint32_t buffer[128];
  memset(buffer,0xFF,128*sizeof(uint32_t));

  const auto NEIGHBORHOOD = 32;

  uint8_t dist_base = 31;
  while ( dist_base ) {
    cout << " DISTANCE TO BASE IS: " << dist_base << endl;
    uint32_t last_view = (NEIGHBORHOOD - 1 - dist_base);
    //
    uint32_t c = 1 << 31;
    cout << bitset<32>(c) << " last_view = " <<  last_view << endl;

    if ( last_view > 0 ) {
      uint32_t *base = &buffer[64];
      uint32_t *viewer = base - last_view;
      while ( viewer != base ) {
        auto cbit = dist_base + last_view;
        cout << " cbit = " << cbit << " dist_base = " << (int)dist_base << " last_view = " << last_view << endl;
        auto vbits = *viewer;
        vbits = vbits & ~((uint32_t)0x1 << cbit);
        *viewer++ = vbits;
        last_view--;
      }
      // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

      uint32_t *reporter = &buffer[32];

      base++;
      while ( reporter < base ) {
        cout << bitset<32>(*reporter) << endl;
        reporter++;
      }
      // ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
    }
    dist_base--;
    cout << endl;
  }
}


/*
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<uint32_t> ud(0, num_elements-1);
    //


const auto NEIGHBORHOOD = 32;


uint8_t test_usurp_membership_position(hh_element *hash_ref, uint32_t c_bits, hh_element *buffer,hh_element *end) {
  //
  uint8_t k = 0xFF & (c_bits >> 1);  // have stored the offsets to the bucket head
  cout << "test_usurp_membership_position: " << (int)k << endl;
  //
  hh_element *base_ref = (hash_ref - k);  // base_ref ... the base that owns the spot
  cout << "test_usurp_membership_position: cbits =  " << bitset<32>(base_ref->c.bits) << endl;

  base_ref = el_check_beg_wrap(base_ref,buffer,end);
  UNSET(base_ref->c.bits,k);   // the element has been usurped...
  cout << "test_usurp_membership_position: cbits =  " << bitset<32>(base_ref->c.bits) << endl;
  //
  uint32_t c = 1 | (base_ref->tv.taken >> k);   // start with as much of the taken spots from the original base as possible
  //
  cout << "test_usurp_membership_position(0): c =  " << bitset<32>(c) << endl;
  auto hash_nxt = base_ref + (k + 1);
  for ( uint8_t i = (k + 1); i < NEIGHBORHOOD; i++, hash_nxt++ ) {
    if ( hash_nxt->c.bits & 0x1 ) { // if there is another bucket, use its record of taken spots
      cout << "test_usurp_membership_position(1):  =  " << bitset<32>(hash_nxt->tv.taken << i) << " -> c.bits " << bitset<32>(hash_nxt->c.bits) << endl;
      c |= (hash_nxt->tv.taken << i); // this is the record, but shift it....
      cout << "test_usurp_membership_position(a): c =  " << bitset<32>(c) << endl;
      break;
    } else if ( hash_nxt->tv.value != 0 ) {  // set the bit as taken
      SET(c,i);
      cout << "test_usurp_membership_position(b): c =  " << bitset<32>(c) << endl;
    }
  }
  hash_ref->tv.taken = c;
  cout << "test_usurp_membership_position(c): c =  " << bitset<32>(c) << endl;
  return k;
}



hh_element *test_seek_next_base(hh_element *base_probe, uint32_t &c, uint32_t &offset_nxt_base, hh_element *buffer, hh_element *end) {
  hh_element *hash_base = base_probe;
  while ( c ) {
    auto offset_nxt = get_b_offset_update(c);
    base_probe += offset_nxt;
    base_probe = el_check_end_wrap(base_probe,buffer,end);
    if ( base_probe->c.bits & 0x1 ) {
      offset_nxt_base = offset_nxt;
      return base_probe;
    }
    base_probe = hash_base;
  }
  return base_probe;
}



void test_seek_min_member(hh_element **min_probe_ref, hh_element **min_base_ref, uint32_t &min_base_offset, hh_element *base_probe, uint32_t time, uint32_t offset, uint32_t offset_nxt_base, hh_element *buffer, hh_element *end) {
  auto c = base_probe->c.bits;		// nxt is the membership of this bucket that has been found
  c = c & (~((uint32_t)0x1));   // the interleaved bucket does not give up its base...
  if ( offset_nxt_base < offset ) {
    c = c & ones_above(offset - offset_nxt_base);  // don't look beyond the window of our base hash bucket
  }
  c = c & zero_above((NEIGHBORHOOD-1) - offset_nxt_base); // don't look outside the window
  cout << "test_seek_min_member: c = " << bitset<32>(c) << endl;
  //
  while ( c ) {			// same as while c
    auto vb_probe = base_probe;
    auto offset_min = get_b_offset_update(c);
    vb_probe += offset_min;
    vb_probe = el_check_end_wrap(vb_probe,buffer,end);

cout << "test_seek_min_member: offset_min = " << (int)offset_min << " time .. " << vb_probe->tv.taken << " " << time << endl;

    if ( vb_probe->tv.taken <= time ) {
			time = vb_probe->tv.taken;
      *min_probe_ref = vb_probe;
      *min_base_ref = base_probe;
      min_base_offset = offset_min;
    }
  
cout << "----" << endl;

  }
}


typedef struct Q_ENTRY_TEST {
	public:
		uint64_t 	loaded_value;
		uint32_t 	h_bucket;
		uint8_t		which_table;
		uint8_t		thread_id;
		uint8_t   __rest[2];
} q_entry_test;


/*
	QueueEntryHolder ...

template<uint16_t const ExpectedMax = 100>
class QueueEntryHolder_test : public  SharedQueue_SRSW<q_entry_test,ExpectedMax> {};


void entry_holder_test(void) {

  cout << "entry_holder_test" << endl;
  //
  QueueEntryHolder_test<> *qeht = new QueueEntryHolder_test<>();
  //
  cout << "is empty: " << (qeht->empty() ? "true" : "false") << endl;

  q_entry_test  funny_bizz;
  funny_bizz.loaded_value = 1;
  qeht->push(funny_bizz);
  auto its_empty = qeht->empty();
  cout << "is empty: " << (qeht->empty() ? "true" : "false") << endl;
  if ( !(its_empty) ) {
    cout << "check w: " << (qeht->_w_cached - qeht->_beg) << endl;
    cout << "check r: " << (qeht->_r_cached - qeht->_beg) << endl;
    cout << qeht->_w_cached << " :: " << qeht->_r_cached << endl;
    cout << qeht->_w.load() << " :: " << qeht->_r.load() << endl;
  }

  q_entry_test  funny_bizz_out;
  if ( qeht->pop(funny_bizz_out) ) {
    cout << "funny_bizz_out.loaded_value: " << funny_bizz_out.loaded_value << endl;
  }

  if ( qeht->pop(funny_bizz_out) ) {
    cout << "funny_bizz_out.loaded_value: " << funny_bizz_out.loaded_value << endl;
  } else {
    cout << "q e h test is empty" << endl;
  }


  qeht->reset();
  for ( int i = 0; i < 100; i++ ) {
     q_entry_test  fizzy_bun;
     fizzy_bun.loaded_value = i+1;
     auto fullness = qeht->push(fizzy_bun);
     q_entry_test *writable, *nextable;
     if ( qeht->full(&writable,&nextable) ) {
      cout << "qeht full at " << i << " indicated by " << fullness << endl;
     }
  }
  its_empty = qeht->empty();
  cout << "again::  is empty: " << (qeht->empty() ? "true" : "false") << endl;
  if ( !(its_empty) ) {
    cout << "check w c: " << (qeht->_w_cached - qeht->_beg) << endl;
    cout << "check r c: " << (qeht->_r_cached - qeht->_beg) << endl;
    cout << "check w: " << (qeht->_w.load() - qeht->_beg) << endl;
    cout << "check r: " << (qeht->_r.load() - qeht->_beg) << endl;
    cout << qeht->_w_cached << " :: " << qeht->_r_cached << endl;
    cout << qeht->_w.load() << " :: " << qeht->_r.load() << endl;
  }

  for ( int i = 0; i < 100; i++ ) {
    cout << "stored [" << i << "]\t" << qeht->_entries[i].loaded_value << "\t";
  }
  cout << endl;

  for ( int j = 0; j < 20; j++ ) {
     q_entry_test  fizzy_bun;
     fizzy_bun.loaded_value = j+100;
     auto fullness = qeht->push(fizzy_bun);
     q_entry_test *writable, *nextable;
     if ( qeht->full(&writable,&nextable) ) {
      cout << "qeht full at " << j << " indicated by " << fullness << endl;
     }
     qeht->pop(fizzy_bun);
     cout << " popped: " << fizzy_bun.loaded_value << endl;
  }


  for ( int i = 0; i < 100; i++ ) {
    cout << "stored [" << i << "]\t" << qeht->_entries[i].loaded_value << "\t";
  }
  cout << endl;


  its_empty = qeht->empty();
  cout << "is empty: " << (qeht->empty() ? "true" : "false") << endl;
  if ( !(its_empty) ) {
    cout << "check w: " << (qeht->_w_cached - qeht->_beg) << endl;
    cout << "check r: " << (qeht->_r_cached - qeht->_beg) << endl;
    cout << qeht->_w_cached << " :: " << qeht->_r_cached << endl;
    cout << qeht->_w.load() << " :: " << qeht->_r.load() << endl;
  }

  while ( !(qeht->empty()) ) {
    q_entry_test  fuzzy_bin;
    qeht->pop(fuzzy_bin);
    cout << " popped: " << fuzzy_bin.loaded_value << endl;
    cout << "check w c: " << (qeht->_w_cached - qeht->_beg) << endl;
    cout << "check r c: " << (qeht->_r_cached - qeht->_beg) << endl;
    cout << "check w: " << (qeht->_w.load() - qeht->_beg) << endl;
    cout << "check r: " << (qeht->_r.load() - qeht->_beg) << endl;
    cout << qeht->_w_cached << " :: " << qeht->_r_cached << endl;
    cout << qeht->_w.load() << " :: " << qeht->_r.load() << endl;
    cout << " " << endl;
  }


  for ( int j = 0; j < 200; j++ ) {
     q_entry_test  fizzy_bun;
     fizzy_bun.loaded_value = j+200;
     auto fullness = qeht->push(fizzy_bun);
     q_entry_test *writable, *nextable;
     if ( qeht->full(&writable,&nextable) ) {
      cout << "qeht full at " << j << " indicated by " << fullness << endl;
     }
     qeht->pop(fizzy_bun);
     cout << " popped: " << fizzy_bun.loaded_value << endl;
  }


  for ( int i = 0; i < 100; i++ ) {
    cout << "stored [" << i << "]\t" << qeht->_entries[i].loaded_value << "\t";
  }
  cout << endl;


}








void entry_holder_threads_test(void) {

  cout << "entry_holder_test" << endl;
  //
  QueueEntryHolder_test<> *qeht = new QueueEntryHolder_test<>();
  //
  cout << "is empty: " << (qeht->empty() ? "true" : "false") << endl;

  uint64_t results[600];

  memset(results,0,600*sizeof(uint64_t));

  auto primary_runner = [&](void) {
      q_entry_test  funny_bizz;
      int loop_ctrl = 0;
      for ( int i = 0; i < 600; i++ ) {
        funny_bizz.loaded_value = i+1;
        while ( !qeht->push(funny_bizz) && (loop_ctrl < 100000) ) { // qeht->push(funny_bizz);   //  
          loop_ctrl++;
          if ( (loop_ctrl % (1<<14)) == 0 ) {
            __libcpp_thread_yield();
          }
        }
        loop_ctrl = 0;
      }
  };




    uint64_t j_final = 0;
    uint64_t k_final = 0;
    uint64_t q_final = 0;



  auto secondary_runner = [&](void) {
      q_entry_test  fizzy_bun;
      uint64_t j = 0;
      uint64_t k = 0;
      uint64_t q = 0;
      while ( k < 600 ) {
        if ( qeht->pop(fizzy_bun) ) {
          j = fizzy_bun.loaded_value;
          results[k++] = j;
        } else q++;
        //if ( q > 10000 ) break;
      }
      //
      q_final = q;
      k_final = k;
      j_final = j;
  };


  auto start = chrono::system_clock::now();


  thread th1(primary_runner);
  thread th2(secondary_runner);

  th1.join();
  th2.join();


  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;
  cout << "Duration pre print time: " << dur_t2.count() << " seconds" << endl;

  cout << " Q : " << q_final << endl;
  cout << " K : " << k_final << endl;
  cout << " J : " << j_final << endl;

  for ( int i = 1; i < 600; i++ ) {
    uint64_t j = i-1;
    if ( results[i] - results[j] > 1 ) {
      cout << "oops: (" << i << ")" << results[i] << " " << results[j] << endl;
    }
  }

  cout << "FINISHED" << endl;

} 



typedef struct LRU_ELEMENT_HDR_test {

	uint32_t	_info;
	uint32_t	_next;
	uint64_t 	_hash;
	time_t		_when;
	uint32_t	_share_key;

  void		init([[maybe_unused]] uint64_t hash_init = UINT64_MAX) {
  }

} LRU_element_test;


class LRU_cache_test : public AtomicStack<LRU_element_test> {

	public:

};




void stack_threads_test(void) {

  LRU_cache_test *stacker = new LRU_cache_test();
  //
  size_t el_sz = sizeof(LRU_element_test) + sizeof(uint64_t);
  //
  uint16_t test_size = 100;
  size_t reg_size = el_sz*test_size + 2*sizeof(atomic<uint32_t>*);  // two control words at the beginning
  //
  uint8_t *data_area = new uint8_t[reg_size];
  uint32_t *reserved_offsets = new uint32_t[test_size];
  
  stacker->setup_region_free_list(data_area,el_sz,reg_size);

  cout << "got through initialization" << endl;

  LRU_element_test *chk = stacker->_ctrl_free;

  cout << "2*sizeof(atomic<uint32_t>*) :: " << 2*sizeof(atomic<uint32_t>*) << endl;
  cout << "el_sz: " << el_sz << " = sizeof(LRU_element_test)  "
              << sizeof(LRU_element_test) << " + sizeof(uint64_t) " <<  sizeof(uint64_t) << endl;
  cout << "first check: " << chk->_next << endl;



  // uint8_t ii = 0;
  // while ( chk->_next != UINT32_MAX ) {
  //   auto nxt = chk->_next;
  //   if ( nxt == 0 ) {
  //     cout << "nxt is 0" << endl;
  //     break;
  //   }
  //   chk = (LRU_element_test *)(data_area + nxt);
  //   cout << chk->_next << endl;
  //   ii++; 
  //   if ( ii > test_size ) {
  //     cout << "ii greater than the test size" << endl;
  //     break;
  //   }
  // }


  //stacker->pop_number(data_area, 1000, reserved_offsets);

  stacker->pop_number(data_area, (test_size - 20), reserved_offsets);
  //
  cout << stacker->_reason << endl;

  for ( int i = 0; i < test_size; i++ ) {
    cout << reserved_offsets[i] << " "; cout.flush();
  }
  //
  cout << endl;
  //

  for ( int i = 0; i < test_size; i++ ) {
    auto offset = reserved_offsets[i];
    if ( offset != 0 ) {
      LRU_element_test *el = (LRU_element_test *)(data_area + offset);
      stacker->_atomic_stack_push(data_area,el);
    }
  }
  //



  uint32_t *reserved_offsets_1 = new uint32_t[test_size];
  uint32_t *reserved_offsets_2 = new uint32_t[test_size];


  auto primary_runner = [&](void) {
      for ( int k = 0; k < 10000; k++ ) {
        for ( int i = 0; i < test_size; i++ ) {
          auto offset = reserved_offsets_1[i];
          if ( offset != 0 ) {
            reserved_offsets_1[i] = 0;
            LRU_element_test *el = (LRU_element_test *)(data_area + offset);
            stacker->_atomic_stack_push(data_area,el);
            break;
          }
        }
        for ( int i = 0; i < test_size; i++ ) {
          auto offset = reserved_offsets_2[i];
          if ( offset != 0 ) {
            reserved_offsets_2[i] = 0;
            LRU_element_test *el = (LRU_element_test *)(data_area + offset);
            stacker->_atomic_stack_push(data_area,el);
            break;
          }
        }
      }

  };


  auto secondary_runner = [&](void) {
      //
      for ( int i = 0; i < 1000; i++ ) {
        memset(reserved_offsets_1,0,test_size);
        stacker->pop_number(data_area, 10, reserved_offsets_1);
      }
      //
  };



  auto tertiary_runner = [&](void) {
      //
      for ( int i = 0; i < 1000; i++ ) {
        memset(reserved_offsets_2,0,test_size);
        stacker->pop_number(data_area, 10, reserved_offsets_2);
      }
      //
  };


  auto start = chrono::system_clock::now();


  thread th1(primary_runner);
  thread th2(secondary_runner);
  thread th3(tertiary_runner);

  th1.join();
  th2.join();
  th3.join();


  chrono::duration<double> dur_t2 = chrono::system_clock::now() - start;
  cout << "Duration pre print time: " << dur_t2.count() << " seconds" << endl;
}





class LRU_cache_mock : public LRU_cache  {

	public:

		// LRU_cache -- constructor
		LRU_cache_mock(void *region, size_t record_size, size_t seg_sz, size_t els_per_tier, size_t reserve, uint16_t num_procs, bool am_initializer, uint8_t tier) 
                        : LRU_cache(region, record_size, seg_sz, els_per_tier, reserve, num_procs, am_initializer, tier) {
			//
		}

		virtual ~LRU_cache_mock() {}

	public:

    // 
		uint32_t		filter_existence_check(com_or_offset **messages,com_or_offset **accesses,uint32_t ready_msg_count,[[maybe_unused]] uint8_t thread_id) {
			uint32_t new_msgs_count = 0;
			while ( --ready_msg_count >= 0 ) {  // walk this list backwards...
				//
				[[maybe_unused]] uint64_t hash = (uint64_t)(messages[ready_msg_count]->_cel->_hash);
				uint32_t data_loc = _test_data_loc++; // _hmap->get(hash,thread_id);  // random locaton for test....
				//
				if ( data_loc != UINT32_MAX ) {    // check if this message is already stored
					messages[ready_msg_count]->_offset = data_loc;  // just putting in an offset... maybe something better
				} else {
					new_msgs_count++;							// the hash has not been found
					accesses[ready_msg_count]->_offset = 0;		// the offset is not yet known (the space will claimed later)
				}
			}
			return new_msgs_count;
		}



		uint64_t		get_augmented_hash_locking([[maybe_unused]] uint32_t full_hash, [[maybe_unused]] uint32_t *h_bucket_ref,[[maybe_unused]] uint8_t *which_table_ref,[[maybe_unused]] uint8_t thread_id = 1) {
			// [[maybe_unused]] HMap_interface *T = this->_hmap;
			uint64_t result = UINT64_MAX;
			//

			return result;
		}



		//  ----
		void			store_in_hash_unlocking([[maybe_unused]] uint32_t full_hash, [[maybe_unused]] uint32_t h_bucket,[[maybe_unused]] uint32_t offset,[[maybe_unused]] uint8_t which_table,[[maybe_unused]] uint8_t thread_id) {

		}


		uint32_t		getter([[maybe_unused]] uint32_t full_hash,[[maybe_unused]] uint32_t h_bucket,[[maybe_unused]] uint8_t thread_id,[[maybe_unused]] uint32_t timestamp = 0) {
			// can check on the limits of the timestamp if it is not zero
			return _test_data_loc;
		}


		/**
		 * update_in_hash
		*/

		uint64_t		update_in_hash([[maybe_unused]] uint32_t full_hash,[[maybe_unused]] uint32_t hash_bucket,[[maybe_unused]] uint32_t new_el_offset,[[maybe_unused]] uint8_t thread_id = 1) {
      uint64_t result =  (uint64_t)full_hash << HALF | hash_bucket;
			return result;
		}


		/**
		 * remove_key
		*/

		void 			remove_key([[maybe_unused]] uint32_t full_hash, [[maybe_unused]] uint32_t h_bucket,[[maybe_unused]] uint8_t thread_id, [[maybe_unused]] uint32_t timestamp) {
			//_timeout_table->remove_entry(timestamp);

		}



    uint32_t _test_data_loc{10};

 };




//
/*
static inline void cleared_for_alloc(atomic<COM_BUFFER_STATE> *read_marker) {
    auto p = read_marker;
    auto current_marker = p->load();
    while(!p->compare_exchange_weak(current_marker,CLEARED_FOR_ALLOC)
                    && ((COM_BUFFER_STATE)(p->load()) != CLEARED_FOR_ALLOC));
}




class Spinlock{
public:
  Spinlock() { unlock(); }

  virtual ~Spinlock(void) { unlock(); }

  void lock(){
    while( flag.test_and_set() ) __libcpp_thread_yield();
  }

  void unlock(){
    flag.clear();
  }

 private:
   std::atomic_flag flag;
 };




void tryout_cpp_example(void) {
    //
    //
    const int N = 16; // four concurrent readers are allowed
    const int loop_n = 40;
    std::atomic<int> cnt(0);
    std::vector<int> data;
    vector<int> what_read_saw;


    Spinlock w_spin;

    bool start = false;
    bool read_start = false;
    
    auto reader = [&]([[maybe_unused]] int id) {
      while ( !read_start ) std::this_thread::sleep_for(1ms);
      while (true) {
          //
          auto count = cnt.fetch_sub(1,std::memory_order_acquire);
          if ( count <= 0 ) {
            count = 0;
            cnt.store(0,std::memory_order_release);
          }

          w_spin.lock();

          //if ( count > 0 ) {
            what_read_saw.push_back(count);
          //}

          // std::cout << ("reader " + std::to_string(id) +
          //               " sees " + std::to_string(*data.rbegin()) + '\n');

          if ( data.size() >= ((loop_n*N) - 1) && ( count <= 0 ) ) {
            w_spin.unlock();
            break;
          }

          w_spin.unlock();
          if ( count == 0 ) {
            __libcpp_thread_yield();
            //std::this_thread::sleep_for(1ms);
          }
  
          // pause
          //std::this_thread::sleep_for(1ms);
      }
    
    };

    auto writer = [&](int id) {
        while ( !start ) std::this_thread::sleep_for(1ms);
        for (int n = 0; n < loop_n; ++n) {

          w_spin.lock();
          data.push_back(n);
          cnt.fetch_add(N + 1);
          std::cout << "writer " << id << " pushed back " << n << '\n';
          w_spin.unlock();

          // pause
          std::this_thread::sleep_for(1ms);
        }
    };

    auto starter = [&]() {
      std::this_thread::sleep_for(1ms);
      read_start = true;
      std::this_thread::sleep_for(10ms);
      start = true;
    };


    std::vector<std::thread> v;
    for (int n = 0; n < N; ++n) {
      v.emplace_back(reader, 2*n+1);
      v.emplace_back(writer, 2*n+2);
    }
    v.emplace_back(starter);


 
    for (auto& t : v)
        t.join();


    cout << "SIZES: " << data.size() << " " << (N*loop_n) << endl;
    cout << "CNT: " << cnt.load() << endl;

    for ( uint32_t j = 0; j < data.size(); j++ ) {
      cout << data[j] << ", "; cout.flush();
    }
    cout << endl;

    cout << "what_read_saw" << endl;
    for ( uint32_t j = 0; j < what_read_saw.size(); j++ ) {
      cout << what_read_saw[j] << ", "; cout.flush();
    }
    cout << endl;

}







void tryout_atomic_counter(void) {
  //

  std::random_device rd;  // a seed source for the random number engine
  std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
  std::uniform_int_distribution<uint32_t> ud(1, 64);

  //
  const int N = 16; // four concurrent readers are allowed
  const int loop_n = 200;
  const uint32_t max_cnt = 1000;

  std::atomic<uint32_t> cnt(0);
  vector<int> what_read_saw;

  Spinlock w_spin;

  bool start = false;
  bool read_start = false;
  
  auto returner = [&](int id) {
    while ( !read_start ) std::this_thread::sleep_for(1ms);
    while (true) {
        //
        auto count = cnt.fetch_add(1,std::memory_order_acquire);
        if ( count >= max_cnt ) {
          count = max_cnt;
          cnt.store(count,std::memory_order_release);
        }

        w_spin.lock();
          what_read_saw.push_back((int)(-id));
          what_read_saw.push_back((int)count);
        w_spin.unlock();
        //
        if ( count >= max_cnt ) {
          cout << "Thread id (" << id << ") quitting" << endl;
          break;
          //std::this_thread::sleep_for(1ms);
        }

        // pause
        //std::this_thread::sleep_for(1ms);
    }
  
  };

  auto requester = [&]([[maybe_unused]] int id) {
    while ( !start ) std::this_thread::sleep_for(1ms);
    uint32_t breakout = 0;
    for (int n = 0; n < loop_n; n++ ) {
      //
      uint32_t decr = ud(gen);
      //
      uint32_t current = cnt.load(std::memory_order_acquire);
      if ( (decr > 0) && ( current >= decr ) ) {
        auto stored = current;
        do {
          if ( current >= decr ) {
            stored = current - decr;
          } else break;
          while ( !(cnt.compare_exchange_weak(current,stored,std::memory_order_acq_rel)) && (current != stored) );
        } while ( current != stored );
      } else {
        if ( ++breakout > 50000 ) break;
        __libcpp_thread_yield();
      }
      // pause
      std::this_thread::sleep_for(1ms);
    }
  };

  auto starter = [&]() {
    std::this_thread::sleep_for(1ms);
    read_start = true;
    std::this_thread::sleep_for(10ms);
    start = true;
  };


  std::vector<std::thread> v;
  for (int n = 0; n < N; ++n) {
    v.emplace_back(returner, 2*n+1);
    v.emplace_back(requester, 2*n+2);
  }
  v.emplace_back(starter);



  for (auto& t : v)
      t.join();


  cout << "CNT: " << cnt.load() << endl;

  // cout << "what_read_saw" << endl;
  // for ( uint32_t j = 0; j < what_read_saw.size(); j++ ) {
  //   cout << what_read_saw[j] << ", "; cout.flush();
  // }
  // cout << endl;

}




void try_out_relaxed_atomic_counter(void) {
  //
  std::atomic<int> cnt = {0};
  //
  auto f = [&]() {
    for (int n = 0; n < 100; ++n) {
        cnt.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::vector<std::thread> v;
  for (int n = 0; n < 100; ++n) {
      v.emplace_back(f);
  }
  for (auto& t : v) {
      t.join();
  }
  cout << "Final counter value is " << cnt << endl;

  cout << "..." << endl;

    
  int x[500];
  
  std::atomic<bool> done{false};
  
  auto f1 = [&]() {
    for (int i = 0; i<500; i++) {  x[i]=i; std::cout << "R"; }
    
    //atomic_thread_fence(std::memory_order_release);
    done.store(true, std::memory_order_relaxed);
  };
  
  auto f2 = [&]() {
    while( !done.load(std::memory_order_relaxed) ) {
    // do operations not related with tasks
    }
    atomic_thread_fence(std::memory_order_acquire);

    // bool chck = false;
    for (int i = 0; i<500; i++) { std::cout << "A"; }
    // for (int i = 0; i<500; i++) { 
    //   if ( !chck ) { chck = done.load(std::memory_order_relaxed); i--; cout << "."; cout.flush(); }
    //   else { std::cout << "A" << i; }
    //   }
  };

  thread t1 (f1);
  thread t2 (f2);

  t1.join();
  t2.join();


  cout << endl;
  

}


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



void test_atomic_stack_and_timeout_buffer(void) {
  //
}

*/
