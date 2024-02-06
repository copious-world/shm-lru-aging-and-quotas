#pragma once



#include <iostream>
#include <atomic>


using namespace std;


typedef struct TIER_TIME {
	atomic<uint32_t>	*lb_time;
	atomic<uint32_t>	*ub_time;
} Tier_time_bucket, *Tier_time_bucket_ref;




// b_search 
//
static inline uint32_t
time_interval_b_search(uint32_t timestamp, Tier_time_bucket_ref timer_table,uint32_t N) {
	Tier_time_bucket_ref beg = timer_table;
	Tier_time_bucket_ref end = timer_table + N;
	//
	if ( (beg->lb_time->load() <= timestamp )&& (timestamp < beg->ub_time->load()) ) return 0;
	beg++; N--;
	if ( ((end-1)->lb_time->load() <= timestamp )&& (timestamp < (end-1)->ub_time->load()) ) return N-1;
	end--; N--;
	//
	while ( beg < end ) {
		N = N >> 1;
		if ( N == 0 ) {
			while ( beg < end ) {
				if ( (beg->lb_time->load() <= timestamp )&& (timestamp < beg->ub_time->load()) ) return beg;
				beg++;
			}
			break;
		}
		if ( mid >= end ) break;
		Tier_time_bucket_ref mid = beg + N;
		//
		uint32_t mid_lb, mid_ub;
		mid_lb = mid->lb_time->load();
		mid_ub = mid->ub_time->load();
		//
		if ( (mid_lb <= timestamp ) && (timestamp < mid_ub) ) return mid;
		if ( timestamp > mid_ub ) beg = mid;
		else end = mid;
	}
	return UINT32_MAX;
}

