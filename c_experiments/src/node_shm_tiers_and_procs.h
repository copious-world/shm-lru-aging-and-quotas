#ifndef _H_TIERS_AND_PROCS_
#define _H_TIERS_AND_PROCS_
#pragma once

#include <queue>

using namespace std;

#include "node_shm_LRU.h"

using namespace std::chrono;


class LRU_cache;

/**
 * 
 * This module provides the entry points to API calls which provide access to storage operations 
 * which might be performed in process or performed in separate threads. This module manages tiers of 
 * storage of entries, where each tier manages data in a window of time determined by the least and maximum age of an entry.
 * 
 * The primary tier handles the most recently added or updated data. As data ages out of the primary tier, the data 
 * will move to higher numbered tiers. Entry requests, keyed on the data hash,
 * can be supplied with a time stamp, which may aid in the location of an entry. If the entry cannot be found in the primary 
 * tier using its hash value, then it might be found in a higher number tier that includes the time stamp provided by the 
 * caller. As an option, the request may direct the search methods to find an entry by its hash by looking through the tiers. 
 * (Sequencing through tiers may be handed to a thread to offload longer searches from main service processes.)
 * 
 * The hash key is determined externally to the module, but the hash has some required charactisrics.
 * 
 * The 64 bit key stores a 32bit hash (xxhash or other) in the lower word.
 * The 32 bits is a hash of the data being stored in th LRU cache. This hash
 * maps to a hash bucket in the HH hash table.
 * 
 * The top 32 bits stores is a structured bit array. The top 1 bit will be
 * the selector of the hash region where the key match will be found. The
 * bottom 20 bits will be a hash bucket number (found my taking the modulus of the hash with with respect to the 
 * maximum number of elements being stored in a tier.)
 * 
 * 
*/


typedef struct R_ENTRY {
	uint32_t 	process;
	uint32_t	timestamp;
	uint32_t 	h_bucket;
	uint32_t	full_hash;
} r_entry;


template<uint16_t const ExpectedMax = 100>
class RemovalEntryHolder : public  SharedQueue_SRSW<r_entry,ExpectedMax> {};



#define ONE_HOUR 	(60*60*1000)

// messages_reserved is an area to store pointers to the messages that will be read.
// duplicate_reserved is area to store pointers to access points that are trying to insert duplicate

template<const uint8_t MAX_TIERS = 8,const uint8_t RESERVE_FACTOR = 3,class LRU_c_impl = LRU_cache>
class TierAndProcManager : public LRU_Consts {

	public:

		// regions -- the regions are shared memory (yet governed by a processes)
		// sometimes processes will cross over in accessing each other's assigned region...

		TierAndProcManager(void *com_buffer,
								map<key_t,void *> &lru_segs, 
									map<key_t,void *> &hh_table_segs, 
											map<key_t,size_t> &seg_sizes,
												bool am_initializer, stp_table_choice tchoice, uint32_t proc_number,
													uint32_t num_procs, uint32_t num_tiers,
														uint32_t els_per_tier, uint32_t max_obj_size,void **random_segs = nullptr) {
			//
			_am_initializer = am_initializer; // need to keep around for downstream initialization
			//
			_Procs = num_procs;
			_proc = proc_number;
			_NTiers = min(num_tiers,(uint32_t)MAX_TIERS);
			_reserve_size = RESERVE_FACTOR;	// set the precent as part of the build
			_com_buffer = (uint8_t *)com_buffer;			// the com buffer is another share section.. separate from the shared data regions
			//
			_offset_to_com_elements = (LRU_c_impl::NUM_ATOMIC_OPS)*sizeof(atomic_flag)*2*_NTiers;
			//
			//
			uint8_t tier = 0;
			for ( auto p : lru_segs ) {
				//
				key_t key = p.first;
				void *lru_region = p.second;
				size_t seg_sz = seg_sizes[key];
				//
				_tiers[tier] = new LRU_c_impl(lru_region, max_obj_size, seg_sz, els_per_tier, _reserve_size, _Procs, _am_initializer, tier);
				_tiers[tier]->set_tier_table((LRU_cache **)_tiers,_NTiers);
				// initialize hopscotch
				tier++;
				if ( tier > _NTiers ) break;
			}
			//
			tier = 0;
			void **random_seg_ref = random_segs;
			//
			for ( auto p : hh_table_segs ) {
				//
				key_t key = p.first;
				void *hh_region = p.second;
				size_t hh_seg_sz = seg_sizes[key];
				//
				LRU_c_impl *lru = _tiers[tier];
				if ( lru != nullptr ) {
					lru->set_hash_impl(hh_region,hh_seg_sz,els_per_tier,tchoice);
					if ( random_seg_ref != nullptr ) {
						lru->set_random_bits(*random_seg_ref++);
					}
				}
				// initialize hopscotch
				tier++;
				if ( tier > _NTiers ) break;
			}

			_status = true;
			set_reason("OK");

			if ( !set_and_init_com_buffer() ) {
				_status = false;
				set_reason("failed to initialize com buffer");
			}
		}

