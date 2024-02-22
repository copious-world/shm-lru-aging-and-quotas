#ifndef _H_TIERS_AND_PROCS_
#define _H_TIERS_AND_PROCS_
#pragma once

// node_shm_tiers_and_procs.h


using namespace std;

#include "node_shm_LRU.h"

using namespace std::chrono;

#define MAX_BUCKET_FLUSH 12
#define ONE_HOUR 	(60*60*1000)

/**
 * The 64 bit key stores a 32bit hash (xxhash or other) in the lower word.
 * The top 32 bits stores a structured bit array. The top 1 bit will be
 * the selector of the hash region where the key match will be found. The
 * bottom 20 bits will store the element offset (an element number) to the position the
 * element data is stored. 
*/


// messages_reserved is an area to store pointers to the messages that will be read.
// duplicate_reserved is area to store pointers to access points that are trying to insert duplicate


template<const uint8_t MAX_TIERS = 8,const uint8_t RESERVE_FACTOR = 3>
class TierAndProcManager : public LRU_Consts {

	public:

		// regions -- the regions are shared memory (yet governed by a processes)
		// sometimes processes will cross over in accessing each other's assigned region...

		TierAndProcManager(void *com_buffer,
								map<key_t,void *> &lru_segs, 
									map<key_t,void *> &hh_table_segs, 
											map<key_t,size_t> &seg_sizes,
												bool am_initializer, uint32_t proc_number,
													uint32_t num_procs, uint32_t num_tiers,
														uint32_t els_per_tier, uint32_t max_obj_size) {
			//
			_am_initializer = am_initializer; // need to keep around for downstream initialization
			//
			_Procs = num_procs;
			_proc = proc_number;
			_NTiers = min(num_tiers,(uint32_t)MAX_TIERS);
			_reserve_size = RESERVE_FACTOR;	// set the precent as part of the build
			_com_buffer = com_buffer;			// the com buffer is another share section.. separate from the shared data regions

			//

			uint8_t tier = 0;
			for ( auto p : lru_segs ) {
				//
				key_t key = p.first;
				void *lru_region = p.second;
				size_t seg_sz = seg_sizes[key];
				//
				_tiers[tier] = new LRU_cache(lru_region, max_obj_size, seg_sz, els_per_tier, _reserve_size, _Procs, _am_initializer, tier);
				_t_times[tier]._lb_time = _tiers[tier]->_lb_time;
				_t_times[tier]._ub_time = _tiers[tier]->_ub_time;
				// initialize hopscotch
				tier++;
				if ( tier > _NTiers ) break;
				_tiers[tier]->set_tier_table(_tiers);
			}
			//
			tier = 0;
			for ( auto p : hh_table_segs ) {
				//
				// key_t key = p.first;
				void *hh_region = p.second;
				// size_t seg_sz = seg_sizes[key];
				//
				LRU_cache *lru = _tiers[tier];
				if ( lru != nullptr ) {
					lru->set_hash_impl(hh_region,els_per_tier);
				}
				// initialize hopscotch
				tier++;
				if ( tier > _NTiers ) break;
			}
		}

		virtual ~TierAndProcManager() {}

	public:

		static uint32_t check_expected_com_region_size(uint32_t num_procs, uint32_t num_tiers) {
			//
			size_t tier_atomics_sz = NUM_ATOMIC_FLAG_OPS_PER_TIER*sizeof(atomic_flag *);  // ref to the atomic flag
			size_t proc_tier_com_sz = sizeof(Com_element)*num_procs;
			uint32_t predict = num_tiers*(tier_atomics_sz + proc_tier_com_sz);
			//
			return predict;
		} 

		// -- set_owner_proc_area
		// 		the com buffer is set separately outside the constructor... this may just be stylistic. 
		//		the com buffer services the acceptance of new data and the output of secondary processes.
		//
		bool		set_owner_proc_area(void) {
			if ( _com_buffer != nullptr ) {
				_owner_proc_area = (Com_element *)(_com_buffer + _NTiers*sizeof(atomic_flag *)) + (_proc*_NTiers);
			}
			return true;
		}


		// -- set_reader_atomic_tags
		void 		set_reader_atomic_tags() {
			if ( _com_buffer != nullptr ) {
				atomic_flag *af = (atomic_flag *)_com_buffer;
				for ( int i; i < _NTiers; i++ ) {
					_readerAtomicFlag[i] = af;
					af++;
				}
			}
		}


