#pragma once


// SPARSE SLAB OPTION
//
// a wait based operation option without interleaving (hence, excess memory)
// fairly usual hash table... 


#include <iostream>
#include <atomic>
#include <map>
#include <stdexcept>
#include <string>

using namespace std;

#include "tick.h"
#include <sys/ipc.h>



// note: control threads will get an index to put into _which_cell;

class TokGenerator {
	public: 

		TokGenerator(void) {}
		virtual ~TokGenerator(void) {}

		void set_token_grist_list(const char **file_names,uint16_t max_files) {
			_max_files = max_files;
			_grist_list = file_names;
			_counter = 0;
		}

		key_t gen_slab_index() {
			if ( _counter >= _max_files ) return -1;
			return ftok(_grist_list[_counter++],1);
		}

		const char 		**_grist_list;
		uint16_t		_max_files{0};
		uint16_t		_counter{0};

};


