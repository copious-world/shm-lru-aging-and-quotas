
//hh_queues_and_states.h

#pragma once


#include "hmap_interface.h"
#include "slab_provider.h"


// USING
using namespace std;
// 


typedef enum {
	HH_FROM_EMPTY,			// from zero 
	HH_FROM_BASE,			// overwritten and old value queued  (wait during overwrite)
	HH_FROM_BASE_AND_WAIT,	// queued no overwrite (no wait)
	HH_USURP,			// specifically a member is being overwritten
	HH_USURP_BASE,		// secondary op during usurp (may not need to wait)
	HH_DELETE, 			// more generally members
	HH_DELETE_BASE,		// specifically rewiting the base or completely erasing it
	HH_ADDER_STATES
} hh_adder_states;




/** 
 * q_entry is struct Q_ENTRY
*/
template<typename T_element>
struct Q_ENTRY {
	public:
		//
		atomic<uint32_t>	*control_bits;
		T_element 			*hash_ref;
		//
		uint32_t 			cbits;
		uint32_t 			cbits_op;
		uint32_t 			cbits_op_base;
		uint32_t 			h_bucket;
		uint32_t			el_key;
		uint32_t			value;
		T_element			*buffer;
		T_element			*end;
		uint8_t				hole;
		uint8_t				which_table;
		hh_adder_states		update_type;
		//
};


typedef struct Q_ENTRY<hh_element> q_entry;
typedef struct Q_ENTRY<sp_element> sp_q_entry;



/** 
 * crop_entry is struct C_ENTRY
*/
template<typename T_element>
struct C_ENTRY {
	public:
		//
		T_element 		*hash_ref;
		T_element		*buffer;
		T_element		*end;
		uint32_t 		cbits;
		uint8_t			which_table;
		//
};


typedef struct C_ENTRY<hh_element> crop_entry;
typedef struct C_ENTRY<sp_element> sp_crop_entry;


/**
 * QueueEntryHolder uses q_entry in SharedQueue_SRSW<q_entry,ExpectedMax>
*/

template<typename T_element,uint16_t const ExpectedMax = 100>
class QueueEntryHolder : public  SharedQueue_SRSW<T_element,ExpectedMax> {

	bool		compare_key(uint32_t key, T_element *el,uint32_t &value) {
		uint32_t ky = el->el_key;
		value = el->value;
		return (ky == key);
	}

};



/**
 * QueueEntryHolder uses q_entry in SharedQueue_SRSW<q_entry,ExpectedMax>
 * 
 * 
*/

template<typename T_element,uint16_t const ExpectedMax = 64>
class CropEntryHolder : public  SharedQueue_SRSW<T_element,ExpectedMax> {
};



template<typename Q_element,typename C_element>
struct PRODUCT_DESCR {
	//
	uint32_t						        partner_thread;
	uint32_t						        stats;
	QueueEntryHolder<Q_element>				_process_queue[2];
	CropEntryHolder<C_element>				_to_cropping[2];

};


typedef struct PRODUCT_DESCR<q_entry,crop_entry> proc_descr;
typedef struct PRODUCT_DESCR<sp_q_entry,sp_crop_entry> sp_proc_descr;



inline uint8_t get_b_offset_update(uint32_t &c) {
	uint8_t offset = countr_zero(c);
	c = c & (~((uint32_t)0x1 << offset));
	return offset;
}





inline uint8_t get_b_reverse_offset_update(uint32_t &c) {
	uint8_t offset = countl_zero(c);
	c = c & (~((uint32_t)0x1 << offset));
	return offset;
}


inline uint8_t search_range(uint32_t c) {
	uint8_t max = countl_zero(c);
	return max;
}


// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----

