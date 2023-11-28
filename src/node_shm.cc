#include "node_shm.h"

//-------------------------------
using namespace Nan;

namespace node {
namespace Buffer {

	using v8::ArrayBuffer;
	using v8::SharedArrayBuffer;
	using v8::ArrayBufferCreationMode;
	using v8::EscapableHandleScope;
	using node::AddEnvironmentCleanupHook;
	using v8::Isolate;
	using v8::Local;
	using v8::MaybeLocal;
	//
	using v8::Object;
	using v8::Array;
	using v8::Integer;
	using v8::Maybe;
	using v8::String;
	using v8::Value;
	using v8::Int8Array;
	using v8::Uint8Array;
	using v8::Uint8ClampedArray;
	using v8::Int16Array;
	using v8::Uint16Array;
	using v8::Int32Array;
	using v8::Uint32Array;
	using v8::Float32Array;
	using v8::Float64Array;

	MaybeLocal<Object> NewTyped(
		Isolate* isolate, 
		char* data, 
		size_t count
	#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback
	#else
	    , node::smalloc::FreeCallback callback
	#endif
	    , void *hint
		, ShmBufferType type
	) {
		size_t length = count * getSize1ForShmBufferType(type);

		EscapableHandleScope scope(isolate);

		/*
		MaybeLocal<Object> mlarr = node::Buffer::New(
			isolate, data, length, callback, hint);
		Local<Object> larr = mlarr.ToLocalChecked();
		
		Uint8Array* arr = (Uint8Array*) *larr;
		Local<ArrayBuffer> ab = arr->Buffer();
		*/
		//Local<ArrayBuffer> ab = ArrayBuffer::New(isolate, data, length, ArrayBufferCreationMode::kExternalized);

		std::shared_ptr<v8::BackingStore> backing = v8::SharedArrayBuffer::NewBackingStore(data, length, 
																						[](void*, size_t, void*){}, nullptr);
		Local<SharedArrayBuffer> ab = v8::SharedArrayBuffer::New(isolate, std::move(backing));
		
		Local<Object> ui;
		switch(type) {
			case SHMBT_INT8:
				ui = Int8Array::New(ab, 0, count);
			break;
			case SHMBT_UINT8:
				ui = Uint8Array::New(ab, 0, count);
			break;
			case SHMBT_UINT8CLAMPED:
				ui = Uint8ClampedArray::New(ab, 0, count);
			break;
			case SHMBT_INT16:
				ui = Int16Array::New(ab, 0, count);
			break;
			case SHMBT_UINT16:
				ui = Uint16Array::New(ab, 0, count);
			break;
			case SHMBT_INT32:
				ui = Int32Array::New(ab, 0, count);
			break;
			case SHMBT_UINT32:
				ui = Uint32Array::New(ab, 0, count);
			break;
			case SHMBT_FLOAT32:
				ui = Float32Array::New(ab, 0, count);
			break;
			default:
			case SHMBT_FLOAT64:
				ui = Float64Array::New(ab, 0, count);
			break;
		}

		return scope.Escape(ui);
	}

}
}

//-------------------------------

namespace Nan {

	inline MaybeLocal<Object> NewTypedBuffer(
	      char *data
	    , size_t count
	#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
	    , node::Buffer::FreeCallback callback
	#else
	    , node::smalloc::FreeCallback callback
	#endif
	    , void *hint
	    , ShmBufferType type
	) {
		size_t length = count * getSize1ForShmBufferType(type);

		if (type != SHMBT_BUFFER) {
	  	assert(count <= node::Buffer::kMaxLength && "too large typed buffer");
			#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
			    return node::Buffer::NewTyped(
			        Isolate::GetCurrent(), data, count, callback, hint, type);
			#else
			    return MaybeLocal<v8::Object>(node::Buffer::NewTyped(
			        Isolate::GetCurrent(), data, count, callback, hint, type));
			#endif
	  } else {
	  	assert(length <= node::Buffer::kMaxLength && "too large buffer");
			#if NODE_MODULE_VERSION > IOJS_2_0_MODULE_VERSION
			    return node::Buffer::New(
			        Isolate::GetCurrent(), data, length, callback, hint);
			#else
			    return MaybeLocal<v8::Object>(node::Buffer::New(
			        Isolate::GetCurrent(), data, length, callback, hint));
			#endif
	  }

	}

}

