#include "node_shm.h"

//-------------------------------
using namespace Nan;


// In this module, the original 'typed array' module will not be shadowed
//-------------------------------

//-------------------------------

namespace node {
namespace node_shm {

	using node::AtExit;
	using v8::Local;
	using v8::Number;
	using v8::Object;
	using v8::Value;


	map<key_t,LRU_cache *>		g_LRU_caches_per_segment;
	map<key_t,HH_map *>			g_HMAP_caches_per_segment;
	map<key_t,MutexHolder *>	g_MUTEX_per_segment;

	void *g_com_buffer = NULL;



	class SharedSegmentsManager {

		public:

			SharedSegmentsManager() {}
			virtual ~SharedSegmentsManager() {}

		public:

			/**
			 * shm_getter
			*/
			int shm_getter(key_t key, int at_shmflg,  int shmflg = 0, bool isCreate = false, size_t size = 0) {
				//
				int res_id = shmget(key, size, shmflg);
				//
				if ( res_id == -1 ) {
					switch(errno) {
						case EEXIST: // already exists
						case EIDRM:  // scheduled for deletion
						case ENOENT: // not exists
							return -1;
						case EINVAL: // should be SHMMIN <= size <= SHMMAX
							return -2;
						default:
							return -2;  // tells caller to get the errno
					}
				} else {
					//
					if ( !isCreate ) {		// means to attach.... 
						//
						struct shmid_ds shminf;
						//
						err = shmctl(res_id, IPC_STAT, &shminf);
						if ( err == 0 ) {
							size = shminf.shm_segsz;   // get the seg size from the system
							_ids_to_seg_sizes[key] = size;
						} else {
							return return -2;							
						}
					}
					//
					void* res = shmat(resId, NULL, at_shmflg);
					//
					if ( res == (void *)-1 ) return -2;
					//
					_seg_to_addrs[key] = res;
				}
				
				return 0;
			}

			/**
			 * get_seg_size
			*/
			size_t get_seg_size(key_t key) {
				int resId = shmget(key, 0, 0);
				if (resId == -1) {
					switch(errno) {
						case ENOENT: // not exists
						case EIDRM:  // scheduled for deletion
							info.GetReturnValue().Set(Nan::New<Number>(-1));
							return;
						default:
							return Nan::ThrowError(strerror(errno));
					}
				}
				struct shmid_ds shminf;
				size_t seg_size;
				//
				err = shmctl(res_id, IPC_STAT, &shminf);
				if ( err == 0 ) {
					seg_size = shminf.shm_segsz;   // get the seg size from the system
					_ids_to_seg_sizes[key] = seg_size;
				} else {
					return return -2;							
				}
				return seg_size;
			}


			/**
			 * _detach_op
			*/
			int _detach_op(key_t key, bool force, bool onExit) {
				//
				int resId = this->key_to_id(key);
				if ( resId < 0 ) return resId;
				//

				void *addr = _seg_to_addrs[key];
				struct shmid_ds shminf;
				int err = shmdt(addr);
				if ( err ) {
					if ( !(onExit) ) return -2;
					return err;
				}
					//get stat
				err = shmctl(resId, IPC_STAT, &shminf);
				if ( err ) {
					if ( !(onExit) ) return -2;
					return err;
				}
				//destroy if there are no more attaches or force==true
				if ( force || shminf.shm_nattch == 0 ) {
					//
					err = shmctl(resId, IPC_RMID, 0);
					if ( err ) {
						if ( !(onExit) ) return -2;
						return err;
					}
					//
					delete _seg_to_addrs[key];
					delete _ids_to_seg_sizes[key];
					//
					return 0;
				} else {
					return shminf.shm_nattch; //detached, but not destroyed
				}
				return -1;
			}


			size_t detach(key_t key,bool forceDestroy) {
				//
				int status = this->_detach_op(key,forceDestroy);
				if ( status == 0 ) {
					this->remove_if_lru(key);
					this->remove_if_hh_map(key);
					this->remove_if_com_buffer(key);
					return _seg_to_addrs.size();
				}
				//
				return status;
			}


			pair<uint16_t,size_t> detach_all(bool forceDestroy = false) {
				unsigned int deleted = 0;
				size_t total_freed = 0;
				for ( auto p : _ids_to_seg_sizes ) {
					key_t key = p.first;
					if ( this->detach(key,forceDestroy) == 0 ) {
						deleted++;
						total_freed += p.second;
					}
				}
				return pair<uint16_t,size_t>(deleted,total_freed);
			}


			void remove_if_lru(key_t key) {}
			void remove_if_hh_map(key_t key) {}
			void remove_if_com_buffer(key_t key) {}


			/**
			 * key_to_id
			*/
			int key_to_id(key_t key) {
				int resId = shmget(key, 0, 0);
				if ( resId == -1 ) {
					switch(errno) {
						case ENOENT: // not exists
						case EIDRM:  // scheduled for deletion
							return(-1);
						default:
							return(-1);
					}
				}
				return resId;
			}


			size_t total_mem_allocated(void) {
				size_t total_mem = 0;
				for ( auto p : _ids_to_seg_sizes ) {
					total_mem += p.second;
				}
				return total_mem;
			}

			bool check_key(key_t key) {
				if ( find(_seg_to_addrs.begin(),_seg_to_addrs.end(),key) != _seg_to_addrs.end() ) return true;
				int resId  this->key_to_id(key);
				if ( resId == -1 ) { return false; }
				return true;
			}



			void *get_addr(key_t key) {
				auto seg = _seg_to_addrs[key];
				return seg;
			}
			

			
	public:
//--
			map<key_t,void *>					_seg_to_addrs;
			map<key_t,size_t> 					_ids_to_seg_sizes;


	};