		virtual ~TierAndProcManager() {}

	public:

		/**
		 * check_expected_com_region_size
		*/

		static uint32_t check_expected_com_region_size(uint32_t num_procs, uint32_t num_tiers) {
			//
			uint32_t predict = LRU_c_impl::check_expected_com_size(num_procs,num_tiers);
			//
			return predict;
		}


		/**
		 * get_owner_proc_area
		*/
		Com_element	*get_owner_proc_area(uint16_t proc) {
			 return ((Com_element *)(_com_buffer + _offset_to_com_elements)) + (proc*_NTiers);
		}


		//
		/**
		 * set_owner_proc_area
		 * 
		 * the com buffer services the acceptance of new data and the output of secondary processes
		*/
		bool		set_owner_proc_area(void) {
			if ( _com_buffer != nullptr ) {
				_owner_proc_area = get_owner_proc_area(_proc);
			}
			return true;
		}


		// -- set_reader_atomic_tags
		/**
		 * set_reader_atomic_tags
		*/
		void 		set_reader_atomic_tags(void) {
			if ( _com_buffer != nullptr ) {
				atomic_flag *af = (atomic_flag *)_com_buffer;
				for ( uint32_t tier = 0; tier < _NTiers; tier++ ) {
#ifndef __APPLE__ 
					af->clear();
#else
					while ( !af->test_and_set() );  // set it high
#endif
					_readerAtomicFlag[tier] = af;
					af++;
				}
			}
		}

		/**
		 * set_removal_atomic_tags
		*/
		void		set_removal_atomic_tags(void) {
			if ( _com_buffer != nullptr ) {
				atomic_flag *af = ((atomic_flag *)_com_buffer) + _NTiers;
				for ( uint32_t i = 0; i < _NTiers; i++ ) {
					_removerAtomicFlag[i] = af;
					af++;
				}
			}
		}


		/**
		 * get_proc_entries
		*/
		Com_element	*get_proc_entries(void) {
			return (Com_element *)(_com_buffer + _NTiers*sizeof(atomic_flag *));
		}


	public:

