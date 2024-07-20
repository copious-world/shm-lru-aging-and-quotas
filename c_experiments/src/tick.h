#pragma once
/*
TICK.
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

// THREAD CONTROL

inline void tick() {
	this_thread::sleep_for(chrono::nanoseconds(20));
}

inline void thread_sleep(uint8_t ticks) {
	microseconds us = microseconds(ticks);
	auto start = high_resolution_clock::now();
	auto end = start + us;
	do {
		std::this_thread::yield();
	} while ( high_resolution_clock::now() < end );
}


static inline uint32_t now_time() {
	//
	uint32_t nowish = 0;
	const auto right_now = chrono::system_clock::now();
	nowish = chrono::system_clock::to_time_t(right_now);
	//
	return nowish;
}