	//
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- 

	SharedSegmentsManager *g_segments_manager = nullptr;


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	static void Init(Local<Object> target);
	static void AtNodeExit(void*);
	//

	// /proc/sys/kernel/shmmni


	// Init info arrays
	static void initShmSegmentsInfo() {
		if ( g_segments_manager != nullptr ) {
			delete g_segments_manager;
		}
		try {
			g_segments_manager = new SharedSegmentsManager();
		} catch (std::error_code e) {
			g_segments_manager = nullptr;
		}
	}




	NAN_METHOD(shm_get) {
		Nan::HandleScope scope;
		// parameters
		key_t key 			= Nan::To<uint32_t>(info[0]).FromJust(); // 0
		size_t seg_size 	= Nan::To<uint32_t>(info[1]).FromJust(); // 1
		int shmflg 			= Nan::To<uint32_t>(info[2]).FromJust(); // 2
		int at_shmflg 		= Nan::To<uint32_t>(info[3]).FromJust(); // 3
		bool is_creator 	= Nan::To<bool>(info[4]).FromJust();	 // 4     // 5 parameters
		//  parameters

		int status = 0;
		if ( g_segments_manager != nullptr ) {   // should be initialized...
			if ( is_creator ) {
			 	status = g_segments_manager->shm_getter(key, at_shmfl, shmflg, true, seg_size);
			} else {
			 	status = g_segments_manager->shm_getter(key, at_shmflg);
			}
		} else status = -1;

		if ( status != 0 ) {		// error
			status = -status;
			switch ( status ) {
				case 1 : {
					info.GetReturnValue().SetNull();
					return;
				}
				case 2 :
				default: {
					return Nan::ThrowRangeError(strerror(errno));
				}
			}
		}
		// no error
		//
		size_t seg_size =  g_segments_manager->_ids_to_seg_sizes[key];
		info.GetReturnValue().Set(Nan::New<Number>(seg_size));
	}



	NAN_METHOD(getSegSize) {
		Nan::HandleScope scope;
		//
		key_t key 			= Nan::To<uint32_t>(info[0]).FromJust(); // 0  // 1 parameter
		//
		if ( g_segments_manager != nullptr ) {
			size_t seg_size =  g_segments_manager->_ids_to_seg_sizes[key];
			if ( seg_size != 0 ) {
				info.GetReturnValue().Set(Nan::New<Number>(seg_size));
			} else {
				seg_size = g_segments_manager->get_seg_size(key);
				info.GetReturnValue().Set(Nan::New<Number>(seg_size));
			}
		} else {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
		}
	}


	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	NAN_METHOD(detach) {
		Nan::HandleScope scope;
		//
		if ( info.Length() < 1 ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		key_t key 			= Nan::To<uint32_t>(info[0]).FromJust(); // 0
		bool forceDestroy	= false;								 // 1   // 2 parameters
		if ( info.Length() > 1 ) {
			forceDestroy = Nan::To<bool>(info[1]).FromJust();	 // 1   // 2 parameters
		}
		//
		int stats_or_count = -1;
		if ( g_segments_manager != nullptr ) {
			stats_or_count = g_segments_manager->detach(key,forceDestroy);
		}
		info.GetReturnValue().Set(Nan::New<Number>(stats_or_count));
	}


	NAN_METHOD(detachAll) {
		bool forceDestroy	= false;								 // 1   // 2 parameters
		if ( info.Length() > 0 ) {
			forceDestroy = Nan::To<bool>(info[0]).FromJust();	 // 1   // 2 parameters
		}
		//
		uint16_t cnt = 0;
		if ( g_segments_manager != nullptr ) {
			pair<uint16_t,size_t> p = g_segments_manager->detach_all(forceDestroy);
			cnt = p.first;
		}
		initShmSegmentsInfo();
		info.GetReturnValue().Set(Nan::New<Number>(cnt));
	}

	NAN_METHOD(getTotalSize) {
		if ( g_segments_manager != nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(g_segments_manager->total_mem_allocated()));
		}
	}

	// node::AtExit
	static void AtNodeExit(void*) {
		if ( g_segments_manager != nullptr ) {
			pair<uint16_t,size_t> p = g_segments_manager->detach_all(false);
			cnt = p.first;
		}
	}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	const uint32_t keyMin = 1;
	const uint32_t keyMax = UINT32_MAX - keyMin;
	const uint32_t lengthMin = 1;
	const uint32_t lengthMax = UINT16_MAX;   // for now

