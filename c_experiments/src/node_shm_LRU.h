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


using namespace std;


//#include "hmap_interface.h"
#include "holey_buffer.h"
#include "atomic_proc_rw_state.h"


#include "time_bucket.h"


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


// five fences (??)
typedef struct COM_ELEMENT {
	atomic<COM_BUFFER_STATE>	_marker;	// control over accessing the message buffer
	uint8_t						_proc;		// the index of the owning proc
	uint8_t						_tier;		// the index of the managed tier (this com element)
	uint8_t						_ops;		// read/write/delete and other flags. 
	//
	uint64_t					_hash;		// the hash of the message
	uint32_t					_offset;	// offset to the LRU element... where data will be stored
	uint32_t					_timestamp;	// control over accessing the message buffer
	char						_message[MAX_MESSAGE_SIZE];   // 64*2
} Com_element;


//
//	LRU_cache --
//
//	Interleaved free memory is a stack -- fixed sized elements
//


class LRU_Consts {

	public: 

		LRU_Consts() {
			_status = true;
			_SUPER_HEADER = 0;
			_NTiers = 0;
			_INTER_PROC_DESCRIPTOR_WORDS = 0;		// initialized by exposed method called by coniguration.
		}

		virtual ~LRU_Consts() {}

	public:

		uint32_t			_SUPER_HEADER;
		uint32_t			_INTER_PROC_DESCRIPTOR_WORDS;
		uint32_t			_NTiers;
		uint32_t			_Procs;
		bool				_status;
		bool				_am_initializer;

		uint32_t 			_beyond_entries_for_tiers_and_mutex;

};


// class LRU_cache : public LRU_Consts {
// 	//
// 	public:
// 		// LRU_cache -- constructor

class LRU_cache : public LRU_Consts {

	public:
		// LRU_cache -- constructor
		LRU_cache(void *region,[[maybe_unused]] size_t record_size,[[maybe_unused]] size_t region_size,[[maybe_unused]] size_t reserve_size,[[maybe_unused]] bool am_initializer,[[maybe_unused]] uint16_t proc_max,[[maybe_unused]] uint8_t tier) {
			_Procs = proc_max;
			_Tier = tier;
			//
			_lb_time = (atomic<uint32_t> *)(region);   // these are governing time boundaries of the particular tier
			_ub_time = _lb_time + 1;
			_cascaded_com_area = (Com_element *)(_ub_time + 1);
			initialize_com_area(proc_max);
			_end_cascaded_com_area = _cascaded_com_area + _Procs;

			// time lower bound and upper bound for a tier...
			_lb_time->store(UINT32_MAX);
			_ub_time->store(UINT32_MAX);

		}

		virtual ~LRU_cache() {}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		uint8_t *start(void) {
			uint8_t *rr = (uint8_t *)(_end_cascaded_com_area);
			return rr;
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		void initialize_com_area(uint16_t proc_max) {
			Com_element *proc_entry = _cascaded_com_area;
			uint32_t P = proc_max;
			for ( uint32_t p = 0; p < P; p++ ) {
				proc_entry->_marker.store(CLEAR_FOR_WRITE);
				proc_entry->_hash = 0L;
				proc_entry->_offset = UINT32_MAX;
				proc_entry->_timestamp = 0;
				proc_entry->_tier = _Tier;
				proc_entry->_proc = p;
				proc_entry->_ops = 0;
				memset(proc_entry->_message,0,MAX_MESSAGE_SIZE);
				proc_entry++;
			}
		}

		void initialize_header_sizes(uint32_t super_header_size,uint32_t N_tiers,uint32_t words_for_mutex_and_conditions) {
			_SUPER_HEADER = super_header_size;
			_NTiers = N_tiers;
			_INTER_PROC_DESCRIPTOR_WORDS = words_for_mutex_and_conditions;		// initialized by exposed method called by coniguration.
			_beyond_entries_for_tiers_and_mutex = (_SUPER_HEADER*_NTiers);
		}

		/**
		 * Prior to attachment, the required space availability must be checked.
		*/
		void attach_to_lru_list([[maybe_unused]] uint32_t *lru_element_offsets,[[maybe_unused]] uint32_t ready_msg_count) {
			//
		}

		uint32_t		free_mem_requested(void) {
			return 0;
		}

		uint32_t claim_free_mem([[maybe_unused]] uint32_t ready_msg_count,[[maybe_unused]] uint32_t *reserved_offsets) {
			return 0;
		}

		void			wait_for_reserves([[maybe_unused]] uint32_t req_count) {}

		bool			has_reserve(void) { return false; }

		bool			check_free_mem([[maybe_unused]] uint32_t msg_count,[[maybe_unused]] bool add) {
			return false;
		}

		bool 			transfer_out_of_tier(void) { return false; }

		void 			move_from_reserve_to_primary(void) { }

		void 			from_reserve([[maybe_unused]] list<LRU_element *> &free_reserve,[[maybe_unused]] uint32_t req_count) {}

		void			return_to_free_mem([[maybe_unused]] LRU_element *el) {}

		uint32_t		timeout_table_evictions([[maybe_unused]] list<uint32_t> &moving,[[maybe_unused]] uint32_t req_count) { return 0; }

		void 			claim_hashes([[maybe_unused]] list<uint32_t> &moving) {}

		void			relinquish_hashes([[maybe_unused]] list<uint32_t> &moving) {}

		uint32_t		filter_existence_check([[maybe_unused]] char **messages,[[maybe_unused]] char **accesses,[[maybe_unused]] uint32_t ready_msg_count) { return 0; }

		bool			add_key_value([[maybe_unused]] uint64_t hash,[[maybe_unused]] uint32_t offset) {
			return true; // faux success
		}

		uint8_t 		*data_location(uint32_t write_offset) {
			uint8_t *strt = this->start();
			if ( strt != nullptr ) {
				return (this->start() + write_offset);
			}
			return nullptr;
		}


		void							*_region;
		//
		uint8_t							_Tier;
		//
		atomic<uint32_t>				*_lb_time;
		atomic<uint32_t>				*_ub_time;
		Com_element						*_cascaded_com_area;   // if outsourcing tiers ....
		Com_element						*_end_cascaded_com_area;

};




template<const uint8_t MAX_TIERS = 8>
class TierAndProcManager : public LRU_Consts {