		// -- get_proc_entry
		Com_element	*get_proc_entries() {
			return (Com_element *)(_com_buffer + _NTiers*sizeof(atomic_flag *));
		}


	public:

		// -- set_and_init_com_buffer
		//		the com buffer is retains a set of values or each process 
		//		
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
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		LRU_cache	*access_tier(uint8_t tier) {
			if ( (0 <= tier) && (tier < MAX_TIERS) ) {
				return _tiers[tier];
			}
			return nullptr;
		}

		LRU_cache	*from_time(uint32_t timestamp) {
			uint32_t index = time_interval_b_search(timestamp, _t_times, _NTiers);
			if ( index < _NTiers ) {
				return _tiers[index];
			}
			return nullptr;
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		atomic<COM_BUFFER_STATE> *get_read_marker(uint8_t tier = 0) {
			Com_element *ce = (_owner_proc_area + tier);
			return &(ce->_marker);
		}

		Com_element *access_point(uint8_t tier = 0) {
			Com_element *ce = (_owner_proc_area + tier);
			return ce;
		}

		uint64_t	*get_hash_parameter(uint8_t tier = 0) {
			Com_element *ce = (_owner_proc_area + tier);
			return &(ce->_hash);
		}

		uint32_t	*get_offset_parameter(uint8_t tier = 0) {
			Com_element *ce = (_owner_proc_area + tier);
			return &(ce->_offset);
		}


		atomic<COM_BUFFER_STATE> *get_read_marker(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = ((Com_element *)_com_buffer) + (proc*_NTiers);
			Com_element *ce = (owner_proc_area + tier);
			return &(ce->_marker);
		}


		Com_element	*access_point(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = ((Com_element *)_com_buffer) + (proc*_NTiers);
			Com_element *ce = (owner_proc_area + tier);
			return ce;
		}

		uint64_t	*get_hash_parameter(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = ((Com_element *)_com_buffer) + (proc*_NTiers);
			Com_element *ce = (owner_proc_area + tier);
			return &(ce->_hash);
		}

		uint32_t	*get_offset_parameter(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = ((Com_element *)_com_buffer) + (proc*_NTiers);
			Com_element *ce = (owner_proc_area + tier);
			return &(ce->_offset);
		}


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



		// run_evictions

		bool		run_evictions(LRU_cache *lru,uint32_t source_tier,uint32_t ready_msg_count) {
			//
			// lru - is a source tier
			uint32_t req_count = lru->free_mem_requested();
			if ( req_count == 0 ) return true;	// for some reason this was invoked, but no one actually wanted free memory.
			//
			 	// if things have gone really wrong, then another process is busy copying old data 
				// out of this tier's data region. Stuff in reserve will end up in the primary (non-reserve) buffer.
				// The data is shifting out of spots from a previous eviction.  

			if ( lru->check_free_mem(ready_msg_count,false) ) return true;  // check if the situation changed on the way here

			//
			LRU_cache *next_tier = this->access_tier(source_tier+1);
			if ( next_tier == nullptr ) {
				// crisis mode...				elements will have to be discarded or sent to another machine
				lru->notify_evictor(UINT32_MAX);   // also copy data...
			} else {
				lru->transfer_hashes(next_tier,req_count);
			}
			//
			return true;
		}



		// Stop the process on a futex until notified...
		void		wait_for_data_present_notification(uint8_t tier) {
			_readerAtomicFlag[tier]->wait(false);  // this tier's LRU shares this read flag
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

		// At the app level obtain the LRU for the tier and work from there
		//
		int 		second_phase_write_handler(uint16_t proc_count, char **messages_reserved, char **duplicate_reserved, uint8_t assigned_tier = 0) {
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
				//  messages[ready_msg_count]->_offset
				//
				// 												WAIT FOR WORK

				wait_for_data_present_notification(assigned_tier);

				//
				//
				com_or_offset **messages = (com_or_offset **)messages_reserved;  // get this many addrs if possible...
				com_or_offset **accesses = (com_or_offset **)duplicate_reserved;
				//
				// 
				// FIRST: gather messages that are aready for addition to shared data structures.
				// 		OP on com buff
				//
				// Go through all the processes that might have written to this tier.
				// Here, the assigned tier provides the offset into the proc's tier entries.
				// Lock down the message written by the proc for the particular tier.. (often 0)
				//
				uint32_t ready_msg_count = 0;
				//
				for ( uint32_t proc = 0; (proc < proc_count); proc++ ) {
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
						ready_msg_count++;
						//
					}
					//
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
					while ( tmp_dups < end_dups ) {			/// update ops cleared first (nothing new in the hash table)
						com_or_offset *dup_access = *tmp_dups++;
						//
						// this element has been found and this is actually a data_loc...
						if ( dup_access->_offset != 0 ) {			// duplicated, an occupied location for the hash
							com_or_offset *to = *tmp;
							uint32_t data_loc = to->_offset;		// offset is in the messages buffer
							to->_offset = 0; 						// clear position
							Com_element *cel = dup_access->_cel;	// duplicate was not clear... ref to com element
							cel->_offset = data_loc;				// to the com element ... output the known offset
							// now get the control word location
							atomic<COM_BUFFER_STATE> *read_marker = &(cel->_marker);			// use the data location
							//  write data without creating a new hash entry.. (an update)
							clear_for_copy(read_marker);  // tells the requesting process to go ahead and write data.
						}
						tmp++;
					}
					//	additional_locations
					//		new items go into memory -- hence new allocation (or taking) of positions
					if ( additional_locations > 0 ) {  // new (additional) locations have been allocated 
						//
						// Is there enough memory?							--- CHECK FREE MEMORY
						// there is enough free memory even if this tier cuts into reserves.
						bool add = true;
						while ( !(lru->check_free_mem(additional_locations,add)) ) {
							// stop until there is space... getting to this point means 
							// that aggressive and immediate action has to be taken before proceeding.
							// A deterministic outcome is required.
							if ( !run_evictions(lru,assigned_tier,additional_locations) ) {
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
						//
						bool mem_claimed = (UINT32_MAX != lru->claim_free_mem(additional_locations,lru_element_offsets)); // negotiate getting a list from free memory
						//
						// if there are elements, they are already removed from free memory and this basket belongs to this process..
						if ( mem_claimed ) {
							//
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
									//
									// second phase writer hands the hash and offset to the lru
									//	this is the first time the lru pairs the hash and offset.
									// 	the lru calls upon the hash table to store the hash/offset pair...
									//
									if ( lru->store_in_hash(hash64,offset) != UINT64_MAX ) { // add to the hash table...
										write_offset_here[0] = offset;
										//
										atomic<COM_BUFFER_STATE> *read_marker = &(ce->_marker);
										clear_for_copy(read_marker);  // release the proc, allowing it to emplace the new data
									}
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
		 * Waking up any thread that waits on input into the tier.
		 * Any number of processes may place a message into a tier. 
		 * If the tier is full, the reader has the job of kicking off the eviction process.
		*/
		bool		wake_up_write_handlers(uint32_t tier) {
			_readerAtomicFlag[tier]->test_and_set();
			_readerAtomicFlag[tier]->notify_all();
			return true;
		}

		/**
		 * 
		*/
		void 		launch_second_phase_threads() {  // handle reads 
			//
			// uint16_t proc_count, char **messages_reserved, char **duplicate_reserved, uint8_t assigned_tier = 0
			// second_phase_write_handler(uint16_t proc_count, char **messages_reserved, char **duplicate_reserved, uint8_t assigned_tier = 0) 
			//
		}


		/**
		 *		put_method
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

		int 		put_method([[maybe_unused]] uint32_t process, uint32_t hash_bucket, uint32_t full_hash, bool updating, char* buffer, unsigned int size, uint32_t timestamp, uint32_t tier, void (delay_func)()) {
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
				hash_parameter[0] = hash_bucket; // put in the hash so that the read can see if this is a duplicate
				hash_parameter[1] = full_hash;
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
					uint8_t *m_insert = lru->data_location(write_offset);
					if ( m_insert != nullptr ) {
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

	protected:

		void					*_com_buffer;
		LRU_cache 				*_tiers[MAX_TIERS];		// local process storage
		Tier_time_bucket		_t_times[MAX_TIERS];	// shared mem storage
		uint16_t				_proc;					// calling proc index (assigned by configuration) (indicates offset in com buffer)
		Com_element				*_owner_proc_area;
		uint8_t					_reserve_size;

	protected:

	//
		atomic_flag 		*_readerAtomicFlag[MAX_TIERS];

};




#endif  // _H_HOPSCOTCH_HASH_LRU_