	NAN_METHOD(create) {
		Nan::HandleScope scope;
		//
		if ( info.Length() < 2 ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		key_t key 			= Nan::To<uint32_t>(info[0]).FromJust(); // 0
		uint32_t count 		= Nan::To<uint32_t>(info[1]).FromJust(); // 1  // 2 parameters
		//
		if ( key < keyMin || key >= keyMax ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		size_t seg_size = count*sizeof(uint32_t);
		if ( seg_size < lengthMin || seg_size >= lengthMax ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		if ( g_segments_manager != nullptr ) {		// initialized

			auto perm = 0660;
			//
			int at_shmflg = 0;
			int shmflg = IPC_CREAT | IPC_EXCL | perm;
			//
			int status = g_segments_manager->shm_getter(key, at_shmfl, shmflg, true, seg_size);
			//
			if ( status != 0 ) {		// error
				status = -status;
				switch ( status ) {
					case 1 : {
						info.GetReturnValue().SetNull();
						return;
					}
					case 2 :
					default: {
						return Nan::ThrowRangeError(strerror(errno));
					}
				}
			}
			//
			size_t seg_size =  g_segments_manager->_ids_to_seg_sizes[key];
			info.GetReturnValue().Set(Nan::New<Number>(seg_size));
		} else {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
		}
	}


	NAN_METHOD(attach) {
		Nan::HandleScope scope;
		//
		if ( info.Length() < 2 ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		key_t key 			= Nan::To<uint32_t>(info[0]).FromJust(); // 0
		if ( key < keyMin || key >= keyMax ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		if ( g_segments_manager != nullptr ) {		// initialized
			//
			int at_shmflg = 0;
			int shmflg = 0;
			//
			int status = g_segments_manager->shm_getter(key, at_shmfl);
			//
			if ( status != 0 ) {		// error
				status = -status;
				switch ( status ) {
					case 1 : {
						info.GetReturnValue().SetNull();
						return;
					}
					case 2 :
					default: {
						return Nan::ThrowRangeError(strerror(errno));
					}
				}
			}
			//
			size_t seg_size =  g_segments_manager->_ids_to_seg_sizes[key];
			info.GetReturnValue().Set(Nan::New<Number>(seg_size));
		} else {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
		}
	}



// class SHM_CLASS {


// #include <stdio.h>                                                                                                                  

// #define SHMMAX_SYS_FILE "/proc/sys/kernel/shmmax"

// int main(int argc, char **argv)
// {
//     unsigned int shmmax;
//     FILE *f = fopen(SHMMAX_SYS_FILE, "r");

//     if (!f) {
//         fprintf(stderr, "Failed to open file: `%s'\n", SHMMAX_SYS_FILE);
//         return 1;
//     }

//     if (fscanf(f, "%u", &shmmax) != 1) {
//         fprintf(stderr, "Failed to read shmmax from file: `%s'\n", SHMMAX_SYS_FILE);
//         fclose(f);
//         return 1;
//     }

//     fclose(f);

//     printf("shmmax: %u\n", shmmax);

//     return 0;
// }










	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	// fixed size data elements 
	NAN_METHOD(initLRU) {
		Nan::HandleScope scope;
		key_t key = 			Nan::To<uint32_t>(info[0]).FromJust();
		size_t rc_sz = 			Nan::To<uint32_t>(info[1]).FromJust();
		size_t seg_sz = 		Nan::To<uint32_t>(info[2]).FromJust();
		bool am_initializer = 	Nan::To<bool>(info[3]).FromJust();
		//
		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		int resId = g_segments_manager->key_to_id(key);
		if ( resId != - 0 ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//		
		LRU_cache *plr = g_LRU_caches_per_segment[key];
		//
		if ( g_segments_manager->check_key(key) ) {
			if ( plr != nullptr ) {
				info.GetReturnValue().Set(Nan::New<Number>(plr->max_count()));
			} else {
				void *region = g_segments_manager->get_addr(key);
				if ( region == nullptr ) {
					info.GetReturnValue().Set(Nan::New<Number>(-1));
					return;
				}
				//
				size_t rec_size = rc_sz;
				size_t seg_size = g_ids_to_seg_sizes[resId];
				seg_size = ( seg_size > seg_sz ) ? seg_sz : seg_size;
				g_ids_to_seg_sizes[resId] = seg_size;

				LRU_cache *lru_cache = new LRU_cache(region,rec_size,seg_size,am_initializer);
				if ( lru_cache->ok() ) {
					g_LRU_caches_per_segment[key] = lru_cache;
					info.GetReturnValue().Set(Nan::New<Number>(lru_cache->max_count()));
				} else {
					return Nan::ThrowError("Bad parametes for initLRU");
				}
			}
		} else {
			if ( plr != nullptr ) {
				g_LRU_caches_per_segment.erase(key);
			}
			info.GetReturnValue().Set(Nan::New<Number>(-1));
		}
	}


	
	// fixed size data elements 
	NAN_METHOD(initHopScotch) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		key_t lru_key =  Nan::To<uint32_t>(info[1]).FromJust();
		bool am_initializer = Nan::To<bool>(info[2]).FromJust();
		size_t max_element_count = Nan::To<uint32_t>(info[3]).FromJust();
		//
		//
		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		int resId = shmget(key, 0, 0);
		if (resId == -1) {
			switch(errno) {
				case ENOENT: // not exists
				case EIDRM:  // scheduled for deletion
					info.GetReturnValue().Set(Nan::New<Number>(-1));
					return;
				default:
					return Nan::ThrowError(strerror(errno));
			}
		}
		//
		HH_map *phm = g_HMAP_caches_per_segment[key];
		//
		if ( shmCheckKey(key) ) {
			if ( phm != nullptr ) {
				info.GetReturnValue().Set(Nan::New<Number>(key));
			} else {
				void *region = g_segments_manager->get_addr(key);
				if ( region == nullptr ) {
					info.GetReturnValue().Set(Nan::New<Number>(-1));
					return;
				}
				//
				HH_map *hmap = new HH_map(region,max_element_count,am_initializer);
				if ( hmap->ok() ) {
					g_HMAP_caches_per_segment[key] = hmap;
					// assign this HH_map to an LRU
					LRU_cache *lru_cache = g_LRU_caches_per_segment[lru_key];
					if ( lru_cache == nullptr ) {
						if ( shmCheckKey(key) ) {
							info.GetReturnValue().Set(Nan::New<Boolean>(false));
						} else {
							info.GetReturnValue().Set(Nan::New<Number>(-2));
						}
					} else {
						lru_cache->set_hash_impl(hmap);
						info.GetReturnValue().Set(Nan::New<Boolean>(true));
					}
					//
				} else {
					return Nan::ThrowError("Bad parametes for initLRU");
				}
				//
			}

		} else {
			if ( phm != nullptr ) {
				g_HMAP_caches_per_segment.erase(key);
			}
			info.GetReturnValue().Set(Nan::New<Number>(-1));
		}
	}




	NAN_METHOD(getMaxCount) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			uint32_t maxcount = lru_cache->max_count();
			info.GetReturnValue().Set(Nan::New<Number>(maxcount));
		}
	}

	NAN_METHOD(getCurrentCount) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			uint32_t count = lru_cache->current_count();
			info.GetReturnValue().Set(Nan::New<Number>(count));
		}
	}


	NAN_METHOD(getFreeCount) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			uint32_t count = lru_cache->free_count();
			info.GetReturnValue().Set(Nan::New<Number>(count));
		}
	}