//-------------------------------

namespace node {
namespace node_shm {

	using node::AtExit;
	using v8::Local;
	using v8::Number;
	using v8::Object;
	using v8::Value;


	map<key_t,LRU_cache *>  g_LRU_caches_per_segment;
	map<key_t,HH_map *>  g_HMAP_caches_per_segment;
	map<int,size_t> g_ids_to_seg_sizes;
	map<key_t,MutexHolder *> g_MUTEX_per_segment;

	void *g_com_buffer = NULL;



	// Arrays to keep info about created segments, call it "info ararys"
	int shmSegmentsCnt = 0;
	size_t shmSegmentsBytes = 0;
	int shmSegmentsCntMax = 0;
	int* shmSegmentsIds = NULL;
	void** shmSegmentsAddrs = NULL;

	// Declare private methods
	static int detachShmSegments();
	static void initShmSegmentsInfo();
	static int detachShmSegment(int resId, void* addr, bool force = false, bool onExit = false);
	static void addShmSegmentInfo(int resId, void* addr, size_t sz);
	static bool hasShmSegmentInfo(int resId);
	static void * getShmSegmentAddr(int resId);
	static bool removeShmSegmentInfo(int resId);
	static void FreeCallback(char* data, void* hint);
	static void Init(Local<Object> target);
	static void AtNodeExit(void*);
	//

	// Init info arrays
	static void initShmSegmentsInfo() {
		detachShmSegments();

		shmSegmentsCnt = 0;
		shmSegmentsCntMax = 16; //will be multiplied by 2 when arrays are full
		shmSegmentsIds = new int[shmSegmentsCntMax];
		shmSegmentsAddrs = new void*[shmSegmentsCntMax];
	}

	// Detach all segments and delete info arrays
	// Returns count of destroyed segments
	static int detachShmSegments() {
		int res = 0;
		if (shmSegmentsCnt > 0) {
			void* addr;
			int resId;
			for (int i = 0 ; i < shmSegmentsCnt ; i++) {
				addr = shmSegmentsAddrs[i];
				resId = shmSegmentsIds[i];
				if (detachShmSegment(resId, addr, false, true) == 0)
					res++;
			}
		}

		SAFE_DELETE_ARR(shmSegmentsIds);
		SAFE_DELETE_ARR(shmSegmentsAddrs);
		shmSegmentsCnt = 0;
		return res;
	}

	// Add segment to info arrays
	static void addShmSegmentInfo(int resId, void* addr, size_t sz) {
		int* newShmSegmentsIds;
		void** newShmSegmentsAddrs;
		if (shmSegmentsCnt == shmSegmentsCntMax) {
			//extend ararys by *2 when full
			shmSegmentsCntMax *= 2;
			newShmSegmentsIds = new int[shmSegmentsCntMax];
			newShmSegmentsAddrs = new void*[shmSegmentsCntMax];
			std::copy(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, newShmSegmentsIds);
			std::copy(shmSegmentsAddrs, shmSegmentsAddrs + shmSegmentsCnt, newShmSegmentsAddrs);
			delete [] shmSegmentsIds;
			delete [] shmSegmentsAddrs;
			shmSegmentsIds = newShmSegmentsIds;
			shmSegmentsAddrs = newShmSegmentsAddrs;
		}
		shmSegmentsIds[shmSegmentsCnt] = resId;
		shmSegmentsAddrs[shmSegmentsCnt] = addr;
		shmSegmentsCnt++;
		g_ids_to_seg_sizes[resId] = sz;
	}

	static bool hasShmSegmentInfo(int resId) {
		int* end = shmSegmentsIds + shmSegmentsCnt;
		int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, resId);
		return (found != end);
	}