static constexpr uint32_t zero_levels[32] {
	~(0xFFFFFFFF << 1), ~(0xFFFFFFFF << 2), ~(0xFFFFFFFF << 3), ~(0xFFFFFFFF << 4), ~(0xFFFFFFFF << 5),
	~(0xFFFFFFFF << 6), ~(0xFFFFFFFF << 7), ~(0xFFFFFFFF << 8), ~(0xFFFFFFFF << 9), ~(0xFFFFFFFF << 10),
	~(0xFFFFFFFF << 11),~(0xFFFFFFFF << 12),~(0xFFFFFFFF << 13),~(0xFFFFFFFF << 14),~(0xFFFFFFFF << 15),
	~(0xFFFFFFFF << 16),~(0xFFFFFFFF << 17),~(0xFFFFFFFF << 18),~(0xFFFFFFFF << 19),~(0xFFFFFFFF << 20),
	~(0xFFFFFFFF << 21),~(0xFFFFFFFF << 22),~(0xFFFFFFFF << 23),~(0xFFFFFFFF << 24),~(0xFFFFFFFF << 25),
	~(0xFFFFFFFF << 26),~(0xFFFFFFFF << 27),~(0xFFFFFFFF << 28),~(0xFFFFFFFF << 29),~(0xFFFFFFFF << 30),
	~(0xFFFFFFFF << 31), 0xFFFFFFFF
};


static constexpr uint32_t one_levels[32] {
	(0xFFFFFFFF << 1), (0xFFFFFFFF << 2), (0xFFFFFFFF << 3), (0xFFFFFFFF << 4), (0xFFFFFFFF << 5),
	(0xFFFFFFFF << 6), (0xFFFFFFFF << 7), (0xFFFFFFFF << 8), (0xFFFFFFFF << 9), (0xFFFFFFFF << 10),
	(0xFFFFFFFF << 11),(0xFFFFFFFF << 12),(0xFFFFFFFF << 13),(0xFFFFFFFF << 14),(0xFFFFFFFF << 15),
	(0xFFFFFFFF << 16),(0xFFFFFFFF << 17),(0xFFFFFFFF << 18),(0xFFFFFFFF << 19),(0xFFFFFFFF << 20),
	(0xFFFFFFFF << 21),(0xFFFFFFFF << 22),(0xFFFFFFFF << 23),(0xFFFFFFFF << 24),(0xFFFFFFFF << 25),
	(0xFFFFFFFF << 26),(0xFFFFFFFF << 27),(0xFFFFFFFF << 28),(0xFFFFFFFF << 29),(0xFFFFFFFF << 30),
	(0xFFFFFFFF << 31), 0
};

//
/**
 * zero_above
 */
static inline uint32_t zero_above(uint8_t hole) {
	if ( hole >= 31 ) {
		return  0xFFFFFFFF;
	}
	return zero_levels[hole];
}


/**
 * ones_above
 */

		//
static inline uint32_t ones_above(uint8_t hole) {
	if ( hole >= 31 ) {
		return  0;
	}
	return one_levels[hole];
}



/**
 * el_check_end
*/

static inline hh_element *el_check_end(hh_element *ptr, hh_element *buffer, hh_element *end) {
	if ( ptr >= end ) return buffer;
	return ptr;
}


/**
 * el_check_beg_wrap
*/

static inline hh_element *el_check_beg_wrap(hh_element *ptr, hh_element *buffer, hh_element *end) {
	if ( ptr < buffer ) return (end - buffer + ptr);
	return ptr;
}


/**
 * el_check_end_wrap
*/

template<typename T_element>
static inline T_element *el_check_end_wrap(T_element *ptr, T_element *buffer, T_element *end) {
	if ( ptr >= end ) {
		uint32_t diff = (ptr - end);
		return buffer + diff;
	}
	return ptr;
}





typedef enum {
	HH_OP_NOOP,
	HH_OP_CREATION,
	HH_OP_USURP,
	HH_OP_MEMBER_IN,
	HH_OP_MEMBER_OUT,
	HH_ALL_OPS
} hh_creation_ops;


