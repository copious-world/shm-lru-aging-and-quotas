#pragma once



#include <iostream>
#include <atomic>


using namespace std;




const uint8_t SLICE_COUNT{2};

typedef enum ACELL_PARTITION {
	ACELL_NO_OVERLAP_LEFT,
	ACELL_OVERLAP_LEFT,
	ACELL_MEDIANS,
	ACELL_OVERLAP_RIGHT,
	ACELL_NO_OVERLAP_RIGHT,
	ACELL_NUM_TYPES
} acells_names;


typedef struct A_RANGE {

	uint32_t	_lb{0};
	uint32_t	_ub{UINT32_MAX};
	uint8_t 	_count{0};				// count number of active ranges which is <= MAX_THREADS
	//
	uint8_t		child_row{UINT8_MAX};	// must be 0..MAX_THREADS or UINT8_MAX if not set.
} activity_range;


class ActivityRange {
	public:

		ActivityRange() {}
		virtual ~ActivityRange() {}

	public:

		uint32_t		global_lb{0};
		uint32_t		global_ub{UINT32_MAX};
		uint32_t		global_median{UINT32_MAX/2};

		uint8_t			_parent_row{0};

		activity_range	_row_cells[ACELL_NUM_TYPES];
};




class ThreadActivity {

	public:

		ThreadActivity() {}
		virtual ~ThreadActivity() {}


		void initialize() {}

		typedef enum TARWops {
			noop, r, d, w
		} ta_ops;

		ta_ops 		op;
		uint32_t 	lb;
		uint32_t	ub;

		// if the op
		uint32_t	match_key;
		uint32_t	value;

		uint32_t	base_bucket;

		atomic<uint32_t>	reader_lock; // ?? 
		atomic<uint32_t>	editor_lock; // ?? 

		bool intersects(ThreadActivity *check) {
			if ( check == nullptr ) return false;
			if ( this->ub < check->lb || check->ub < this->lb ) return false;
			return true;
		}

};




template<const uint8_t MAX_THREADS = 64>
class ThreadActivityCollection {
	public:

		ThreadActivityCollection() {}
		virtual ~ThreadActivityCollection() {}


		void setup_thread_activity(uint8_t *activity_region) {
			ThreadActivity *ta = (ThreadActivity *)activity_region;
			for ( uint8_t s = 0; s < SLICE_COUNT; s++ ) {
				for ( uint8_t t = 0; t < MAX_THREADS; t++ ) {
					ta->initialize();
					_all_activity[s][t] = ta++;
				}
			}
		}

		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

		void wait_on_overlap(uint8_t thread_id,uint8_t slice) {

			ThreadActivity *ta = _all_activity[slice][thread_id];

			for ( uint8_t t = 0; t < MAX_THREADS; t++ ) {
				if ( t != thread_id ) {
					ThreadActivity *ta2 = _all_activity[slice][t];
					if ( ta2->op != ThreadActivity::ta_ops::noop ) {
						while ( (ta2->op > ta->op) && ta2->intersects(ta)  ) {
							__libcpp_thread_yield();
						}
					}
				}
			}

		}


		void register_range(uint8_t thread_id,uint32_t h_bucket,uint32_t H,uint8_t slice) {
			//
			ThreadActivity *ta = _all_activity[slice][thread_id];
			ta->op = ThreadActivity::ta_ops::r;
			ta->lb = h_bucket;
			ta->ub = h_bucket + (sizeof(uint32_t) - countl_zero(H));

		}


		void set_activity_range_op(uint8_t thread_id,uint32_t lb,uint32_t ub,uint8_t slice,ta_ops op) {
			ThreadActivity *ta = _all_activity[slice][thread_id];
			ta->op = op;
			ta->lb = lb;
			ta->ub = ub;
			for ( uint8_t t = 0; t < MAX_THREADS; t++ ) {
				if ( t != thread_id ) {
					ThreadActivity *ta2 = _all_activity[slice][t];
					if ( (ta2->op > ta->op) && ta2->intersects(ta)  ) {
						ta2->editor_lock.fetch_add(1);
					}
				}
			}
		}


		void clear_activity_range_op(uint8_t thread_id,uint32_t lb,uint32_t ub,uint8_t slice,ta_ops op) {
			ThreadActivity *ta = _all_activity[slice][thread_id];
			ta->op = ThreadActivity::ta_ops::noop;
			ta->lb = UINT32_MAX;
			ta->ub = UINT32_MAX;
			for ( uint8_t t = 0; t < MAX_THREADS; t++ ) {
				if ( t != thread_id ) {
					ThreadActivity *ta2 = _all_activity[slice][t];
					if ( (ta2->op > ta->op) && ta2->intersects(ta)  ) {
						ta2->editor_lock.fetch_sub(1);
					}
				}
			}
		}



		/**
		 * search_usurping_overlaps
		 * 
		 * 	Last ditch search for a value in motion to its final position.
		 * 	The overlap in the process of puting in a new value. If it is not yet installed, it might be in 
		 * 	one of the registered writes that overlaps with the search range.
		*/
		bool search_usurping_overlaps(uint32_t el_key,uint32_t &value,uint8_t thread_id,uint8_t selector) {
			ThreadActivity **ta = &(_all_activity[selector]);
			ThreadActivity *thrd_req = (ta + thread_id)[0];
			for ( uint8_t t = 0; t < MAX_THREADS; t++ ) {
				if ( t != thread_id ) {
					ThreadActivity *thrd = (ta + i)[0];
					if ( thrd->op == ThreadActivity::ta_ops::w ) {
						if ( thrd->intersects(thrd_req)  ) {
							if ( thrd->match_key == el_key ) {
								value = thrd->value;
								return true;
							}
						}
					}
				}
			}
			return false;
		}



		ThreadActivity					*_all_activity[SLICE_COUNT][MAX_THREADS];

};