	static void *getShmSegmentAddr(int resId) {
		int* end = shmSegmentsIds + shmSegmentsCnt;
		int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, resId);
		if (found == end) {
			//not found in info array
			return nullptr;
		}
		int i = found - shmSegmentsIds;
		void* addr = shmSegmentsAddrs[i];
		return(addr);
	}

	// Remove segment from info arrays
	static bool removeShmSegmentInfo(int resId) {
		int* end = shmSegmentsIds + shmSegmentsCnt;
		int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, resId);
		if (found == end)
			return false; //not found
		int i = found - shmSegmentsIds;
		if (i == shmSegmentsCnt-1) {
			//removing last element
		} else {
			std::copy(shmSegmentsIds + i + 1, 
				shmSegmentsIds + shmSegmentsCnt, 
				shmSegmentsIds + i);
			std::copy(shmSegmentsAddrs + i + 1, 
				shmSegmentsAddrs + shmSegmentsCnt,
				shmSegmentsAddrs + i);
		}
		shmSegmentsIds[shmSegmentsCnt-1] = 0;
		shmSegmentsAddrs[shmSegmentsCnt-1] = NULL;
		shmSegmentsCnt--;
		return true;
	}

	// Detach segment
	// Returns count of left attaches or -1 on error
	static int detachShmSegment(int resId, void* addr, bool force, bool onExit) {
		int err;
		struct shmid_ds shminf;
		//detach
		err = shmdt(addr);
		if (err == 0) {
			//get stat
			err = shmctl(resId, IPC_STAT, &shminf);
			if (err == 0) {
				//destroy if there are no more attaches or force==true
				if (force || shminf.shm_nattch == 0) {
					err = shmctl(resId, IPC_RMID, 0);
					if (err == 0) {
						shmSegmentsBytes -= shminf.shm_segsz;
						return 0; //detached and destroyed
					} else {
						if(!onExit)
							Nan::ThrowError(strerror(errno));
					}
				} else {
					return shminf.shm_nattch; //detached, but not destroyed
				}
			} else {
				if(!onExit)
					Nan::ThrowError(strerror(errno));
			}
		} else {
			switch(errno) {
				case EINVAL: // wrong addr
				default:
					if(!onExit)
						Nan::ThrowError(strerror(errno));
				break;
			}
		}
		return -1;
	}



	bool shmCheckKey(key_t key) {

		int resId = shmget(key, 0, 0);
		if (resId == -1) {
			switch(errno) {
				case ENOENT: // not exists
				case EIDRM:  // scheduled for deletion
					return(false);
				default:
					return(false);
			}
		}
		return true;
	}

	// Used only when creating byte-array (Buffer), not typed array
	// Because impl of CallbackInfo::New() is not public (see https://github.com/nodejs/node/blob/v6.x/src/node_buffer.cc)
	// Developer can detach shared memory segments manually by shm.detach()
	// Also shm.detachAll() will be called on process termination
	static void FreeCallback(char* data, void* hint) {
		int resId = reinterpret_cast<intptr_t>(hint);
		void* addr = (void*) data;

		detachShmSegment(resId, addr, false, true);
		removeShmSegmentInfo(resId);
	}

	NAN_METHOD(get) {
		Nan::HandleScope scope;
		int err;
		struct shmid_ds shminf;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		size_t count = Nan::To<uint32_t>(info[1]).FromJust();
		int shmflg = Nan::To<uint32_t>(info[2]).FromJust();
		int at_shmflg = Nan::To<uint32_t>(info[3]).FromJust();
		ShmBufferType type = (ShmBufferType) Nan::To<int32_t>(info[4]).FromJust();
		size_t size = count * getSize1ForShmBufferType(type);
		bool isCreate = (size > 0);
		
		int resId = shmget(key, size, shmflg);
		if (resId == -1) {
			switch(errno) {
				case EEXIST: // already exists
				case EIDRM:  // scheduled for deletion
				case ENOENT: // not exists
					info.GetReturnValue().SetNull();
					return;
				case EINVAL: // should be SHMMIN <= size <= SHMMAX
					return Nan::ThrowRangeError(strerror(errno));
				default:
					return Nan::ThrowError(strerror(errno));
			}
		} else {
			if (!isCreate) {
				err = shmctl(resId, IPC_STAT, &shminf);
				if (err == 0) {
					size = shminf.shm_segsz;
					count = size / getSize1ForShmBufferType(type);
				} else
					return Nan::ThrowError(strerror(errno));
			}
			
			void* res = shmat(resId, NULL, at_shmflg);
			if (res == (void *)-1)
				return Nan::ThrowError(strerror(errno));

			if (!hasShmSegmentInfo(resId)) {
				addShmSegmentInfo(resId, res, size);
				shmSegmentsBytes += size;
			}

			info.GetReturnValue().Set(Nan::NewTypedBuffer(
				reinterpret_cast<char*>(res),
				count,
				FreeCallback,
				reinterpret_cast<void*>(static_cast<intptr_t>(resId)),
				type
			).ToLocalChecked());
		}
	}

	NAN_METHOD(detach) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		bool forceDestroy = Nan::To<bool>(info[1]).FromJust();
		//
		bool check = (g_LRU_caches_per_segment.find(key) != g_LRU_caches_per_segment.end());
		if ( check) {
			g_LRU_caches_per_segment.erase(key);
		}
		check =  (g_HMAP_caches_per_segment.find(key) != g_HMAP_caches_per_segment.end());
		if ( check) {
			g_HMAP_caches_per_segment.erase(key);
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
		} else {
			int* end = shmSegmentsIds + shmSegmentsCnt;
			int* found = std::find(shmSegmentsIds, shmSegmentsIds + shmSegmentsCnt, resId);
			if (found == end) {
				//not found in info array
				info.GetReturnValue().Set(Nan::New<Number>(-1));
				return;
			}
			int i = found - shmSegmentsIds;
			void* addr = shmSegmentsAddrs[i];

			int res = detachShmSegment(resId, addr, forceDestroy);
			if (res != -1)
				removeShmSegmentInfo(resId);
			info.GetReturnValue().Set(Nan::New<Number>(res));
		}
	}

	NAN_METHOD(detachAll) {
		int cnt = detachShmSegments();
		initShmSegmentsInfo();
		info.GetReturnValue().Set(Nan::New<Number>(cnt));
	}

	NAN_METHOD(getTotalSize) {
		info.GetReturnValue().Set(Nan::New<Number>(shmSegmentsBytes));
	}

	// node::AtExit
	static void AtNodeExit(void*) {
		detachShmSegments();
	}



	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


	// fixed size data elements 
	NAN_METHOD(initLRU) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		size_t rc_sz = Nan::To<uint32_t>(info[1]).FromJust();
		size_t seg_sz = Nan::To<uint32_t>(info[2]).FromJust();
		bool am_initializer = Nan::To<bool>(info[3]).FromJust();
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
		
		LRU_cache *plr = g_LRU_caches_per_segment[key];
		//
		if ( hasShmSegmentInfo(resId) ) {
			if ( plr != nullptr ) {
				info.GetReturnValue().Set(Nan::New<Number>(plr->max_count()));
			} else {
				void *region = getShmSegmentAddr(resId);
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
		if ( hasShmSegmentInfo(resId) ) {
			if ( phm != nullptr ) {
				info.GetReturnValue().Set(Nan::New<Number>(key));
			} else {
				void *region = getShmSegmentAddr(resId);
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




	NAN_METHOD(getSegSize) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();

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
		size_t seg_size = g_ids_to_seg_sizes[resId];
		info.GetReturnValue().Set(Nan::New<Number>(seg_size));
	}



	NAN_METHOD(getMaxCount) {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();

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
		uint64_t hash64 = (((uint64_t)full_hash << HALF) | (uint64_t)hash_bucket);
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
			char *data = *data_arg;
			// is the key already assigned ?  >> check_for_hash 
			uint32_t offset = lru_cache->check_for_hash(hash64);
			if ( offset == UINT32_MAX ) {  // no -- go ahead and add a new element  >> add_el
				offset = lru_cache->add_el(data,hash64);
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
		if ( hasShmSegmentInfo(resId) ) {
			if ( mtx != nullptr ) {
				// just say that access through the key is possible
				info.GetReturnValue().Set(Nan::New<Boolean>(true));
			} else {
				// setup the access
				void *region = getShmSegmentAddr(resId);
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


	uint32_t g_SUPER_HEADER =0;
	uint32_t g_NTiers = 0;
	uint32_t g_INTER_PROC_DESCRIPTOR_WORDS = 0;
	const uint32_t OFFSET_TO_MESSAGE = 32;
	const uint32_t MAX_MESSAGE_SIZE = 128;
	//
	const uint32_t OFFSET_TO_MARKER = 0;
	const uint32_t OFFSET_TO_OFFSET = 2;
	const uint32_t OFFSET_TO_HASH = 4;   // start of 64bits
	const uint32_t OFFSET_TO_MESSAGE = (4+8+2);		/// a possible message location, but data being stored will only trade the offset
	const uint32_t DEFAULT_MICRO_TIMEOUT = 2; // 2 seconds

	const MAX_WAIT_LOOPS = 1000;


	NAN_METHOD(set_com_buf)  {
		Nan::HandleScope scope;
		key_t key = Nan::To<uint32_t>(info[0]).FromJust();
		uint32_t SUPER_HEADER = Nan::To<uint32_t>(info[1]).FromJust();
		uint32_t NTiers = Nan::To<uint32_t>(info[2]).FromJust();
		uint32_t INTER_PROC_DESCRIPTOR_WORDS = Nan::To<uint32_t>(info[3]).FromJust();

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

		void *region = getShmSegmentAddr(resId);
		if ( region == nullptr ) {
			info.GetReturnValue().Set(Nan::New<Number>(-1));
			return;
		}
		//

		g_com_buffer = region
		info.GetReturnValue().Set(Nan::New<Boolean>(true));

		g_SUPER_HEADER = SUPER_HEADER;
		g_NTiers = NTiers;
		g_INTER_PROC_DESCRIPTOR_WORDS = INTER_PROC_DESCRIPTOR_WORDS;
	}

	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	static_assert(sizeof(T) == sizeof(std::atomic<T>), 
			"atomic<T> isn't the same size as T");

	static_assert(std::atomic<T>::is_always_lock_free,  // C++17
			"atomic<T> isn't lock-free, unusable on shared mem");
			
	// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

	typedef enum {
		CLEAR_FOR_WRITE,	// unlocked - only one process will write in this spot, so don't lock for writing. Just indicate that reading can be done
		CLEARED_FOR_ALLOC,	// the current process will set the atomic to CLEARED_FOR_ALLOC
		LOCKED_FOR_ALLOC,	// a thread (process) that picks up the reading task will block other readers from this spot
		CLEARED_FOR_COPY	// now let the writer copy the message into storage
	} COM_BUFFER_STATE;



	// only one process/thread should own this position. 
	// The only contention will be that some process/thread will inspect the buffer to see if there is a job there.
	// waiting for the state to be CLEAR_FOR_WRITE
	//
	inline bool wait_to_write(char *read_marker,uint16_t loops = MAX_WAIT_LOOPS,uint32_t delay = 2) {
		auto p = static_cast<atomic<uint8_t>*>(read_marker);
		uint16_t count = 0;
		while ( true ) {
			count++;
			COM_BUFFER_STATE clear = (COM_BUFFER_STATE)(p->load(std::memory_order_relaxed));
			if ( clear == CLEAR_FOR_WRITE ) break
			//
			if ( count > loops ) {
				return false;
			}
			usleep(delay);
		}
		return true;
	}

	// MAX_WAIT_LOOPS
	// await_write_offset(read_marker,MAX_WAIT_LOOPS,4)

	//
	inline void clear_for_write(unsigned char *read_marker) {   // first and last
		auto p = static_cast<atomic<uint8_t>*>(read_marker);
		while(!p->compare_exchange_weak((COM_BUFFER_STATE)(*read_marker),CLEAR_FOR_WRITE)
						&& ((COM_BUFFER_STATE)(*read_marker) !== CLEAR_FOR_WRITE));
	}

	//
	inline void cleared_for_alloc(unsigned char *read_marker) {
		auto p = static_cast<atomic<uint8_t>*>(read_marker);
		while(!p->compare_exchange_weak((COM_BUFFER_STATE)(*read_marker),CLEARED_FOR_ALLOC)
						&& ((COM_BUFFER_STATE)(*read_marker) !== CLEARED_FOR_ALLOC));
	}

	//
	inline void claim_for_alloc(unsigned char *read_marker) {
		auto p = static_cast<atomic<uint8_t>*>(read_marker);
		while(!p->compare_exchange_weak((COM_BUFFER_STATE)(*read_marker),LOCKED_FOR_ALLOC)
						&& ((COM_BUFFER_STATE)(*read_marker) !== LOCKED_FOR_ALLOC));
	}

	//
	inline bool await_write_offset(char *read_marker,uint16_t loops,uint32_t delay) {
		auto p = static_cast<atomic<uint8_t>*>(read_marker);
		uint32_t count = 0;
		while ( true ) {
			count++;
			COM_BUFFER_STATE clear = (COM_BUFFER_STATE)(p->load(std::memory_order_relaxed));
			if ( clear == CLEARED_FOR_COPY ) break
			//
			if ( count > MAX_WAIT_LOOPS ) {
				return false;
			}
			usleep(2);
		}
		return true;
	}


	//
	inline void clear_for_copy(unsigned char *read_marker) {
		auto p = static_cast<atomic<uint8_t>*>(read_marker);
		while(!p->compare_exchange_weak((COM_BUFFER_STATE)(*read_marker),CLEARED_FOR_COPY)
						&& ((COM_BUFFER_STATE)(*read_marker) !== CLEARED_FOR_COPY));
	}



	void transfer_to_source_tier(char *temp_bloat,size_t rec_size,map<uint32_t,map<uint32_t,uint64_t> &moving_objects,uint32_t req_count,uint32_t source_tier) {
		//
		// transfer control to a thread managing the next tier
		//
		for ( auto p : moving_objects ) {{
			uint32_t i = p.first;
			uint64_t hash = p.second;
			char *data = temp_bloat + rec_size*i;
			thread_adds_data(source_tier+1,data,hash);
		}}
	}

	bool run_evictions(LRU_cache *lru,uint32_t source_tier) {
		//
		// lru - is a source tier 
		//
		map<uint32_t,uint64_t> moving_objects;
		//
		uint8_t *start = _region;
		size_t step = _step;
		//
		LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
		_count_free = ctrl_free->_prev;  // using the hash field as the counter
		//
		uint32_t req_count = free_mem_requested();
		if ( req_count == 0 ) return true;
		//
		char *temp_bloat = new char[_record_size*req_count]
		char *tmp = temp_bloat;
		//
		for ( uint32_t i = 0; i < req_count; i++ ) {
			pair<uint32_t,uint64_t> &p = lru->lru_remove_last();
			uint32_t t_offset = p.first;
			uint32_t hash = p.second;
			LRU_element *moving = (LRU_element *)(start + t_offset);  // always the same each tier...
			char *data = (char *)(moving + 1);
			memcpy(data,tmp,_record_size);
			tmp += _record_size;
			return_to_free_mem(moving);
			moving_objects[i] = hash;
		}
		//
		transfer_to_source_tier(temp_bloat,_record_size,moving_objects,req_count,source_tier);
		return true;
	}

	// messages_reserved is an area to store pointers to the messages that will be read.
	// duplicate_reserved is area to store pointers to access points that are trying to insert duplicate

	// 				(TBD)
	//  	attach_to_lru_list(LRU_element *first,LRU_element *last);
	//		pair<uint32_t,uint32_t> filter_existence_check(messages,accesses,ready_msg_count);
	//		LRU_element *claim_free_mem(ready_msg_count,assigned_tier)



	// LRU_cache method
	pair<uint32_t,uint32_t> filter_existence_check(char **messages,char **accesses,uint32_t ready_msg_count) {
		uint32_t new_count = 0;
		uint32_t dup_count = 0;
		while ( ready_msg_count-- >= 0 ) {
			uint64_t hash = ((uint64_t *)(*messages))[0];
			if ( _hmap_i->get(hash) == 0 ) {
				accesses[dup_count++] = messages[ready_msg_count];
				messages[ready_msg_count] = NULL;
			}
		}
		return pair<uint32_t,uint32_t>(new_count,dup_count);
	}



/*
Free memory is a stack with atomic var protection.
If a basket of new entries is available, then claim a basket's worth of free nodes and return then as a 
doubly linked list for later entry. Later, add at most two elements to the LIFO queue the LRU.
*/
	// LRU_cache method
	inline uint32_t free_mem_requested(void) {
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
	bool check_free_mem(uint32_t msg_count,bool add) {
		//
		uint8_t *start = _region;
		size_t step = _step;
		//
		LRU_element *ctrl_free = (LRU_element *)(start + 2*step);  // always the same each tier...
		_count_free = ctrl_free->_prev;  // using the hash field as the counter
		//
		auto requested = static_cast<atomic<uint32_t>*>(&(ctrl_free->_hash));
		uint32_t total_requested = requested->load(std::memory_order_relaxed);
		if ( add ) {
			total_requested += msg_count;
			requested->fetch_add(msg_count);
		}
		if ( _count_free < total_requested ) return false;
		return true;
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





	// At the app level obtain the LRU for the tier and work from there
	int reader_operation(uint16_t proc_count, char **messages_reserved, char **duplicate_reserved, uint16_t assigned_tier) {
		if ( g_com_buffer == NULL  ) {    // com buffer not intialized
			return -5; // plan error numbers: this error is huge problem cannot operate
		}
		if ( (proc_count > 0) && (assigned_tier >= 0) ) {
			//
			LRU_cache *lru = g_tier_to_LRU[assigned_tier];
			if ( lru == NULL ) {
				return(-1);
			}
			//
			char **messages = messages_reserved;  // get this many addrs if possible...
			char **accesses = duplicate_reserved;
			//
			uint32_t condition_key = g_tier_to_CONDITION[tier];
			ConditionHolder *cond = g_CONDITION_per_segment[condition_key];
			if ( cond != NULL) {
				if ( cond->wait_on_timed(shared_state,DEFAULT_MICRO_TIMEOUT) ) {  // start working
					// 
					uint32_t ready_msg_count = 0;
					// 		OP on com buff
					// FIRST: gather messages that are aready for addition to shared data structures.
					//
					for ( uint32_t proc = 0; proc < proc_count; proc++ ) {
						//
						uint32_t p_offset = g_INTER_PROC_DESCRIPTOR_WORDS*(proc)*assigned_tier + g_SUPER_HEADER*g_NTiers;
						char *access_point = ((char *)g_com_buffer) + p_offset;
						char *read_marker = (access_point + OFFSET_TO_MARKER);

						//
						if ( (COM_BUFFER_STATE)(*read_marker) == CLEARED_FOR_ALLOC ) {
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
					// 		OP on com buff
					// SECOND: If duplicating, free the message slot, otherwise gather memory for storing new objecs
					//
					if ( ready_msg_count > 0 ) {  // a collection of message this process/thread will enque
						// 	-- FILTER
						pair<uint32_t,uint32_t> update = lru->filter_existence_check(messages,accesses,ready_msg_count);

						ready_msg_count = update.first;
						uint32_t duplicate_count = update.second
						while ( duplicate_count-- > 0 ) {
							char *access_point = accesses[duplicate_count];
							if ( access_point != NULL ) {
								char *read_marker = (access_point + OFFSET_TO_MARKER);
								clear_for_copy(read_marker);
							}
						}
						//
						if ( ready_msg_count > 0 ) {
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
							memset((void *)lru_element_offsets,0,sizeof(uint32_t)*(ready_msg_count+1)); // clear the buffer

							lru->claim_free_mem(ready_msg_count,lru_element_offsets); // negotiate getting a list from free memory
							//
							// if there are elements, they are already removed from free memory and this basket belongs to this process..
							if ( first ) {
								//
								uint32_t *current = lru_element_offsets;
								uint8_t *start = lru->_region;
								uint32_t offset = 0;
								//
								uint32_t N = ready_msg_count;
								//
								// map hashes to the offsets
								//
								while ( offset = *current++ ) {
									// read from com buf
									char *access_point = messages[--N];
									if ( access_point != nullptr ) {
										uint32_t *write_offset_here = (access_point + OFFSET_TO_OFFSET);
										uint64_t *hash_parameter = (uint64_t *)(access_point + OFFSET_TO_HASH);
										uint64_t hash64 = hash_parameter[0];
										//
										lru->add_key_value(hash64,offset);			// add to the hash table...
										write_offset_here[0] = offset;
										//
										char *read_marker = (access_point + OFFSET_TO_MARKER);
										clear_for_copy(read_marker);
									}
								}
								//
								lru->attach_to_lru_list(lru_element_offsets,ready_msg_count);  // attach to an LRU as a whole bucket...
							}
						}
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
		uint32_t condition_key = g_tier_to_CONDITION[tier];
		ConditionHolder *cond = g_CONDITION_per_segment[condition_key];
		if ( cond != NULL) {
			return cond->signal();
		}
		return false
	}

	/**
	 *
	*/
	NAN_METHOD(put)  {
		Nan::HandleScope scope;
		uint32_t process = Nan::To<uint32_t>(info[0]).FromJust();	// the process number
		uint32_t hash_bucket = Nan::To<uint32_t>(info[1]).FromJust();	// hash modulus the number of buckets (hence a bucket)
		uint32_t full_hash = Nan::To<uint32_t>(info[2]).FromJust();		// hash of the value
		bool updating = Nan::To<bool>(info[3]).FromJust();		// hash of the value
		//
		char* buffer = (char*) node::Buffer::Data(info[3]->ToObject());  // should be a reference...
    	unsigned int size = info[4]->Uint32Value();

		//
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
				uint32_t p_offset = g_INTER_PROC_DESCRIPTOR_WORDS*(process)*i + g_SUPER_HEADER*g_NTiers;
				char *access_point = ((char *)g_com_buffer) + p_offset;
				unsigned char *read_marker = (access_point + OFFSET_TO_MARKER);
				uint32_t *hash_parameter = (uint32_t *)(access_point + OFFSET_TO_HASH);
				uint32_t *offset_offset = (uint32_t *)(access_point + OFFSET_TO_OFFSET);
				//
				// Writing will take place after a place in the LRU has been given to this writer...
				// 
				// WAIT - a reader may be still taking data out of our slot.
				// let it finish before puting in the new stuff.
				if ( wait_to_write(read_marker) ) {	// will wait (spin lock style) on an atomic indicating the read state of the process
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
						info.GetReturnValue().Set(Nan::New<Boolean>(false));
						return;
					}
					// the write offset should come back to the process's read maker
					offset_offset[0] = updating ? UINT32_MAX : 0;
					//					
					if ( await_write_offset(read_marker,MAX_WAIT_LOOPS,4) ) {
						uint32_t write_offset = offset_offset[0];
						if ( (write_offset == UINT32_MAX) && !(updating) ) {	// a duplicate has been found
							clear_for_write(read_marker);   // next write from this process can now proceed...
							info.GetReturnValue().Set(Nan::New<Number>(-1));
							return;
						}
						//
						char *m_insert = lru->data_location(write_offset);
						memcpy(m_insert,buffer,min(size,MAX_MESSAGE_SIZE));
						//
						clear_for_write(read_marker);   // next write from this process can now proceed...
					} else {
						clear_for_write(read_marker);   // next write from this process can now proceed...
						info.GetReturnValue().Set(Nan::New<Number>(-1));
						return;
					}
				} else {
					// something went wrong ... perhaps a frozen reader...
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
		
		Nan::SetMethod(target, "get", get);
		Nan::SetMethod(target, "detach", detach);
		Nan::SetMethod(target, "detachAll", detachAll);
		Nan::SetMethod(target, "getTotalSize", getTotalSize);
		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
		Nan::SetMethod(target, "initLRU", initLRU);
		Nan::SetMethod(target, "getSegSize", getSegSize);
		Nan::SetMethod(target, "max_count", getMaxCount);
		Nan::SetMethod(target, "current_count", getCurrentCount);
		Nan::SetMethod(target, "free_count", getFreeCount);
		Nan::SetMethod(target, "epoch_time", time_since_epoch);

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