		/**
		 * set_and_init_com_buffer
		 * 	the shaed com buffer retains a set of values or each process 
		 */	
		bool 		set_and_init_com_buffer() {
			//
			if ( _com_buffer == nullptr ) return false;
			set_reader_atomic_tags();
			set_owner_proc_area();
			//
			if ( _am_initializer ) {
				// _com_buffer is now pointing at the com_buffer... but it has not been essentially formatted.
				Com_element *proc_entry = this->get_proc_entries();  // start after shared tier tags (all proc entries)
				//
				//  N = _Procs*_NTiers :: Maybe overkill, except that may prove useful for processes to contend over individual tiers.
				uint32_t P = _Procs;
				uint32_t T = _NTiers;
				//
				for ( uint32_t p = 0; p < P; p++ ) {
					for ( uint32_t t = 0; t < T; t++ ) {
						proc_entry->_marker.store(CLEAR_FOR_WRITE);
						proc_entry->_hash = 0L;
						proc_entry->_offset = UINT32_MAX;
						proc_entry->_timestamp = 0;
						proc_entry->_tier = t;
						proc_entry->_proc = p;
						proc_entry->_ops = 0;
						memset(proc_entry->_message,0,MAX_MESSAGE_SIZE);
						proc_entry++;
					}
				}
			}
			return true;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		LRU_c_impl	*access_tier(uint8_t tier) {
			if ( (0 <= tier) && (tier < MAX_TIERS) ) {
				return _tiers[tier];
			}
			return nullptr;
		}

		LRU_c_impl	*from_time(uint32_t timestamp) {
			LRU_c_impl *lru = nullptr;
			for ( uint8_t i = 0; i < MAX_TIERS; i++ ) {
				lru = _tiers[i];
				uint32_t lb_time = lru->_lb_time->load();
				if ( timestamp > lb_time ) {
					return lru;
				}
			}

			return nullptr;
		}

/*
			// instead, just look at the few number of tiers (usually not more than 3)
			// look at their lower and upper bounds, which will be set more intelligently during transfers.

			uint32_t index = time_interval_b_search(timestamp, _t_times, _NTiers);
			if ( index < _NTiers ) {
				return _tiers[index];
			}
*/


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		Com_element *access_point(uint8_t tier = 0) {
			Com_element *ce = (_owner_proc_area + tier);  // access to the control block of the instance's `_proc` for the tier
			return ce;
		}

		atomic<COM_BUFFER_STATE> *get_read_marker(uint8_t tier = 0) {
			Com_element *ce = (_owner_proc_area + tier);  // From the instance's reserved com elements by it's `_proc` member.
			return &(ce->_marker);   // The marker (may be the first field), provides access to just the atomically shared field.
		}

		uint64_t	*get_hash_parameter(uint8_t tier = 0) {		// the hash parameter of a read and write operations
			Com_element *ce = (_owner_proc_area + tier);		// values will be written here and modified values returned here.
			return &(ce->_hash);
		}

		uint32_t	*get_offset_parameter(uint8_t tier = 0) {	// The offset to the storage data area for writing the object
			Com_element *ce = (_owner_proc_area + tier);		// new element offsets will be returned here
			return &(ce->_offset);
		}


		Com_element	*access_point(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = get_owner_proc_area(proc);;
			Com_element *ce = (owner_proc_area + tier);
			return ce;
		}

		atomic<COM_BUFFER_STATE> *get_read_marker(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = get_owner_proc_area(proc);;
			Com_element *ce = (owner_proc_area + tier);
			return &(ce->_marker);
		}

		uint64_t	*get_hash_parameter(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = get_owner_proc_area(proc);;
			Com_element *ce = (owner_proc_area + tier);
			return &(ce->_hash);
		}

		uint32_t	*get_offset_parameter(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = get_owner_proc_area(proc);;
			Com_element *ce = (owner_proc_area + tier);
			return &(ce->_offset);
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		/**
		 * run_evictions
		 * 
		*/

		bool		run_evictions(LRU_c_impl *lru,uint32_t source_tier,uint32_t ready_msg_count) {
			//
			// lru - is a source tier
			uint32_t req_count = lru->free_mem_requested();
			if ( req_count == 0 ) return true;	// for some reason this was invoked, but no one actually wanted free memory.
			//
			 	// if things have gone really wrong, then another process is busy copying old data 
				// out of this tier's data region. Stuff in reserve will end up in the primary (non-reserve) buffer.
				// The data is shifting out of spots from a previous eviction.  

			if ( lru->check_and_maybe_request_free_mem(ready_msg_count,false) ) return true;  // check if the situation changed on the way here

			//
			LRU_c_impl *next_tier = this->access_tier(source_tier+1);
			if ( next_tier == nullptr ) {
				// crisis mode...				elements will have to be discarded or sent to another machine
				lru->notify_evictor(UINT32_MAX);   // also copy data...
			} else {
				lru->transfer_hashes(next_tier,req_count,1);   // default thread id
			}
			//
			return true;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		// Stop the process on a futex until notified...
		void		wait_for_data_present_notification(uint8_t tier) {
#ifndef __APPLE__
			_readerAtomicFlag[tier]->clear();
			_readerAtomicFlag[tier]->wait(false);  // this tier's LRU shares this read flag
#else
//cout << ((this->_thread_running[tier]) ? "running " : "not running ") << tier << endl;
//cout << "waiting..."; cout.flush();
			// FOR MAC OSX
			while ( _readerAtomicFlag[tier]->test_and_set(std::memory_order_acquire) && this->_thread_running[tier] ) {
//cout << "+"; cout.flush();
				microseconds us = microseconds(100);
				auto start = high_resolution_clock::now();
				auto end = start + us;
				do {
					std::this_thread::yield();
//cout << "."; cout.flush();
				} while ( high_resolution_clock::now() < end );
			}
#endif
		}



		/**
		 * Waking up any thread that waits on input into the tier.
		 * Any number of processes may place a message into a tier. 
		 * If the tier is full, the reader has the job of kicking off the eviction process.
		*/
		bool		wake_up_write_handlers(uint32_t tier) {
#ifndef __APPLE__
			_readerAtomicFlag[tier]->test_and_set();
			_readerAtomicFlag[tier]->notify_all();
#else
			_readerAtomicFlag[tier]->clear();
#endif
			return true;
		}


		/**
		 * wait_for_removal_notification
		*/

		void 		wait_for_removal_notification(uint8_t tier) {
#ifndef __APPLE__
			_removerAtomicFlag[tier]->clear();
			_removerAtomicFlag[tier]->wait(false);  // this tier's LRU shares this read flag
#else
			while ( _removerAtomicFlag[tier]->test_and_set(std::memory_order_acquire) ) {
				microseconds us = microseconds(100);
				auto start = high_resolution_clock::now();
				auto end = start + us;
				do {
					std::this_thread::yield();
				} while ( high_resolution_clock::now() < end );
			}
#endif
		}
		
		bool 		wakeup_removal(uint32_t tier) {
			_removerAtomicFlag[tier]->test_and_set();
#ifndef __APPLE__
			_removerAtomicFlag[tier]->notify_all();
#else
			_removerAtomicFlag[tier]->clear();
#endif
			return true;
		}


		/**
		 * launch_second_phase_threads
		*/
		void 		launch_second_phase_threads() {  // handle reads 
			uint32_t P = _Procs;
			uint32_t T = _NTiers;
			for ( uint8_t i = 0; i < T; i++ ) {
				auto secondary_runner = [&](uint8_t tier) {
					uint32_t P = this->_Procs;
					com_or_offset **messages_reserved = new com_or_offset *[P];
					com_or_offset **duplicate_reserved = new com_or_offset *[P];
					this->_thread_running[tier] = true;
					while ( this->_thread_running[tier] ) {
	            		this->second_phase_write_handler(P,messages_reserved,duplicate_reserved,tier);
					}
					delete[] messages_reserved;
					delete[] duplicate_reserved;
        		};
				_tier_threads[i] = new thread(secondary_runner,i);
				//
				auto removal_runner = [&](uint8_t tier) {
					this->_removals_running[tier] = true;
					while ( this->_removals_running[tier] ) {
						this->removal_thread(tier);
					}
				};
				_tier_removal_threads[i] = new thread(removal_runner,(i + P));
				//
				LRU_c_impl *tier_lru = access_tier(i);
				if ( tier_lru != nullptr ) {
					//
					auto evictor_runner = [&](uint8_t tier,LRU_c_impl *lru) {
						this->_evictors_running[tier] = true;
						while ( this->_evictors_running[tier] ) {
            				lru->local_evictor();
						}
        			};
					//
					_tier_evictor_threads[i] = new thread(evictor_runner,i,tier_lru);
					//
					// hash_table_value_restore_thread
					auto restore_runner = [this](uint8_t slice_for_thread,uint8_t tier,LRU_c_impl *lru) {
						this->_restores_running[tier][slice_for_thread] = true;
						while ( this->_restores_running[tier][slice_for_thread] ) {
	            			lru->hash_table_value_restore_thread(slice_for_thread);
						}
        			};
					//
					_tier_value_restore_for_hmap_threads_0[i] = new thread(restore_runner,0,i,tier_lru);
					_tier_value_restore_for_hmap_threads_1[i] = new thread(restore_runner,1,i,tier_lru);
					//

					// hash_table_value_restore_thread
					// needs a configuration parameter CONFIG
					auto random_generator_runner = [this](uint8_t tier,LRU_c_impl *lru) {
						this->_random_generator_running[tier] = true;
						while ( this->_random_generator_running[tier] ) {
	            			lru->hash_table_random_generator_thread_runner();
						}
        			};
					//
					_tier_random_generator_for_hmap_threads[i] = new thread(random_generator_runner,i,tier_lru);
					//
					
				}
			}
		}


		/**
		 * 			shutdown_threads  -- a final action.. turns off all threads used by this application.
		*/
		void shutdown_threads() {
			for ( uint8_t tier = 0; tier < this->_NTiers; tier++  ) {
				this->_thread_running[tier] = false;
				this->_removals_running[tier] = false;
				this->_evictors_running[tier] = false;
				this->_restores_running[tier][0] = false;
				this->_restores_running[tier][1] = false;
			}
			for ( uint8_t i = 0; i < this->_NTiers; i++  ) {
				_tier_threads[i]->join();
				_tier_removal_threads[i]->join();
				_tier_evictor_threads[i]->join();
				_tier_value_restore_for_hmap_threads_0[i]->join();
				_tier_value_restore_for_hmap_threads_1[i]->join();
			}
		}



		/**
		 * second_phase_write_handler
		 * 
		 * The backend reader operation, second phase write, is launched from a new thread during initialization.
		 * A number of readers may occur among cores for handling insertion and expulsion of data from a tier.
		 * So, a core may handle one or more tiers, launching a thread for each tier it manages.
		 * 
		 * Each read operation is assigned a tier for which it reads. 
		 * Data shared with the writer is negotiated between cores within areas set aside for each tier.
		 * So, for each tier there will be atomics that operate specifically for the tier in question.
		 * 
		*/

		/**
		 * second_phase_waiter
		 * 		helper for `second_phase_write_handler`
		*/

		void second_phase_waiter(queue<uint32_t> &ready_procs,uint16_t proc_count,uint8_t assigned_tier) {
			do {
				for ( uint32_t proc = 0; (proc < proc_count); proc++ ) {
					atomic<COM_BUFFER_STATE> *read_marker = this->get_read_marker(proc, assigned_tier);
					if ( read_marker->load() == CLEARED_FOR_ALLOC ) {   // process has a message
						ready_procs.push(proc);
					}
				}
				//
				if ( ready_procs.size() == 0 ) {
					wait_for_data_present_notification(assigned_tier);
				}
			} while ( (ready_procs.size() == 0) && this->_thread_running[assigned_tier] );
		}

		// At the app level obtain the LRU for the tier and work from there
		//
		int 		second_phase_write_handler(uint16_t proc_count, com_or_offset **messages_reserved, com_or_offset **duplicate_reserved, uint8_t assigned_tier = 0) {
			//
			if ( _com_buffer == NULL  ) {    // com buffer not intialized
				return -5; // plan error numbers: this error is huge problem cannot operate
			}
			if ( (proc_count > 0) && (assigned_tier < _NTiers) ) {
				//
				LRU_c_impl *lru = access_tier(assigned_tier);
				if ( lru == NULL ) {
					return(-1);
				}
				//  messages[ready_msg_count]->_offset
				//
				// 												WAIT FOR WORK
				queue<uint32_t> ready_procs;		// ---- ---- ---- ---- ---- ----
				second_phase_waiter(ready_procs, proc_count, assigned_tier);
				//
				//
				com_or_offset **messages = messages_reserved;  // get this many addrs if possible...
				com_or_offset **accesses = duplicate_reserved;
				//
				// 
				// FIRST: gather messages that are aready for addition to shared data structures.
				// 		OP on com buff
				//
				// Go through all the processes that might have written to this tier.
				// Here, the assigned tier provides the offset into the proc's tier entries.
				// Lock down the message written by the proc for the particular tier.. (often 0)
				//
				uint32_t ready_msg_count = 0;  // ready_msg_count should end up less than or equal to proc_count
				//
				while ( !(ready_procs.empty()) ) {
					//
					uint32_t proc = ready_procs.front();
					ready_procs.pop();
					//
					atomic<COM_BUFFER_STATE> *read_marker = this->get_read_marker(proc, assigned_tier);
					Com_element *access = this->access_point(proc,assigned_tier);
					//
					if ( read_marker->load() == CLEARED_FOR_ALLOC ) {   // process has a message
						//
						claim_for_alloc(read_marker); // This is the atomic update of the write state
						//
						messages[ready_msg_count]->_cel = access;
						accesses[ready_msg_count]->_cel = access;
						//
					}
					//
					ready_msg_count++;
				}
				// rof; 
				//
				// SECOND: If duplicating, free the message slot, otherwise gather memory for storing new objecs
				// 		OP on com buff
				//
				if ( ready_msg_count > 0 ) {  // a collection of message this process/thread will enque
					// 	-- FILTER - only allocate for new objects
					uint32_t additional_locations = lru->filter_existence_check(messages,accesses,ready_msg_count);
					//
					// accesses are null or zero offset if the hash already has an allocated location.
					// If accesses[i] is a zero offset, then the element is new. Otherwise, the element 
					// already exists and its offset has been placed into the corresponding messages[i] location.
					// If accesses[i] is a zero offset, then the messages[i] is a reference to the data write location.
					//
					com_or_offset **tmp_dups = accesses;
					com_or_offset **end_dups = accesses + ready_msg_count;  // look at the whole bufffer, see who is set
					com_or_offset **tmp = messages;

					// Walk the messages and accesses in sync step.
					//	That is, for the write that are updates of things in the table, 
					//	just make the offset to the object available to the writer.
					//	Change the state of its location to allow the client sider being served to write
					//	and later the client sider will clear the position for other writers.
					//
					uint32_t maybe_update[ready_msg_count];
					uint8_t count_updates = 0;
					while ( tmp_dups < end_dups ) {			/// UPDATE ops cleared first (nothing new in the hash table)
						com_or_offset *dup_access = *tmp_dups++;
						//
						// this element has been found and this is actually a data_loc...
						if ( dup_access->_offset != 0 ) {			// duplicated, an occupied location for the hash
							com_or_offset *to = *tmp;
							uint32_t data_loc = to->_offset;		// offset is in the messages buffer
							to->_offset = 0; 						// clear position
							maybe_update[count_updates++] = data_loc;
							Com_element *cel = dup_access->_cel;	// duplicate was not clear... ref to com element
							cel->_offset = data_loc;				// to the com element ... output the known offset
							// now get the control word location
							atomic<COM_BUFFER_STATE> *read_marker = &(cel->_marker);			// use the data location
							//  write data without creating a new hash entry.. (an update)
							clear_for_copy(read_marker);  // tells the requesting process to go ahead and write data.
						}
						tmp++;
					}
					// 
					if ( count_updates > 0 ) {
						lru->timestamp_update(maybe_update,count_updates);
					}
					//
					//	additional_locations
					//		new items go into memory -- hence new allocation (or taking) of positions
					if ( additional_locations > 0 ) {  // new (additional) locations have been allocated 
						//
						// Is there enough memory?							--- CHECK FREE MEMORY
						// there is enough free memory even if this tier cuts into reserves.
						bool add = true;
						while ( !(lru->check_and_maybe_request_free_mem(additional_locations,add)) ) {
							// stop until there is space... getting to this point means 
							// that aggressive and immediate action has to be taken before proceeding.
							// A deterministic outcome is required. This also means that limit checking 
							// and also time interval checking for aging out data have both failed elsewhere.
							if ( !run_evictions(lru,assigned_tier,additional_locations) ) {  // last ditch
								return(-1);			// failed to move of old entries
							}
							add = false;  // this process should not add the same amount to the global free mem request more than once.
						}
						// GET LIST FROM FREE MEMORY 
						//
						// should be on stack
						uint32_t lru_element_offsets[additional_locations+1];
						// clear the buffer
						memset((void *)lru_element_offsets,0,sizeof(uint32_t)*(additional_locations+1)); 

						// the next thing off the free stack.
						// obtain storage for the object data 
						bool mem_claimed = (UINT32_MAX != lru->claim_free_mem(additional_locations,lru_element_offsets)); // negotiate getting a list from free memory
						//
						// if there are elements, they are already removed from free memory and this basket belongs to this process..
						if ( mem_claimed ) {
							//
							// OFFSETS TO HASH TABLE
							uint32_t *current = lru_element_offsets;   // offset to new elemnents in the regions
							uint32_t offset = 0;
							//
							uint32_t N = ready_msg_count;
							com_or_offset **tmp = messages;
							com_or_offset **end_m = messages + N;
							//
							// map hashes to the offsets
							//
							while ( tmp < end_m ) {   // only as many elements as proc placing data into the tier (parameter)
								// read from com buf
								com_or_offset *access_point = *tmp++;
								if ( access_point != nullptr ) {
									//
									offset = *current++;
									//
									Com_element *ce = access_point->_cel;
									//
									uint32_t *write_offset_here = (&ce->_offset);
									uint64_t *hash_parameter =  (&ce->_hash);
									//
									uint64_t hash64 = hash_parameter[0];
									uint32_t full_hash = (uint32_t)((hash64 >> HALF) & 0xFFFFFFFF);
									uint32_t hash_bucket = (uint32_t)(hash64 & 0xFFFFFFFF);
									//
									// second phase writer hands the hash and offset to the lru
									//	this is the first time the lru pairs the hash and offset.
									// 	the lru calls upon the hash table to store the hash/offset pair...
									//

									auto thread_id = lru->_thread_id;
									uint8_t which_slice;
									atomic<uint32_t> *control_bits;
									uint32_t cbits = 0;
									uint32_t cbits_op = 0;
									uint32_t cbits_base_op = 0;
									hh_element *buffer = nullptr;
									hh_element *end_buffer = nullptr;
									hh_element *el = nullptr;
									CBIT_stash_holder *cbit_stashes[4];

									// hash_bucket goes in by ref and will be stamped
									uint64_t augmented_hash = lru->get_augmented_hash_locking(full_hash,&control_bits,&hash_bucket,&which_slice,&cbits,&cbits_op,&cbits_base_op,&el,&buffer,&end_buffer,cbit_stashes);

									if ( augmented_hash != UINT64_MAX ) { // add to the hash table...
										write_offset_here[0] = offset;
										// the 64 bit version goes back to the caller...
										hash_parameter[0] = augmented_hash;  // put the augmented hash where the process can get it.
										//
										atomic<COM_BUFFER_STATE> *read_marker = &(ce->_marker);
										clear_for_copy(read_marker);  // release the proc, allowing it to emplace the new data
										// -- if there is a problem, it will affect older entries
										lru->store_in_hash_unlocking(control_bits,full_hash,hash_bucket,offset,which_slice,cbits,cbits_op,cbits_base_op,el,buffer,end_buffer,cbit_stashes);
									} // else the bucket has not been locked...
								}
							}
							//
							lru->attach_to_lru_list(lru_element_offsets,ready_msg_count);  // attach to an LRU as a whole bucket...
						} else {
							// release messages that are waiting... no memory available so the procs 
							// have to be notified... 
							return -1;
						}
					}
				}
				return 0;
			}
			return(-1);
		}



		/**
		 *		put_method
		 * 
		 * Initiates the process by which the system find a place to write data. This method waits on the position to write data.
		 * 
		 * This method first waits on access only if its entry has already been reached by itself or a process looking to pick up
		 * hash parameters. Once a process or thread is servicing the search for the next location to return for a previous request.
		 * 
		 * This puts to a tier. Most often, this will be called with tier 0 for a new piece of data.
		 * But, it may be invoked for a list of values being moved to an older tier during tier evictions.
		 * 
		*/

		int 		put_method(uint32_t &hash_bucket, uint32_t &full_hash, bool updating, char* buffer, unsigned int size, uint32_t timestamp, uint32_t tier, void (delay_func)()) {
			//
			if ( _com_buffer == nullptr ) return -1;  // has not been initialized
			if ( (buffer == nullptr) || (size <= 0) ) return -1;  // might put a limit on size lower and uppper
			//
			//
			LRU_c_impl *lru = from_time(timestamp);   // this is being accessed in more than one place...

			if ( lru == nullptr ) {  // has not been initialized
				return -1;
			}
			//
			// particular atomics
			atomic<COM_BUFFER_STATE> *read_marker =	this->get_read_marker();
			uint64_t *hash_parameter = this->get_hash_parameter();
			uint32_t *offset_offset = this->get_offset_parameter();

			//
			// Writing will take place after a place in the LRU has been given to this writer...
			// 
			// WAIT - a reader may be still taking data out of our slot.
			// let it finish before puting in the new stuff.

			if ( wait_to_write(read_marker,MAX_WAIT_LOOPS,delay_func) ) {	// will wait (spin lock style) on an atomic indicating the read state of the process
				// 
				// tell a reader to get some free memory
				uint32_t *hpar_low = (uint32_t *)hash_parameter;
				uint32_t *hpar_hi = (hpar_low + 1);
				hpar_low[0] = hash_bucket;	// put in the hash so that the reader can see if this is a duplicate
				hpar_hi[0] = full_hash;		// but also, this is the hash (not yet augmented)
				// the write offset should come back to the process's read maker
				offset_offset[0] = updating ? UINT32_MAX : 0;
				//
				//
				cleared_for_alloc(read_marker);   // allocators can now claim this process request
				//
				// will sigal just in case this is the first writer done and a thread is out there with nothing to do.
				// wakeup a conditional reader if it happens to be sleeping and mark it for reading, 
				// which prevents this process from writing until the data is consumed
				bool status = wake_up_write_handlers(tier);
				if ( !status ) {
					return -2;
				}
				//
				if ( await_write_offset(read_marker,MAX_WAIT_LOOPS,delay_func) ) {
					//
					uint32_t write_offset = offset_offset[0];
					//
					if ( (write_offset == UINT32_MAX) && !(updating) ) {	// a duplicate has been found
						clear_for_write(read_marker);   // next write from this process can now proceed...
						return -1;
					}
					//
					uint8_t *m_insert = lru->data_location(write_offset);  // write offset filtered by above line
					if ( m_insert != nullptr ) {
						hash_bucket = hpar_low[0]; // return the augmented hash ... update by reference...
						full_hash = hpar_hi[0];
						memcpy(m_insert,buffer,min(size,MAX_MESSAGE_SIZE));  // COPY IN NEW DATA HERE...
					} else {
						clear_for_write(read_marker);   // next write from this process can now proceed...
						return -1;
					}
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



		/**
		 * get_method
		 * 
		 * Works on getting the value stored for the hash.
		 * The LRU memory belonging to the tier has the value in its memory (free stack) region if it exists. 
		 * If it can't be found immediately in the first tier, the process attempts to find the best tier to 
		 * find the element based on the current state of timestamps.
		 * 
		 * The process only deals with contention for positions. The process would have to wait in some way to get the element
		 * data. So, waiting on a thread may not be that fruitful even if the waiter is designed to yield time to other threads
		 * closer to the client. Here, threads closer to the client might be servicing new requests that then land in an event queue
		 * behind this lookup probe.
		 * 
		 * 
		*/

		int get_method(uint32_t hash_bucket, uint32_t full_hash, char *data, size_t sz, uint32_t timestamp, uint32_t tier,[[maybe_unused]] void (delay_func)()) {
			uint32_t write_offset = 0;
			//
			if ( tier == 0 ) {
				LRU_c_impl *lru = access_tier(tier);
				if ( lru != nullptr ) {
					write_offset = lru->getter(hash_bucket,full_hash,timestamp);
					if ( write_offset != UINT32_MAX ) {
						uint8_t *stored_data = lru->data_location(write_offset);
						if ( stored_data != nullptr ) {
							memcpy(data,stored_data,sz);
							return 0;
						}
					} else {
						lru = from_time(timestamp);
						if ( lru != nullptr ) {
							write_offset = lru->getter(hash_bucket,full_hash,timestamp);
							if ( write_offset != UINT32_MAX ) {
								uint8_t *stored_data = lru->data_location(write_offset);
								if ( stored_data != nullptr ) {
									memcpy(data,stored_data,sz);
									return 0;
								}
							}
						}
					}
				}
			}
			//
			return -1;
		}



		/**
		 * search_method
		 * 
		 * 
		 * Launch a search thread if `get` fails to find the entry within the first tier.
		 * 
		 * 
		*/

		int search_method(uint32_t hash_bucket, uint32_t full_hash, char *data, size_t sz, uint32_t tier, void (delay_func)()) {
			uint32_t write_offset = 0;
			//
			if ( tier == 0 ) {
				uint32_t timestamp = 0;
				LRU_c_impl *lru = access_tier(tier);
				if ( lru != nullptr ) {
					write_offset = lru->getter(hash_bucket,full_hash,timestamp);
					if ( write_offset != UINT32_MAX ) {
						uint8_t *stored_data = lru->data_location(write_offset);
						if ( stored_data != nullptr ) {
							memcpy(data,stored_data,sz);
							return 0;
						}
					} else {
						// hash_table_value_restore_thread
						memset(data,0,sz);
						auto tier_search_runner = [this](uint32_t tier, uint32_t hash_bucket, uint32_t full_hash, char *data, size_t sz) {
							uint32_t timestamp = 0;
							for ( ; tier < this->_NTiers; tier++ ) {
								LRU_c_impl *lru = this->access_tier(tier);
								uint32_t write_offset = lru->getter(hash_bucket,full_hash,timestamp);
								if ( write_offset != UINT32_MAX ) {
									uint8_t *stored_data = lru->data_location(write_offset);
									if ( stored_data != nullptr ) {
										memcpy(data,stored_data,sz);
										return;
									}
								}
							}
						};
						//
						thread th(tier_search_runner,tier,hash_bucket,full_hash,data,sz);
						while ( data[0] == 0 ) {
							delay_func();  // let other parts of the process/thread run
						}
						th.join();
						return 0;
					}
				}
			}
			//
			return -1;
		}

		/**
		 * del_method
		 * 	A small group of methods. 
		 * 	*	del_method -- the method called by the application, for leaving a key to remove from tables... (hit and run)
		 * 	* 	removal_thread -- this is a thread main runtime. This thread waits to be notified for work in the removal queue.
		 * 						There should be one thread for each tier, as in the case with writing
		 * 	* 	del_action -- called by the removal thread - this method handles the logic of calling the LRU methods for removing all aspects of an object...
		 * -- 
		*/


		/**
		 * del_action
		 * 
		 * attempt to remove an element before it age out of the system. 
		 * 
		 * Performs the get operation, but does not copy the data.
		 * 
		*/

		int del_action(uint32_t hash_bucket, uint32_t full_hash, uint32_t timestamp, uint32_t tier) {
			LRU_c_impl *lru = access_tier(tier);
			if ( lru != nullptr ) {
				auto write_offset = lru->getter(hash_bucket,full_hash);
				if ( write_offset != UINT32_MAX ) {
					uint8_t *stored_data = lru->data_location(write_offset);
					if ( stored_data != nullptr ) {
						// uint32_t new_el_offset = UINT32_MAX;   // block out the position until it is removed...
						uint64_t loaded_key = lru->update_in_hash(full_hash,hash_bucket,UINT32_MAX);
						if ( loaded_key != UINT64_MAX ) {
							LRU_element *le = (LRU_element *)(stored_data);
							uint32_t found_timestamp = le->_when;
							lru->free_memory_and_key(le,hash_bucket,hash_bucket,found_timestamp);
						}
						return 0;
					}
				} else {
					lru = from_time(timestamp);
					if ( lru != nullptr ) {
						write_offset = lru->getter(hash_bucket,full_hash);
						if ( write_offset != UINT32_MAX ) {
							uint8_t *stored_data = lru->data_location(write_offset);
							if ( stored_data != nullptr ) {
								// uint32_t new_el_offset = UINT32_MAX;   // block out the position until it is removed...
								uint64_t loaded_key = lru->update_in_hash(full_hash,hash_bucket,UINT32_MAX);
								if ( loaded_key != UINT64_MAX ) {
									LRU_element *le = (LRU_element *)(stored_data);
									lru->free_memory_and_key(le,hash_bucket,hash_bucket,timestamp);
								}
								return 0;
							}
						}
					}
				}
			}
			return -1;
		}


		/**
		 * removal_thread - this method is the inner loop of the removal thread. 
		 * 	See the thread initializer to understand the looping of the thread.
		 * 
		 *	This method contains a loop, which the method operating on all removal requests it has available.
		 *	The thread will stop looping as soon as it empties the `_removal_work` queue.
		 *	
		*/

		void removal_thread(uint8_t tier) {
			if ( _removal_work[tier].empty() ) {
				wait_for_removal_notification(tier);
			}
			while ( !_removal_work[tier].empty() ) {
				r_entry re;
				if ( _removal_work[tier].pop(re) ) {
					uint32_t timestamp = re.timestamp;
					uint32_t hash_bucket = re.h_bucket;
					uint32_t full_hash = re.full_hash;
					//
					this->del_action(hash_bucket,full_hash,timestamp,tier);
				}
			}
		}


		/**
		 * del_method
		 * 
		 * The del method is the method the client process calls. It's main action is to put the hash key and bucket key
		 * into the removal work queue. After that the `removal_thread` takes over and uses `del_action` to free up the 
		 * storage position and the hash table positions.
		 * 
		*/

		int del_method(uint32_t process,uint32_t h_bucket, uint32_t full_hash,uint32_t timestamp,uint32_t tier) {
			//
			r_entry re = {process,timestamp,h_bucket,full_hash};
			_removal_work[tier].push(re);
			wakeup_removal(tier);		// just in case the removal thread is waiting.
		}

	protected:

		bool					_status;
		char					*_reason;

	public:

		bool					status() {
			return _status;
		}


		void set_reason(const char *msg) {
			_reason = (char *)msg;
		}

		char					*reason() {
			char *msg = this->_reason;
			set_reason("OK");
			return msg;
		}

	//protected:
	public:
		//
		//
		uint8_t					*_com_buffer;
		LRU_c_impl 				*_tiers[MAX_TIERS];		// local process storage
		//
		thread					*_tier_threads[MAX_TIERS];
		bool					_thread_running[MAX_TIERS];
		//
		thread					*_tier_removal_threads[MAX_TIERS];
		bool					_removals_running[MAX_TIERS];
		//
		thread					*_tier_evictor_threads[MAX_TIERS];
		bool					_evictors_running[MAX_TIERS];
		//
		thread					*_tier_value_restore_for_hmap_threads_0[MAX_TIERS];
		thread					*_tier_value_restore_for_hmap_threads_1[MAX_TIERS];
		bool					_restores_running[MAX_TIERS][2];
		//
		thread					*_tier_random_generator_for_hmap_threads[MAX_TIERS];
		bool					_random_generator_running[MAX_TIERS];
		//
		uint16_t				_proc;					// calling proc index (assigned by configuration) (indicates offset in com buffer)
		Com_element				*_owner_proc_area;
		uint8_t					_reserve_size;
		uint16_t				_offset_to_com_elements;

	protected:

	//
		atomic_flag 			*_readerAtomicFlag[MAX_TIERS];
		atomic_flag 			*_removerAtomicFlag[MAX_TIERS];
		RemovalEntryHolder<>	_removal_work[MAX_TIERS];

};




#endif  // _H_HOPSCOTCH_HASH_LRU_