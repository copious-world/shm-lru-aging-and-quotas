#pragma once



#include <iostream>
#include <atomic>


using namespace std;


typedef struct TIER_TIME {
	atomic<uint32_t>	*_lb_time;
	atomic<uint32_t>	*_ub_time;
} Tier_time_bucket, *Tier_time_bucket_ref;


// b_search 
//
static inline uint32_t
time_interval_b_search(uint32_t timestamp, Tier_time_bucket_ref timer_table,uint32_t N) {
	Tier_time_bucket_ref beg = timer_table;
	Tier_time_bucket_ref end = timer_table + N;
	//
	// check inclusion in the first bucket
	if ( (beg->_lb_time->load() <= timestamp ) && (timestamp < beg->_ub_time->load()) ) return 0;
	beg++; N--;
	// check inclusino in the last bucket (_ub_time will be MAX_INT for the most recent bucket)
	if ( ((end-1)->_lb_time->load() <= timestamp ) && (timestamp < (end-1)->_ub_time->load()) ) return N;
	end--; N--;
	//
	while ( beg < end ) {
		N = N >> 1;
		if (  N <= 1 ) { //
			while ( beg < end ) {
				if ( (beg->_lb_time->load() <= timestamp ) && (timestamp < beg->_ub_time->load()) ) { return (beg - timer_table); }
				beg++;
			}
			break;
		}
		//
		Tier_time_bucket_ref mid = beg + N;
		if ( mid >= end ) break;
		//
		uint32_t mid_lb, mid_ub;
		mid_lb = mid->_lb_time->load();
		mid_ub = mid->_ub_time->load();
		//
		if ( (mid_lb <= timestamp ) && (timestamp < mid_ub) ) { return (mid - timer_table); }
		if ( timestamp >= mid_ub ) beg = mid;
		else end = mid;
	}
	return UINT32_MAX;
}

