#pragma once

/*
worker_waters.h.
Copyright (C) 2024 Richard Leddy

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <bitset>
#include <iostream>
#include <string.h>
#include <atomic>
#include <chrono>
#include <thread>

using namespace std;
using namespace chrono;

#include "hmap_interface.h"


typedef struct R_ENTRY {
	uint32_t 	process;
	uint32_t	timestamp;
	uint32_t 	h_bucket;
	uint32_t	full_hash;
} r_entry;


template<uint16_t const ExpectedMax = 100>
class RemovalEntryHolder : public  SharedQueue_SRSW<r_entry,ExpectedMax> {};

template<const uint8_t MAX_TIERS = 8>
class WorkWaiters {

	public:

		WorkWaiters(void) {}
		virtual ~WorkWaiters(void) {}

	public:

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



		// Stop the process on a futex until notified...
		void		wait_for_data_present_notification(uint8_t tier,bool *thread_is_running) {
#ifndef __APPLE__
			_readerAtomicFlag[tier]->clear();
			_readerAtomicFlag[tier]->wait(false);  // this tier's LRU shares this read flag
#else
//cout << ((this->_thread_running[tier]) ? "running " : "not running ") << tier << endl;
//cout << "waiting..."; cout.flush();
			// FOR MAC OSX
			while ( _readerAtomicFlag[tier]->test_and_set(std::memory_order_acquire) && *thread_is_running) {
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


		// ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----


		// -- set_reader_atomic_tags
		/**
		 * set_reader_atomic_tags
		*/
		void 		set_reader_atomic_tags(atomic_flag *c_buffer,uint32_t n_tiers) {
			if ( c_buffer != nullptr ) {
				atomic_flag *af = (atomic_flag *)c_buffer;
				for ( uint32_t tier = 0; tier < n_tiers; tier++ ) {
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
		void		set_removal_atomic_tags(atomic_flag *c_buffer,uint32_t n_tiers) {
			if ( c_buffer != nullptr ) {
				atomic_flag *af = ((atomic_flag *)c_buffer) + n_tiers;
				for ( uint32_t i = 0; i < n_tiers; i++ ) {
					_removerAtomicFlag[i] = af;
					af++;
				}
			}
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

		// removal_waiting
		void		removal_waiting(uint8_t tier) {
			if ( _removal_work[tier].empty() ) {
				wait_for_removal_notification(tier);
			}
		}

		// add_work
		void add_work(uint8_t tier,r_entry &re) {
			_removal_work[tier].push(re);
		}
		
		// has_removal_work
		bool		has_removal_work(uint8_t tier) {
			return !_removal_work[tier].empty();
		}

		// get_work
		bool		get_work(uint8_t tier,r_entry &re) {
			return _removal_work[tier].pop(re);
		}

	protected:

		RemovalEntryHolder<>	_removal_work[MAX_TIERS];
		atomic_flag 			*_removerAtomicFlag[MAX_TIERS];
		atomic_flag 			*_readerAtomicFlag[MAX_TIERS];

};




class RestoreAndCropWaiters {
	public:

		RestoreAndCropWaiters(void) {
		}

		virtual ~RestoreAndCropWaiters(void) {}


		void initialize_waiters(void) {
			_sleeping_reclaimer.clear();  // atomic that pauses the relcaimer thread until set.
			_sleeping_cropper.clear();
		}


		/**
		 * wait_notification_restore - put the restoration thread into a wait state...
		*/
		void  wait_notification_restore() {
#ifndef __APPLE__
			do {
				_sleeping_reclaimer.wait(false);
			} while ( _sleeping_reclaimer.test(std::memory_order_acquire) );
#else
			while ( _sleeping_reclaimer.test_and_set() ) __libcpp_thread_yield();
#endif
		}

		/**
		 * wake_up_one_restore -- called by the requesting thread looking to have a value put back in the table
		 * after its temporary removal.
		*/
		void wake_up_one_restore(void) {
#ifndef __APPLE__
			do {
				_sleeping_reclaimer.test_and_set();
			} while ( !(_sleeping_reclaimer.test(std::memory_order_acquire)) );
			_sleeping_reclaimer.notify_one();
#else
			_sleeping_reclaimer.clear();
#endif
		}


		/**
		 * wait_notification_restore - put the restoration thread into a wait state...
		*/
		void  wait_notification_cropping() {
#ifndef __APPLE__
			do {
				_sleeping_cropper.wait(false);
			} while ( _sleeping_cropper.test(std::memory_order_acquire) );
#else
			while ( _sleeping_cropper.test_and_set() ) __libcpp_thread_yield();
#endif
		}


		/**
		 * wake_up_one_restore -- called by the requesting thread looking to have a value put back in the table
		 * after its temporary removal.
		*/
		void wake_up_one_cropping(void) {
#ifndef __APPLE__
			do {
				_sleeping_cropper.test_and_set();
			} while ( !(_sleeping_cropper.test(std::memory_order_acquire)) );
			_sleeping_cropper.notify_one();
#else
			_sleeping_cropper.clear();
#endif
		}



		void initialize_random_waiters(atomic_flag *start) {
			_rand_gen_thread_waiting_spinner = start;
			_random_share_lock = (atomic_flag *)(_rand_gen_thread_waiting_spinner + 1);
			//
			_rand_gen_thread_waiting_spinner->clear();
			_random_share_lock->clear();
		}


		void random_waiter_wait_for_signal(void) {
#ifndef __APPLE__
				do {
					_rand_gen_thread_waiting_spinner->wait(false);
				} while ( !_rand_gen_thread_waiting_spinner->test() );
#else
				while ( _rand_gen_thread_waiting_spinner->test_and_set() ) {
					thread_sleep(10);
				}
#endif
		}

		void random_waiter_notify(void) {
#ifndef __APPLE__
			while ( !(_rand_gen_thread_waiting_spinner->test_and_set()) );
			_rand_gen_thread_waiting_spinner->notify_one();
#else
			_rand_gen_thread_waiting_spinner->clear(std::memory_order_release);
#endif
		
		}

		void randoms_worker_lock() {
#ifndef __APPLE__
				while ( _random_share_lock->test() ) {  // if not cleared, then wait
					_random_share_lock->wait(true);
				};
				while ( !_random_share_lock->test_and_set() );
#else
				while ( _random_share_lock->test_and_set() ) {
					thread_sleep(10);
				};
#endif
		}


		void randoms_worker_unlock() {
#ifndef __APPLE__
			while ( _random_share_lock->test() ) {
				_random_share_lock->clear();
			};
			_random_share_lock->notify_one();
#else
			while ( _random_share_lock->test() ) {   // make sure it clears
				_random_share_lock->clear();
			};
#endif
		}


	public:

		atomic_flag						_sleeping_reclaimer;
		atomic_flag						_sleeping_cropper;

		atomic_flag		 				*_rand_gen_thread_waiting_spinner;
		atomic_flag		 				*_random_share_lock;


};