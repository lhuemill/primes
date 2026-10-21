/*
 * primes/primes_util.h
 *
 * Copyright 2026 Louis Huemiller Jr.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <climits>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <stdio.h>
#include <unistd.h>

#ifndef _PRIMES_UTIL_H_
#define _PRIMES_UTIL_H_

#include "sparsebit_c.h"

namespace primes_util {
	class BitFile : public std::fstream
	{
		public:
			BitFile(void);
			~BitFile(void);
			std::vector<bool> read_bits(size_t num);
			void write_bits(std::vector<bool> bv);
			void flush(void);

		private:
			char _rdbyte;
			std::ptrdiff_t _rdbyte_idx;
			uint8_t _wrbuf[64];
			std::ptrdiff_t _wrbuf_idx;
	};

	class PrimesFile
	{
		public:
			enum Mode {
				CREATE = 0x9100,
				READ,
			};

			enum Format {
				SBIT000 = 0x0a73,
				PRIMES32 = 0x3332,
				PRIMES64 = 0x3634,
				SBIT = 0xa334,
				DELTA000 = 0xa71c,
				DELTA = 0xa830,
			};

			static const size_t headerlen = 8;
			static const std::vector<bool> PRIMESI0_OP_EVEN2x;
			static const std::vector<bool> PRIMESI0_OP_EVEN4x;
			static const std::vector<bool> PRIMESI0_OP_EVEN6x;
			static const std::vector<bool> PRIMESI0_OP_EVEN8x;
			static const std::vector<bool> PRIMESI0_OP_EVEN10x;
			static const std::vector<bool> PRIMESI0_OP_4x;
			static const std::vector<bool> PRIMESI0_OP_32x;
			static const std::vector<bool> PRIMESI0_OP_64x;
			static const std::vector<bool> PRIMESI0_OP_END;
			static const size_t PRIMESI0_OP_LEN_MAX;

			struct trait_runlen {
				std::vector<bool> opcode;
				bool even;
				uint64_t delta_min;
				uint64_t delta_max;
				size_t val_bits;
			};

			PrimesFile(Mode mode, const char *pathname, Format format = DELTA,
				uint64_t start = 0, uint64_t end = 0, uint64_t num = 0);
			~PrimesFile(void);
			const static struct trait_runlen runlen_traits[];
			bool good(void);
			Format format() { return _format; }
			uint64_t start() { return _start; }
			uint64_t end() { return _end; }
			uint64_t num() { return _num; }
			int close(void);
			std::vector<bool> get_bits(size_t num);
			void set_bit(uint64_t idx);
			uint64_t next_set(void);
			void dump(void);
			static std::string format_str(PrimesFile::Format format);

			std::vector<bool> _delta2bv(uint64_t delta);
			static int readline(FILE *file, std::string& result,
				size_t max  = std::numeric_limits<size_t>::max());

		private:
			Mode _mode;
			std::string _filename;
			BitFile _bitfile;
			Format _format;
			uint64_t _start = 0 - 1;
			uint64_t _end = 2;
			bool _range_set = false;
			uint64_t _lowest = 0;
			uint64_t _highest = 0;
			bool _lowest_highest_set = false;
			uint64_t _num = 0;
			uint64_t _current;
			bool _current_set = false;
			sb_t *_sb = NULL;
	};
} // namespace primes_util

#endif //  _PRIMES_UTIL_H_