	public:

		// regions -- the regions are shared memory (yet governed by a processes)
		// sometimes processes will cross over in accessing each other's assigned region...

		TierAndProcManager(void *regions[MAX_TIERS], size_t rc_sz, size_t seg_sz, size_t reserve_size, uint16_t proc_max, bool am_initializer, uint16_t proc_num, uint8_t num_tiers_in_use, uint32_t SUPER_HEADER, uint32_t INTER_PROC_DESCRIPTOR_WORDS) {
			//
			_am_initializer = am_initializer; // need to keep around for downstream initialization
			//
			_Procs = proc_max;
			_proc = proc_num;
			_NTiers = num_tiers_in_use;
			//
			for ( uint8_t i = 0; i < num_tiers_in_use; i++ ) {
				// seg_sz -> region_size
				//    am_initializer -- either read or set the initializer
				//
				_tiers[i] = new LRU_cache(regions[i], rc_sz, seg_sz, reserve_size, proc_max, am_initializer, i);
				//
				//
				_tiers[i]->initialize_header_sizes(SUPER_HEADER,num_tiers_in_use,INTER_PROC_DESCRIPTOR_WORDS);
				_t_times[i]._lb_time = _tiers[i]->_lb_time;
				_t_times[i]._ub_time = _tiers[i]->_ub_time;
			}
			_com_buffer = nullptr;   // the com buffer is another share section.. separate from the shared data regions
		}

		// -- set_com_buffer
		// 		the com buffer is set separately outside the constructor... this may just be stylistic. 
		//		the com buffer services the acceptance of new data and the output of secondary processes.
		//
		bool		set_com_buffer(void *com_buffer) {
			if ( com_buffer == nullptr ) return false;
			_com_buffer = com_buffer;
			_owner_proc_area = ((Com_element *)_com_buffer) + (_proc*_NTiers);
			return true;
		}