	// time_since_epoch
	// helper to return the time in milliseconds
	NAN_METHOD(time_since_epoch) {
		Nan::HandleScope scope;
		uint64_t epoch_time;
		epoch_time = epoch_ms();
		info.GetReturnValue().Set(Nan::New<Number>(epoch_time));
	}


	// set el -- add a new entry to the LRU.  IF the LRU is full, return indicative value.
	//
	NAN_METHOD(set_el)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();				// key to the shared memory segment
		uint32_t hash_bucket = Nan::To<uint32_t>(info[1]).FromJust();	// hash modulus the number of buckets (hence a bucket)
		uint32_t full_hash = Nan::To<uint32_t>(info[2]).FromJust();		// hash of the value
		Utf8String data_arg(info[3]);
		//
		// originally full_hash is the whole 32 bit hash and hash_bucket is the modulus of it by the number of buckets
		// uint64_t hash64 = (((uint64_t)full_hash << HALF) | (uint64_t)hash_bucket);
		//
		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		// First check to see if a buffer was every allocated
		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {		// buffer was not set yield an error
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			char *data = *data_arg;
			// is the key already assigned ?  >> check_for_hash 
			uint32_t offset = lru_cache->check_for_hash(full_hash,hash_bucket);
			if ( offset == UINT32_MAX ) {  // no -- go ahead and add a new element  >> add_el
				offset = lru_cache->add_el(data,full_hash,hash_bucket);
				if ( offset == UINT32_MAX ) {
					info.GetReturnValue().Set(Nan::New<Boolean>(false));
				} else {
					info.GetReturnValue().Set(Nan::New<Number>(offset));
				}
			} else {
				// there is already data -- so attempt ot update the element with new data.
				if ( lru_cache->update_el(offset,data) ) {
					info.GetReturnValue().Set(Nan::New<Number>(offset));
				} else {
					info.GetReturnValue().Set(Nan::New<Boolean>(false));
				}
			}
		}
	}


	// set_many -- add list of new entries to the LRU.  Return the results of adding.
	//
	NAN_METHOD(set_many)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		Local<Array> jsArray = Local<Array>::Cast(info[1]);

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		//
		// First check to see if a buffer was every allocated
		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {		// buffer was not set yield an error
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			// important -- the code is only really simple to write if v8 is used straightup.
			// nan will help get the context -- use v8 get and set with the context
			v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
			//
			uint16_t n = jsArray->Length();
			Local<Array> jsArrayResults = Nan::New<Array>(n);
			//
//cout << "N " << n << endl;
			//
			for (uint16_t i = 0; i < n; i++) {
				Local<v8::Array> jsSubArray = Local<Array>::Cast(jsArray->Get(context, i).ToLocalChecked());
        		uint32_t hash = jsSubArray->Get(context, 0).ToLocalChecked()->Uint32Value(context).FromJust();
        		uint32_t index = jsSubArray->Get(context, 1).ToLocalChecked()->Uint32Value(context).FromJust();
				Utf8String data_arg(jsSubArray->Get(context, 2).ToLocalChecked());
				uint64_t hash64 = (((uint64_t)index << HALF) | (uint64_t)hash);
				char *data = *data_arg;
	//cout << data << endl;
				// is the key already assigned ?  >> check_for_hash 
				uint32_t offset = lru_cache->check_for_hash(hash64);
				if ( offset == UINT32_MAX ) {  // no -- go ahead and add a new element  >> add_el
					offset = lru_cache->add_el(data,hash64);
					if ( offset == UINT32_MAX ) {
						Nan::Set(jsArrayResults, i, Nan::New<Boolean>(false));
					} else {
						Nan::Set(jsArrayResults, i, Nan::New<Number>(offset));
					}
				} else {
					// there is already data -- so attempt ot update the element with new data.
					if ( lru_cache->update_el(offset,data) ) {
						Nan::Set(jsArrayResults, i, Nan::New<Number>(offset));
					} else {
						Nan::Set(jsArrayResults, i, Nan::New<Boolean>(false));
					}
				}
				//
			}
			info.GetReturnValue().Set(jsArrayResults);
		}
	}


	NAN_METHOD(get_el)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		key_t index = Nan::To<uint32_t>(info[1]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			char data[lru_cache->record_size()];
			uint8_t rslt = lru_cache->get_el(index,data);
			if ( rslt == 0 || rslt == 1 ) {
				if ( rslt == 0 ) {
					info.GetReturnValue().Set(New(data).ToLocalChecked());
				} else {
					string fix_data = strdup(data);
					string prefix = "DELETED: ";
					fix_data = prefix + fix_data;
					memset(data,0,lru_cache->record_size());
					memcpy(data,fix_data.c_str(),
										min( fix_data.size(), (lru_cache->record_size() - 1)) );
					info.GetReturnValue().Set(New(data).ToLocalChecked());
				}
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-2));
			}
		}
	}


	NAN_METHOD(get_el_hash)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		uint32_t hash = Nan::To<uint32_t>(info[1]).FromJust();
		uint32_t index = Nan::To<uint32_t>(info[2]).FromJust();
		//
		uint64_t hash64 = (((uint64_t)index << HALF) | (uint64_t)hash);

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
//cout << "get h> " << hash << " i> " << index << " " << hash64 << endl;
		//
		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			uint32_t index = lru_cache->check_for_hash(hash64);
			if ( index == UINT32_MAX ) {
				info.GetReturnValue().Set(Nan::New<Number>(-2));
			} else {
				char data[lru_cache->record_size()];
				uint8_t rslt = lru_cache->get_el(index,data);
				if ( rslt == 0 ) {
					if ( rslt == 0 ) {
						info.GetReturnValue().Set(New(data).ToLocalChecked());
					}
				} else {
					info.GetReturnValue().Set(Nan::New<Number>(-2));
				}
			}
		}
	}



	NAN_METHOD(del_el)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		key_t index = Nan::To<uint32_t>(info[1]).FromJust();
		//
		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			if ( lru_cache->del_el(index) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(true));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-2));
			}
		}
	}

	NAN_METHOD(del_key)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		uint32_t hash = Nan::To<uint32_t>(info[1]).FromJust();		// bucket index
		uint32_t full_hash = Nan::To<uint32_t>(info[2]).FromJust();
		uint64_t hash64 = (((uint64_t)full_hash << HALF) | (uint64_t)hash);
		//

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			uint32_t index = lru_cache->check_for_hash(hash64);
			if ( index == UINT32_MAX ) {
				info.GetReturnValue().Set(Nan::New<Number>(-2));
			} else {
				if ( lru_cache->del_el(index) ) {
					info.GetReturnValue().Set(Nan::New<Boolean>(true));
				} else {
					info.GetReturnValue().Set(Nan::New<Number>(-2));
				}
			}

		}
	}

	NAN_METHOD(remove_key)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		uint32_t hash = Nan::To<uint32_t>(info[1]).FromJust();
		uint32_t index = Nan::To<uint32_t>(info[2]).FromJust();
		uint64_t hash64 = (((uint64_t)index << HALF) | (uint64_t)hash);
		//

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			lru_cache->remove_key(hash64);
			info.GetReturnValue().Set(Nan::New<Boolean>(true));
		}
	}

	NAN_METHOD(set_share_key)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		key_t index = Nan::To<uint32_t>(info[1]).FromJust();
		uint32_t share_key = Nan::To<uint32_t>(info[2]).FromJust();
		//
		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			if ( lru_cache->set_share_key(index,share_key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(true));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-2));
			}
		}
	}

	NAN_METHOD(get_last_reason)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		//
		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			const char *reason = lru_cache->get_last_reason();
			info.GetReturnValue().Set(New(reason).ToLocalChecked());
		}
	}

	NAN_METHOD(reload_hash_map)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		//

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			lru_cache->reload_hash_map();
			info.GetReturnValue().Set(Nan::New<Boolean>(true));
		}
	}

	NAN_METHOD(reload_hash_map_update)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		uint32_t share_key = Nan::To<uint32_t>(info[0]).FromJust();
		//

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			lru_cache->reload_hash_map_update(share_key);
			info.GetReturnValue().Set(Nan::New<Boolean>(true));
		}
	}

	NAN_METHOD(run_lru_eviction)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		time_t cutoff = Nan::To<uint32_t>(info[1]).FromJust();
		uint32_t max_evict_b = Nan::To<uint32_t>(info[2]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];

		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			list<uint64_t> evict_list;
			uint8_t max_evict = (uint8_t)(max_evict_b);
			time_t time_shift = epoch_ms();
			time_shift -= cutoff;
			lru_cache->evict_least_used(time_shift,max_evict,evict_list);
			string evicted_hash_as_str = joiner(evict_list);
			info.GetReturnValue().Set(New(evicted_hash_as_str.c_str()).ToLocalChecked());
		}
	}

	NAN_METHOD(run_lru_eviction_get_values)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		time_t cutoff = Nan::To<uint32_t>(info[1]).FromJust();
		uint32_t max_evict_b = Nan::To<uint32_t>(info[2]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			map<uint64_t,char *> evict_map;
			uint8_t max_evict = (uint8_t)(max_evict_b);
			time_t time_shift = epoch_ms();
			time_shift -= cutoff;
			lru_cache->evict_least_used_to_value_map(time_shift,max_evict,evict_map);

			//string test = map_maker_destruct(evict_map);
			//cout << test << endl;

			Local<Object> jsObject = Nan::New<Object>();
			js_map_maker_destruct(evict_map,jsObject);
			info.GetReturnValue().Set(jsObject);
		}
	}

	NAN_METHOD(run_lru_targeted_eviction_get_values)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		time_t cutoff = Nan::To<uint32_t>(info[1]).FromJust();
		uint32_t max_evict_b = Nan::To<uint32_t>(info[2]).FromJust();
		//
		uint32_t hash_bucket = Nan::To<uint32_t>(info[3]).FromJust();
		uint32_t original_hash = Nan::To<uint32_t>(info[4]).FromJust();
		//
		uint64_t hash64 = (((uint64_t)index << HALF) | (uint64_t)original_hash);

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			map<uint64_t,char *> evict_map;
			uint8_t max_evict = (uint8_t)(max_evict_b);
			time_t time_shift = epoch_ms();
			time_shift -= cutoff;
			//
			uint8_t evict_count = lru_cache->evict_least_used_near_hash(hash_bucket,time_shift,max_evict,evict_map);
			//
			if ( evict_count < max_evict ) {
				uint8_t remaining = max_evict - evict_count;
				lru_cache->evict_least_used_to_value_map(time_shift,remaining,evict_map);
			}

			//string test = map_maker_destruct(evict_map);
			//cout << test << endl;

			Local<Object> jsObject = Nan::New<Object>();
			js_map_maker_destruct(evict_map,jsObject);
			info.GetReturnValue().Set(jsObject);
		}
	}


	/**
	 * 
	*/
	NAN_METHOD(run_lru_eviction_move_values)  {
		Nan::HandleScope scope;
		key_t key1 = Nan::To<uint32_t>(info[0]).FromJust();
		key_t key2 = Nan::To<uint32_t>(info[1]).FromJust();
		time_t cutoff = Nan::To<uint32_t>(info[2]).FromJust();
		uint32_t max_evict_b = Nan::To<uint32_t>(info[3]).FromJust();


		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		LRU_cache *from_lru_cache = g_LRU_caches_per_segment[key1];
		LRU_cache *to_lru_cache = g_LRU_caches_per_segment[key2];
		if ( from_lru_cache == nullptr ) {
			if ( shmCheckKey(key1) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else if ( to_lru_cache == nullptr ) {
			if ( shmCheckKey(key2) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			map<uint64_t,char *> evict_map;
			uint8_t max_evict = (uint8_t)(max_evict_b);
			time_t time_shift = epoch_ms();
			time_shift -= cutoff;
			from_lru_cache->evict_least_used_to_value_map(time_shift,max_evict,evict_map);

			map<uint64_t,uint32_t> offsets;

			for ( auto p : evict_map ) {
				uint64_t hash64 = p.first;
				char *data = p.second;
				uint32_t offset = to_lru_cache->check_for_hash(hash64);
				if ( offset == UINT32_MAX ) {  // no -- go ahead and add a new element  >> add_el
					offset = to_lru_cache->add_el(data,hash64);
					if ( offset != UINT32_MAX ) {
						offsets[hash64] = offset;
					}
				}
			}
			//
			info.GetReturnValue().Set(Nan::New<Boolean>(true));
		}
	}


	NAN_METHOD(debug_dump_list)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		bool backwards = Nan::To<bool>(info[1]).FromJust();


		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		LRU_cache *lru_cache = g_LRU_caches_per_segment[key];
		if ( lru_cache == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			list<uint32_t> evict_list;
			lru_cache->_walk_allocated_list(3,backwards);
			info.GetReturnValue().Set(Nan::New<Boolean>(true));
		}	
	}


	// MUTEX

	// fixed size data elements 
	NAN_METHOD(init_mutex) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		bool am_initializer = Nan::To<bool>(info[1]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		int resId = shmget(key, 0, 0);
		if (resId == -1) {
			switch(errno) {
				case ENOENT: // not exists
				case EIDRM:  // scheduled for deletion
					info.GetReturnValue().Set(Nan::New<Number>(-1));
					return;
				default:
					return Nan::ThrowError(strerror(errno));
			}
		}
		
		MutexHolder *mtx = g_MUTEX_per_segment[key];
		//
		if ( shmCheckKey(key) ) {
			if ( mtx != nullptr ) {
				// just say that access through the key is possible
				info.GetReturnValue().Set(Nan::New<Boolean>(true));
			} else {
				// setup the access
				void *region = g_segments_manager->get_addr(key);
				if ( region == nullptr ) {
					info.GetReturnValue().Set(Nan::New<Number>(-1));
					return;
				}
				//
				mtx = new MutexHolder(region,am_initializer);

				if ( mtx->ok() ) {
					g_MUTEX_per_segment[key] = mtx;
					info.GetReturnValue().Set(Nan::New<Boolean>(true));
				} else {
					string throw_message = mtx->get_last_reason();
					return Nan::ThrowError(throw_message.c_str());
				}
			}
		} else {
			if ( mtx != nullptr ) {
				g_MUTEX_per_segment.erase(key);
			}
			info.GetReturnValue().Set(Nan::New<Number>(-1));
		}
	}


	NAN_METHOD(get_last_mutex_reason)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		MutexHolder *mtx = g_MUTEX_per_segment[key];
		if ( mtx == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			string mtx_reason = mtx->get_last_reason();
			const char *reason = mtx_reason.c_str();
			info.GetReturnValue().Set(New(reason).ToLocalChecked());
		}
	}



	NAN_METHOD(try_lock)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		MutexHolder *mtx = g_MUTEX_per_segment[key];
		if ( mtx == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Number>(-3));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			if ( mtx->try_lock() ) {  // returns true if the lock is acquired
				info.GetReturnValue().Set(Nan::New<Boolean>(true));
			} else {
				if ( mtx->ok() ) {  // the lock aquizition failed but the reason was useful (EBUSY)
					info.GetReturnValue().Set(Nan::New<Boolean>(false));
				} else {  // the lock aquizition failed but the reason indicated bad state
					info.GetReturnValue().Set(Nan::New<Number>(-1));
				}
			}
		}
	}


	NAN_METHOD(lock)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();


		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		MutexHolder *mtx = g_MUTEX_per_segment[key];
		if ( mtx == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Number>(-3));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			if ( mtx->lock() ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(true));
			} else {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			}
		}
	}



	NAN_METHOD(unlock)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		//

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		MutexHolder *mtx = g_MUTEX_per_segment[key];
		if ( mtx == nullptr ) {
			if ( shmCheckKey(key) ) {
				info.GetReturnValue().Set(Nan::New<Number>(-3));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			if ( mtx->unlock() ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(true));
			} else {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			}
		}
	}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// SUPER_HEADER,NTiers,INTER_PROC_DESCRIPTOR_WORDS
	// let p_offset = sz*(this.proc_index)*i + SUPER_HEADER*NTiers


	static TierAndProcManager *g_tiers_procs = nullptr;


	NAN_METHOD(set_com_buf)  {
		Nan::HandleScope scope;

		uint32_t com_key = Nan::To<uint32_t>(info[0]).FromJust();
		Local<Array> keys = Local<Array>::Cast(info[1]);

		uint32_t SUPER_HEADER = Nan::To<uint32_t>(info[2]).FromJust();
		uint32_t NTiers = Nan::To<uint32_t>(info[3]).FromJust();
		uint32_t INTER_PROC_DESCRIPTOR_WORDS = Nan::To<uint32_t>(info[3]).FromJust();
		//
		//
		size_t rc_sz = Nan::To<uint32_t>(info[4]).FromJust();
		size_t seg_sz = Nan::To<uint32_t>(info[5]).FromJust();
		bool am_initializer = Nan::To<bool>(info[6]).FromJust();
		//

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}


		uint16_t n = keys->Length();
		v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();Value(context).FromJust();
		//
		void *regions[MAX_TIERS];

		for ( int i = 0; i < min(NTiers,n); i++ ) {
			uint32_t key = keys->Get(context, i).ToLocalChecked()->Uint32Value(context).FromJust();
			//
			int resId = shmget(key, 0, 0);
			if (resId == -1) {
				switch(errno) {
					case ENOENT: // not exists
					case EIDRM:  // scheduled for deletion
						info.GetReturnValue().Set(Nan::New<Number>(-1));
						return;
					default:
						return Nan::ThrowError(strerror(errno));
				}
			}
			//
			void *region = g_segments_manager->get_addr(key);
			if ( region == nullptr ) {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
				return;
			}
			//
			regions[i] = = region;
		}

		// ----

		{
			int resId = shmget(com_key, 0, 0);
			if (resId == -1) {
				switch(errno) {
					case ENOENT: // not exists
					case EIDRM:  // scheduled for deletion
						info.GetReturnValue().Set(Nan::New<Number>(-1));
						return;
					default:
						return Nan::ThrowError(strerror(errno));
				}
			}
			//
			void *region = g_segments_manager->get_addr(key);
			if ( region == nullptr ) {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
				return;
			}
			g_com_buffer = region;
		}

		//
		//	launch readers after hopscotch tables are put into place
		//	The g_tiers_procs are initialized with the regions as communication buffers...
		//
		g_tiers_procs = new TierAndProcManager(regions, rc_sz, am_initializer, NTiers, SUPER_HEADER, INTER_PROC_DESCRIPTOR_WORDS);
		g_tiers_procs->set_and_init_com_buffer(g_com_buffer);

		info.GetReturnValue().Set(Nan::New<Boolean>(true));
	}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	static_assert(sizeof(T) == sizeof(std::atomic<T>), 
			"atomic<T> isn't the same size as T");

	static_assert(std::atomic<T>::is_always_lock_free,  // C++17
			"atomic<T> isn't lock-free, unusable on shared mem");
			
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----



	// messages_reserved is an area to store pointers to the messages that will be read.
	// duplicate_reserved is area to store pointers to access points that are trying to insert duplicate

	// 				(TBD)
	//  	attach_to_lru_list(LRU_element *first,LRU_element *last);
	//		pair<uint32_t,uint32_t> filter_existence_check(messages,accesses,ready_msg_count);
	//		LRU_element *claim_free_mem(ready_msg_count,assigned_tier)




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



	// LRU_cache method




	void allow_delay() {
		usleep(2);
	}


	/**
	 * `put`
	 *
	 *  add or update a value  (JavaScript call)
	 * 
	 * 	The method mostly accesses the com buffer in order to make a piece of data available to 
	 * 	other processes for placement into tables.
	 * 
	 *	This method requires that the com buffer (a reference known to this module), 
	 * 	has been initialized...
	 * 
	 * 	A policy on the tier may be to set the new value (after searching for it) in the first tier (0)
	 * 	and then removing the value from older tiers eventually, since the old timestamps will not be accessible in
	 * 	in future searches. 
	 * 	
	 * 	Parameters:
	 * 	process - The process number of the calling process
	 *  hash_bucket - The modulus of the hash relative to the full table size.
	 *	full_hash - The hash (e.g. XXHash of the data being stored)
	 *	updating -  Whether or not this is to be treated as a new key or if the key is expected to be present.
	 *	buffer - the actual data to be stored, a buffer type
	 *	size - the size of the buffer
	 *	timestamp - if the method is calld for updating, the old timestamp will help locate the tier where the key-value
	 *				pair will be located. 
	*/
	NAN_METHOD(put)  {
		//
		Nan::HandleScope scope;
		uint32_t process = Nan::To<uint32_t>(info[0]).FromJust();	// the process number
		uint32_t hash_bucket = Nan::To<uint32_t>(info[1]).FromJust();	// hash modulus the number of buckets (hence a bucket)
		uint32_t full_hash = Nan::To<uint32_t>(info[2]).FromJust();		// hash of the value
		bool updating = Nan::To<bool>(info[3]).FromJust();		// hash of the value
		//
		char* buffer = (char*) node::Buffer::Data(info[3]->ToObject());  // should be a reference...
    	unsigned int size = info[4]->Uint32Value();

		uint32_t timestamp = 0;
		if ( updating ) {
			timestamp = info[5]->Uint32Value();
		}
		//



		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		uint32_t tier = 0;  // new elements go into the first tier ... later they may move...
		LRU_cache *lru = g_tier_to_LRU[tier];   // this is being accessed in more than one place...

		if ( lru == nullptr ) {  // has not been initialized
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		if ( g_com_buffer == NULL ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		} else {
			//
			if ( buffer && (size > 0) ) {
				//
				int status = g_tiers_procs->put_method(process, hash_bucket, full_hash, updating, buffer, size, timestamp, tier, allow_delay);
				if ( status < -1 ) {
					info.GetReturnValue().Set(Nan::New<Boolean>(false));
					return;
				}
				if ( status == -1 ) {
					info.GetReturnValue().Set(Nan::New<Number>(-1));
					return;
				}
				//
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
				return;
			}
		}
		info.GetReturnValue().Set(Nan::New<Boolean>(true));
	}


	// Init module
	static void Init(Local<Object> target) {
		initShmSegmentsInfo();
		
		Nan::SetMethod(target, "shm_get", shm_get);
		Nan::SetMethod(target, "detach", detach);
		Nan::SetMethod(target, "detachAll", detachAll);
		Nan::SetMethod(target, "getTotalSize", getTotalSize);
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		Nan::SetMethod(target, "epoch_time", time_since_epoch);


		Nan::SetMethod(target, "initLRU", initLRU);
		Nan::SetMethod(target, "getSegSize", getSegSize);
		Nan::SetMethod(target, "max_count", getMaxCount);
		Nan::SetMethod(target, "current_count", getCurrentCount);
		Nan::SetMethod(target, "free_count", getFreeCount);

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		Nan::SetMethod(target, "set_el", set_el);
		Nan::SetMethod(target, "set_many", set_many);
		//
		Nan::SetMethod(target, "get_el", get_el);
		Nan::SetMethod(target, "del_el", del_el);
		Nan::SetMethod(target, "del_key", del_key);
		Nan::SetMethod(target, "remove_key", remove_key);

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		Nan::SetMethod(target, "get_el_hash", get_el_hash);
		Nan::SetMethod(target, "get_last_reason", get_last_reason);
		Nan::SetMethod(target, "reload_hash_map", reload_hash_map);
		Nan::SetMethod(target, "reload_hash_map_update", reload_hash_map_update);
		Nan::SetMethod(target, "set_share_key", set_share_key);
		//
		Nan::SetMethod(target, "debug_dump_list", debug_dump_list);
		//
		// HOPSCOTCH HASH
		Nan::SetMethod(target, "initHopScotch", initHopScotch);
		//
		Nan::SetMethod(target, "run_lru_eviction", run_lru_eviction);
		Nan::SetMethod(target, "run_lru_eviction_get_values", run_lru_eviction_get_values);
		Nan::SetMethod(target, "run_lru_eviction_move_values", run_lru_eviction_move_values);
		
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		Nan::SetMethod(target, "init_mutex", init_mutex);
		Nan::SetMethod(target, "try_lock", try_lock);
		Nan::SetMethod(target, "lock", lock);
		Nan::SetMethod(target, "unlock", unlock);
		Nan::SetMethod(target, "get_last_mutex_reason", get_last_mutex_reason);
	
		//



		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		Nan::Set(target, Nan::New("IPC_PRIVATE").ToLocalChecked(), Nan::New<Number>(IPC_PRIVATE));
		Nan::Set(target, Nan::New("IPC_CREAT").ToLocalChecked(), Nan::New<Number>(IPC_CREAT));
		Nan::Set(target, Nan::New("IPC_EXCL").ToLocalChecked(), Nan::New<Number>(IPC_EXCL));
		Nan::Set(target, Nan::New("SHM_RDONLY").ToLocalChecked(), Nan::New<Number>(SHM_RDONLY));
		Nan::Set(target, Nan::New("NODE_BUFFER_MAX_LENGTH").ToLocalChecked(), Nan::New<Number>(node::Buffer::kMaxLength));
		//enum ShmBufferType
		Nan::Set(target, Nan::New("SHMBT_BUFFER").ToLocalChecked(), Nan::New<Number>(SHMBT_BUFFER));
		Nan::Set(target, Nan::New("SHMBT_INT8").ToLocalChecked(), Nan::New<Number>(SHMBT_INT8));
		Nan::Set(target, Nan::New("SHMBT_UINT8").ToLocalChecked(), Nan::New<Number>(SHMBT_UINT8));
		Nan::Set(target, Nan::New("SHMBT_UINT8CLAMPED").ToLocalChecked(), Nan::New<Number>(SHMBT_UINT8CLAMPED));
		Nan::Set(target, Nan::New("SHMBT_INT16").ToLocalChecked(), Nan::New<Number>(SHMBT_INT16));
		Nan::Set(target, Nan::New("SHMBT_UINT16").ToLocalChecked(), Nan::New<Number>(SHMBT_UINT16));
		Nan::Set(target, Nan::New("SHMBT_INT32").ToLocalChecked(), Nan::New<Number>(SHMBT_INT32));
		Nan::Set(target, Nan::New("SHMBT_UINT32").ToLocalChecked(), Nan::New<Number>(SHMBT_UINT32));
		Nan::Set(target, Nan::New("SHMBT_FLOAT32").ToLocalChecked(), Nan::New<Number>(SHMBT_FLOAT32));
		Nan::Set(target, Nan::New("SHMBT_FLOAT64").ToLocalChecked(), Nan::New<Number>(SHMBT_FLOAT64));


		Isolate* isolate = target->GetIsolate();
		AddEnvironmentCleanupHook(isolate,AtNodeExit,nullptr);
		//node::AtExit(AtNodeExit);
	}

}
}

//-------------------------------

NODE_MODULE(shm, node::node_shm::Init);
