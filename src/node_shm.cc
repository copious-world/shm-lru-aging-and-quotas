#include "node_shm.h"

//-------------------------------
using namespace Nan;


// In this module, the original 'typed array' module will not be shadowed
//-------------------------------

//-------------------------------

namespace node {
namespace node_shm {

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




	// ----
		// shm_get
	// ----

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



	// ----
		// getSegSize
	// ----

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


	// ----
		// detach
	// ----

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

	// ----
		// detachAll
	// ----

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

	// ----
		// getTotalSize
	// ----

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


	NAN_METHOD(create) {
		Nan::HandleScope scope;
		//
		if ( g_segments_manager == nullptr ) {		// initialized
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return
		}
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



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// SUPER_HEADER,NTiers,INTER_PROC_DESCRIPTOR_WORDS
	// let p_offset = sz*(this.proc_index)*i + SUPER_HEADER*NTiers


	static TierAndProcManager<3> *g_tiers_procs = nullptr;

	// fixed size data elements 
	NAN_METHOD(initialize_com_and_all_tiers) {
		Nan::HandleScope scope;
		//
		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		if ( info.Length() < 2 ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		//
		bool am_initializer			= Nan::To<bool>(info[0]).FromJust(); 	 // 0
		uint32_t proc_number		= Nan::To<uint32_t>(info[1]).FromJust(); // 1
		uint32_t num_procs 			= Nan::To<uint32_t>(info[2]).FromJust(); // 2
		uint32_t num_tiers 			= Nan::To<uint32_t>(info[3]).FromJust(); // 3
		uint32_t els_per_tier 		= Nan::To<uint32_t>(info[4]).FromJust(); // 4  // els per tier includes expecter # legal sessions + reserve...
		uint32_t max_obj_size 		= Nan::To<uint32_t>(info[5]).FromJust(); // 5
		key_t com_key 				= Nan::To<uint32_t>(info[6]).FromJust(); // 6
		key_t randoms_key 			= Nan::To<uint32_t>(info[6]).FromJust(); // 6
		//
		if ( com_key < keyMin || com_key >= keyMax ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		Local<Array> jsArray		= Local<Array>::Cast(info[6]);			 // 8
		uint16_t n = jsArray->Length();
		n = min(n,num_tiers);   // should be the same, but in case not, use the smaller number.
		//
		v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
		//
		list<uint32_t> lru_keys;
		list<uint32_t> hh_keys;
		//
		for ( uint16_t i = 0; i < n; i++ ) {
			uint16_t j = 2*i;
			uint16_t k = j + 1;
			// data is stored in the lru... the hash table is divided into regions for control and integer sized entries
			uint32_t lru_key  = Local<Array>::Cast(jsArray->Get(context, j)->Uint32Value(context).FromJust();
			lru_keys.push_back(lru_key);
			uint32_t hh_key  = Local<Array>::Cast(jsArray->Get(context, k)->Uint32Value(context).FromJust();
			hh_keys.push_back(lru_key);
		}
		//
		status = g_segments_manager->region_intialization_ops(lru_keys, hh_keys, am_initializer,
																num_procs, num_tiers, els_per_tier, max_obj_size, com_key, randoms_key);
		if ( status != 0 ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		//	launch readers after hopscotch tables are put into place
		//	The g_tiers_procs are initialized with the regions as communication buffers...
		//

		g_tiers_procs = new TierAndProcManager<3>(g_segments_manager->_com_buffer,
													g_segments_manager->_seg_to_lrus,
														g_segments_manager->_seg_to_hh_tables,
															g_segments_manager->_ids_to_seg_sizes,
																am_initializer, proc_number,
																	num_procs, num_tiers, els_per_tier, max_obj_size);
		//
		status = g_tiers_procs->launch_second_phase_threads();
		//
		if ( status != 0 ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		int count_segs = g_segments_manager->_ids_to_seg_addrs.size();
		info.GetReturnValue().Set(Nan::New<Number>(count_segs));
	}






	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	NAN_METHOD(getMaxCount) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_segments_manager->_seg_to_lrus[key];
		if ( lru_cache == nullptr ) {
			if ( g_segments_manager->check_key(key) ) {
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

		LRU_cache *lru_cache = g_segments_manager->_seg_to_lrus[key];
		if ( lru_cache == nullptr ) {
			if ( g_segments_manager->check_key(key) ) {
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

		LRU_cache *lru_cache = g_segments_manager->_seg_to_lrus[key];
		if ( lru_cache == nullptr ) {
			if ( g_segments_manager->check_key(key) ) {
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





	NAN_METHOD(get_last_reason)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		//
		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		LRU_cache *lru_cache = g_segments_manager->_seg_to_lrus[key];
		if ( lru_cache == nullptr ) {
			if ( g_segments_manager->check_key(key) ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
			} else {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
			}
		} else {
			const char *reason = lru_cache->get_last_reason();
			info.GetReturnValue().Set(New(reason).ToLocalChecked());
		}
	}




	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


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
			timestamp =  Nan::To<uint32_t>(info[5]).FromJust();
		}
		//

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		if ( g_tiers_procs == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		if ( g_segments_manager->_com_buffer == NULL ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		} else {
			//
			if ( buffer && (size > 0) ) {
				//
				uint32_t tier = 0;   // the entry point from the application is the zero tier, yet this may change due to the timestamp
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




	// ----
		// get 
	// ----

	NAN_METHOD(get)  {
		Nan::HandleScope scope;
		uint32_t process = Nan::To<uint32_t>(info[0]).FromJust();	// the process number
		uint32_t hash_bucket = Nan::To<uint32_t>(info[1]).FromJust();
		uint32_t full_hash = Nan::To<uint32_t>(info[2]).FromJust();
		// selector
		uint32_t timestamp =  Nan::To<uint32_t>(info[3]).FromJust();

		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}

		//
		if ( g_tiers_procs == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		if ( g_segments_manager->_com_buffer == NULL ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		} else {
			//
			uint32_t tier = 0;   // the entry point from the application is the zero tier, yet this may change due to the timestamp
			size_t rsz = g_tiers_procs->record_size();
			char data[rsz];
			//
			int status = g_tiers_procs->get_method(hash_bucket, full_hash, data, rsz, timestamp, tier, allow_delay);
			if ( status < -1 ) {
				info.GetReturnValue().Set(Nan::New<Boolean>(false));
				return;
			}
			if ( status == -1 ) {
				info.GetReturnValue().Set(Nan::New<Number>(-1));
				return;
			}
			//
			info.GetReturnValue().Set(New(data).ToLocalChecked());
		}

	}




	// ----
		// del el
	// ----

	NAN_METHOD(del_key)  {
		Nan::HandleScope scope;
		//
		uint32_t process = Nan::To<uint32_t>(info[0]).FromJust();	// the process number
		uint32_t hash = Nan::To<uint32_t>(info[1]).FromJust();		// bucket index
		uint32_t full_hash = Nan::To<uint32_t>(info[2]).FromJust();
		// selector bit
		uint32_t timestamp =  Nan::To<uint32_t>(info[3]).FromJust();

		//
		if ( g_segments_manager == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		if ( g_tiers_procs == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//
		uint32_t tier = 0;   // the entry point from the application is the zero tier, yet this may change due to the timestamp
		int status = g_tiers_procs->del_method(process,hash,full_hash,timestamp,tier);
		if ( status == 0 ) {
			info.GetReturnValue().Set(Nan::New<Boolean>(true));
		} else {
			info.GetReturnValue().Set(Nan::New<Number>(-2));
		}
	}




	// Init module
	static void Init(Local<Object> target) {
		initShmSegmentsInfo();
		
		Nan::SetMethod(target, "shm_get", shm_get);
		Nan::SetMethod(target, "detach", detach);
		Nan::SetMethod(target, "detachAll", detachAll);
		Nan::SetMethod(target, "getTotalSize", getTotalSize);
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		Nan::SetMethod(target, "initialize_com_and_all_tiers", initialize_com_and_all_tiers);

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		Nan::SetMethod(target, "epoch_time", time_since_epoch);

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		Nan::SetMethod(target, "getSegSize", getSegSize);
		Nan::SetMethod(target, "max_count", getMaxCount);
		Nan::SetMethod(target, "current_count", getCurrentCount);
		Nan::SetMethod(target, "free_count", getFreeCount);

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		Nan::SetMethod(target, "put", put);
		Nan::SetMethod(target, "get", get);
		Nan::SetMethod(target, "del_key", del_key);

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		Nan::SetMethod(target, "get_last_reason", get_last_reason);
		//
		Nan::SetMethod(target, "debug_dump_list", debug_dump_list);
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----	

		Isolate* isolate = target->GetIsolate();
		AddEnvironmentCleanupHook(isolate,AtNodeExit,nullptr);
		//node::AtExit(AtNodeExit);
	}

}
}

//-------------------------------

NODE_MODULE(shm, node::node_shm::Init);