		// -- set_and_init_com_buffer
		//		the com buffer is retains a set of values or each process 
		//		
		bool 		set_and_init_com_buffer(void *com_buffer) {
			if ( set_com_buffer(com_buffer) && _am_initializer ) {
				// _com_buffer is now pointing at the com_buffer... but it has not been essentially formatted.
				Com_element *proc_entry = (Com_element *)(com_buffer);
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

		bool 		set_reader_atomic_tags() {
			if ( _com_buffer != nullptr ) {
				atomic_flag *af = (atomic_flag *)_com_buffer;
				for ( int i; i < _NTiers; i++ ) {
					_readerAtomicFlag[i] = af;
					af++;
				}
				return true;
			}
			return false;
		}

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

		uint32_t	*get_hash_parameter(uint8_t tier = 0) {
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

		uint32_t	*get_hash_parameter(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = ((Com_element *)_com_buffer) + (proc*_NTiers);
			Com_element *ce = (owner_proc_area + tier);
			return &(ce->_hash);
		}

		uint32_t	*get_offset_parameter(uint8_t proc, uint8_t tier = 0) {
			Com_element *owner_proc_area = ((Com_element *)_com_buffer) + (proc*_NTiers);
			Com_element *ce = (owner_proc_area + tier);
			return &(ce->_offset);
		}

		// TierAndProcManager
		bool run_evictions(LRU_cache *lru,uint32_t source_tier,uint32_t ready_msg_count) {
			//
			// lru - is a source tier
			uint32_t req_count = lru->free_mem_requested();
			if ( req_count == 0 ) return true;	// for some reason this was invoked, but no one actually wanted free memory.
			//
			 	// if things have gone really wrong, then another process is busy copying old data 
				// out of this tier's data region. Stuff in reserve will end up in the primary (non-reserve) buffer.
				// The data is shifting out of spots from a previous eviction.  
			while  ( !(lru->has_reserve()) ) {
				lru->wait_for_reserves(req_count);
			}
			//
			if ( lru->check_free_mem(ready_msg_count,false) ) return true;

			//
			LRU_cache *next_tier = this->access_tier(source_tier+1);
			if ( next_tier == nullptr ) {
				// crisis mode...				elements will have to be discarded or sent to another machine
				return lru->transfer_out_of_tier();   // also copy data...
			} else {
				//
				// use a secondary free list for new req_count elements 
				// and at the same time, yield the old position to a second tier that shares this tier's primary.
				list<LRU_element *> free_reserve;
				lru->from_reserve(free_reserve,req_count);
				//
				for ( LRU_element *reserve_el : free_reserve  ) {
					lru->return_to_free_mem(reserve_el);
				}
				//
				list<uint32_t> moving;
				uint32_t count_reclaimed_stamps = lru->timeout_table_evictions(moving,req_count);
				next_tier->claim_hashes(moving);
				lru->relinquish_hashes(moving);
				// have to wakeup a secondary process that will move data from reserve to primary
				// and move relinquished data to the secondary... (free up reserve again... need it later)
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

				// Stop the process on a futex until notified...
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
					atomic<COM_BUFFER_STATE> *read_marker = this->get_read_marker(proc, assigned_tier);
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

					/// questionable   DUBIOUS
					//
					while ( accesses < end_dups ) {
						char *dup_access = *accesses++;
						if ( dup_access != nullptr ) {
							uint32_t data_loc = (uint32_t)(tmp[0]);
							tmp[0] = nullptr;
							uint32_t *write_offset_here = dup_access;
							write_offset_here[0] = data_loc;
							// now get the control word location
							Com_element *ce = (Com_element *)(access_point);
							atomic<COM_BUFFER_STATE> *read_marker = &(ce->_marker);
							//
							clear_for_copy(read_marker);  // tells the requesting process to go ahead and write data.
						}
						tmp++;
					}
					//
					if ( additional_locations > 0 ) {
						//
						// Is there enough memory?							--- CHECK FREE MEMORY
						bool add = true;
						while ( !(lru->check_free_mem(ready_msg_count,add)) ) {
							if ( !run_evictions(lru,assigned_tier,ready_msg_count) ) {
								return(-1);
							}
							add = false;  // this process should not add the same amount to the global free mem request more than once.
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
							uint8_t *start = lru->start();
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
									Com_element *ce = (Com_element *)(access_point);

									uint32_t *write_offset_here = (&ce->_offset);
									uint64_t *hash_parameter =  (&ce->_hash);

									uint64_t hash64 = hash_parameter[0];
									//
									if ( lru->add_key_value(hash64,offset) ) { // add to the hash table...
										write_offset_here[0] = offset;
										//
										atomic<COM_BUFFER_STATE> *read_marker = &(ce->_marker);
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

			return(-1);
		}


		/**
		 * Waking up any thread that waits on input into the tier.
		 * Any number of processes may place a message into a tier. 
		 * If the tier is full, the reader has the job of kicking off the eviction process.
		*/
		bool wake_up_readers(uint32_t tier) {
			_readerAtomicFlag[tier]->test_and_set();
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
			// particular atomics
			atomic<COM_BUFFER_STATE> *read_marker =	this->get_read_marker();
			uint32_t *hash_parameter = this->get_hash_parameter();
			uint32_t *offset_offset = this->get_offset_parameter();

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

		uint32_t raise_lru_lb_time_bounds(uint32_t lb_timestamp) {
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
		}

	protected:

		void					*_com_buffer;
		LRU_cache 				*_tiers[MAX_TIERS];		// local process storage
		Tier_time_bucket		_t_times[MAX_TIERS];	// shared mem storage
		uint16_t				_proc;					// calling proc index (assigned by configuration) (indicates offset in com buffer)
		Com_element				*_owner_proc_area;

	protected:

	//
		atomic_flag 		*_readerAtomicFlag[MAX_TIERS];


};




#endif  // _H_HOPSCOTCH_HASH_LRU_