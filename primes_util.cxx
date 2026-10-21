/*
 * primes/primes_util.cxx
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

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#include <assert.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "primes_util.h"
#include "sparsebit_c.h"

#define MEMCLEAR(p, n) memset((p), 0, (n))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define CONST_STRLEN(str) (ARRAY_SIZE(str) - 1)

using namespace std;
using namespace primes_util;

BitFile::BitFile(void)
{
	_rdbyte_idx = CHAR_BIT;
	MEMCLEAR(_wrbuf, sizeof(_wrbuf));
	_wrbuf_idx = 0;
}

BitFile::~BitFile(void)
{
}

std::vector<bool> BitFile::read_bits(size_t num)
{
	std::vector<bool> bv;

	// While bits to be read.
	while (num) {
		// If buffer is empty, fill it with another byte of data.
		if (_rdbyte_idx >= CHAR_BIT) {
			this->read(&_rdbyte, 1);
			_rdbyte_idx = 0;
		}

		char mask = (1 << (CHAR_BIT - 1)) >> _rdbyte_idx;
		bv.push_back(_rdbyte & mask);

		_rdbyte_idx++;
		num--;
	}

	return bv;
}

void BitFile::write_bits(vector<bool> bv)
{
	// For each of the bits
	for (auto bit : bv) {
		// If buffer is full, write its contents to the file.
		if (_wrbuf_idx == (sizeof(_wrbuf) * CHAR_BIT)) {
			this->write((const char *) _wrbuf, sizeof(_wrbuf));
			MEMCLEAR(_wrbuf, sizeof(_wrbuf));
			_wrbuf_idx = 0;
		}

		// Add bit to _wrbuf.
		if (bit) {
			size_t byte_offset = _wrbuf_idx / CHAR_BIT;
			size_t bit_offset = _wrbuf_idx % CHAR_BIT;
			_wrbuf[byte_offset] |= (1 << (CHAR_BIT - (1 + bit_offset)));
		}

		_wrbuf_idx++;
	}
}

void BitFile::flush(void)
{
	if (_wrbuf_idx > 0) {
		size_t num = ((_wrbuf_idx - 1) / CHAR_BIT) + 1;
		this->write((const char *) _wrbuf, num);
	}

	_wrbuf_idx = 0;
}

// PRIMESIO_OP_EVEN2x  00xx            - 0x2   + xx * 2
// PRIMESIO_OP_EVEN4x  01xxxx          - 0xa   + xxxx * 2
// PRIMESIO_OP_EVEN6x  100xxxxxx       - 0x2a  + xxxxxx * 2
// PRIMESIO_OP_EVEN8x  101xxxxxxxx     - 0xaa  + xxxxxxxx * 2
// PRIMESIO_OP_EVEN10x 1100xxxxxxxxxx  - 0x2aa + xxxxxxxxxx * 2
// PRIMESIO_OP_4x      1101xxxx        - xxxx
// Unused              11100
// Unused              11101
// Unused              111100
// Unused              111101
// PRIMESIO_OP_32x     1111100(32 x's) - (32 x's)
// PRIMESIO_OP_64x     1111101(64 x's) - (64 x's)
// PRIMESIO_OP_END     111111          - End

const vector<bool> PrimesFile::PRIMESI0_OP_EVEN2x = {false, false};
const vector<bool> PrimesFile::PRIMESI0_OP_EVEN4x = { false, true };
const vector<bool> PrimesFile::PRIMESI0_OP_EVEN6x = { true, false, false };
const vector<bool> PrimesFile::PRIMESI0_OP_EVEN8x = { true, false, true };
const vector<bool> PrimesFile::PRIMESI0_OP_EVEN10x
	= { true, true, false, false };
const vector<bool> PrimesFile::PRIMESI0_OP_4x = { true, true, false, true };
const vector<bool> PrimesFile::PRIMESI0_OP_32x
     = { true, true, true, true, true, false, false };
const vector<bool> PrimesFile::PRIMESI0_OP_64x
	= { true, true, true, true, true, false, true };
const vector<bool> PrimesFile::PRIMESI0_OP_END
	= { true, true, true, true, true, true };
const size_t PrimesFile::PRIMESI0_OP_LEN_MAX
	= PrimesFile::PRIMESI0_OP_64x.size();

// Even range start is equal to previous Even range end + 2.
// Even range end equals (range_start + ((1 << val_bits) - 1) * 2).
const struct PrimesFile::trait_runlen PrimesFile::runlen_traits[] = {
  /*    Opcode          Even   Start          End       */
	{PRIMESI0_OP_EVEN2x,  true,    0x2,                0x8,  2},
	{PRIMESI0_OP_EVEN4x,  true,    0xa,               0x28,  4},
	{PRIMESI0_OP_EVEN6x,  true,   0x2a,               0xa8,  6},
	{PRIMESI0_OP_EVEN8x,  true,   0xaa,              0x2a8,  8},
	{PRIMESI0_OP_EVEN10x, true,  0x2aa,              0xaa8, 10},
	{PRIMESI0_OP_4x,      false,   0x0,                0xf,  4},
	{PRIMESI0_OP_32x,     false,   0x0,         0xffffffff, 32},
	{PRIMESI0_OP_64x,     false,   0x0, 0xffffffffffffffff, 64}
};

PrimesFile::PrimesFile(Mode mode, const char *path, Format format,
	uint64_t start, uint64_t end, uint64_t num)
{
	int rv;
	size_t sz;
	ostringstream err_stream;

	_mode = mode;
	_filename = string(path);
	_format = format;
	_start = start;
	_end = end;
	_num = num;

	if (_mode == CREATE) {
		// Create or truncate the file.
		_bitfile.open(path, ios_base::in | ios_base::out
			| ios_base::binary | ios_base::trunc);
		if (!_bitfile.good()) {
			ostringstream err_stream;
			err_stream << "Error while trying to create: " << path << endl;
			err_stream << "    errno: " << errno;
			throw runtime_error(err_stream.str());
		}

		// Write format header.
		switch (format) {
		case SBIT:
throw runtime_error("PrimesFile Create does not support SBIT format");
			break;

		case DELTA: {
_current = start;
_current_set = true;
			const char header[] = "primes";
			_bitfile << header;
			uint16_t buf = htons(format);
			_bitfile.write((char *) &buf, sizeof(buf));
// TODO: Eliminate all code that allows start, or end to be changed after
//       construction. This then leaves only num as a place-holder. Needed
//       so that first delta is from start, instead of from 0.
			// Write place holders for start, end, and num.
			uint64_t val = 0;
			for (unsigned int i = 0; i < 3; i++) {
				switch (i) {
				case 0:
					val = _start;
					break;

				case 1:
					val = _end;
					break;

				case 2:
					val = _num;
					break;
				}
				uint32_t buf;
				assert(sizeof(val) == 8);
				buf = htonl(val >> 32);
				_bitfile.write((const char *) &buf, sizeof(buf));
				buf = htonl(val & ((1ull << 32) - 1));
				_bitfile.write((const char *) &buf, sizeof(buf));
			}

			break;
		}

		case DELTA000: {
_current = 0;
_current_set = true;
			const char header[] = "primes";
			_bitfile << header;
			uint16_t buf = htons(format);
			_bitfile.write((char *) &buf, sizeof(buf));

			// Write place holders for start, end, and num.
			uint64_t val = 0;
			for (unsigned int i = 0; i < 3; i++) {
				switch (i) {
				case 0:
					val = _start;
					break;

				case 1:
					val = _end;
					break;

				case 2:
					val = _num;
					break;
				}
				uint32_t buf;
				assert(sizeof(val) == 8);
				buf = htonl(val >> 32);
				_bitfile.write((const char *) &buf, sizeof(buf));
				buf = htonl(val & ((1ull << 32) - 1));
				_bitfile.write((const char *) &buf, sizeof(buf));
			}

			break;
		}

		case PRIMES32: {
			const char header[] = "primes";
			_bitfile << header;
			uint16_t buf = htons(format);
			_bitfile.write((char *) &buf, sizeof(buf));

			// Write place holders for start, end, and num.
			uint32_t val = 0;
			for (unsigned int i = 0; i < 3; i++) {
				switch (i) {
				case 0:
					val = _start;
					break;

				case 1:
					val = _end;
					break;

				case 2:
					val = _num;
					break;
				}
				uint32_t buf;
				assert(sizeof(val) == 4);
				buf = htonl(val);
				_bitfile.write((const char *) &buf, sizeof(buf));
			}
			break;
		}

		default:
			err_stream << "Unknown format of: " << format; 
			throw invalid_argument(err_stream.str());

			break;
		}
	} else if (mode == READ) {
		// Open file.
		_bitfile.open(path, ios_base::in | ios_base::binary);
		if (!_bitfile.good()) {
			ostringstream err_stream;
			err_stream << "Error while trying to open: " << path << endl;
			err_stream << "    errno: " << errno;
			throw runtime_error(err_stream.str());
		}

		// Read header
		char header_buf[PrimesFile::headerlen];
 		_bitfile.read(header_buf, sizeof(header_buf));
		string header_str(header_buf, 6);
		if (header_str != "primes") {
			ostringstream err_stream;
			err_stream << "PrimesFile Read - Unexpected header id of: "
				<< header_str;
			throw runtime_error(err_stream.str());
		}
		PrimesFile::Format read_format = (PrimesFile::Format)
    	ntohs(*((uint16_t *) &header_buf[6]));
		_format = read_format;
		switch (read_format) {
		case PrimesFile::SBIT000: {
			_bitfile.seekp(7, ios_base::beg);
			do {
				string line;
				getline(_bitfile, line);
				if (line.length() == 0) { break; } // Blank line marks end of parameters
				size_t pos = line.find_first_of(":");
				string param_name = line.substr(0, pos);
				string param_val = line.substr(pos + 2);
				char *endptr;
				uint64_t val;
				val = strtoull(param_val.c_str(), &endptr, 0);

				if (param_name == "start") {
					_start = val;
				} else if (param_name == "end") {
					_end = val;
				} else if (param_name == "num") {
					_num = val;
				}
			} while (true);

			FILE *file_c = fopen(path, "rb");
			if (file_c == NULL) {
				err_stream << "Failure doing fopen on: " << path
					<< ", errno: " << errno;
				throw runtime_error(err_stream.str());
			}
			rv = fseeko(file_c, _bitfile.tellg(), SEEK_SET);
			if (rv == -1) {
				err_stream << "Failure using fseeko on: " << path
					<< ", errno: " << errno;
				throw runtime_error(err_stream.str());
			}
			_sb = sb_create();
			if (_sb == NULL) {
				err_stream << "Failure creating sparsebit for: " << path
					<< ", errno: " << errno;
				throw runtime_error(err_stream.str());
			}
			rv = sb_load(file_c, _sb);
			if (rv != 0) {
				err_stream << "Failure loading sparsebit for: " << path
					<< ", errno: " << errno;
				throw runtime_error(err_stream.str());
			}
			break;
		}

		case PrimesFile::PRIMES32: {
			// Read start, end, and num parameters.
			uint32_t val_buf;
			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_start = (uint64_t) ntohl(val_buf);

			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_end = (uint64_t) ntohl(val_buf);

			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_num = (uint64_t) ntohl(val_buf);
			break;
		}

		case PrimesFile::PRIMES64:
		case PrimesFile::SBIT:
		case PrimesFile::DELTA:
		case PrimesFile::DELTA000: {
			// Read start, end, and num parameters.
			uint32_t val_buf;
			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_start = ((uint64_t) ntohl(val_buf)) << 32;
			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_start |= ((uint64_t) ntohl(val_buf));

			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_end = ((uint64_t) ntohl(val_buf)) << 32;
			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_end |= ((uint64_t) ntohl(val_buf));

			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_num = ((uint64_t) ntohl(val_buf)) << 32;
			_bitfile.read((char *) &val_buf, sizeof(val_buf));
			_num |= ((uint64_t) ntohl(val_buf));
			break;
		}

		default:
			err_stream << "Unknown format of: 0x" << hex << read_format << dec; 
			throw invalid_argument(err_stream.str());
		}
	}
}

PrimesFile::~PrimesFile(void)
{
	int rv;
	ostringstream err_stream;

	if (_bitfile.is_open()) {
		try {
			rv = this->close();
			if (rv != 0) { exit(100); }
		}
		catch (...) {
			exit(101);
		}
	}
}

bool PrimesFile::good(void)
{
	return _bitfile.good();
}

int PrimesFile::close(void)
{
	int rv;
	size_t sz;
	ostringstream err_stream;

	switch (_format) {
	case SBIT:

		break;

	case DELTA: {
		uint32_t val;

		if (_mode == CREATE) {
			// Write end
			_bitfile.write_bits(PRIMESI0_OP_END);
			_bitfile.flush();

			auto orig_pos = _bitfile.tellp();

			// Seek to start of file parameters.
			_bitfile.seekp(8, ios_base::beg);

// TODO: Skip instead of rewrite start and end values.
			// Write start paramter value.
			val = htonl(_start >> 32);
			_bitfile.write((const char *) &val, sizeof(val));
			val = htonl(_start & ((uint64_t) 1 << 32) - 1);
			_bitfile.write((const char *) &val, sizeof(val));

			// Write end paramter value.
			val = htonl(_end >> 32);
			_bitfile.write((const char *) &val, sizeof(val));
			val = htonl(_end & ((uint64_t) 1 << 32) - 1);
			_bitfile.write((const char *) &val, sizeof(val));

			// Write num paramter value.
			val = htonl(_num >> 32);
			_bitfile.write((const char *) &val, sizeof(val));
			val = htonl(_num & ((uint64_t) 1 << 32) - 1);
			_bitfile.write((const char *) &val, sizeof(val));

			_bitfile.seekp(orig_pos, ios_base::beg);
		}

		if (!_bitfile) { return -1; }

		break;
	}

	case DELTA000: {
		uint32_t val;

		if (_mode == CREATE) {
			// Write end
			_bitfile.write_bits(PRIMESI0_OP_END);
			_bitfile.flush();

			auto orig_pos = _bitfile.tellp();

			// Seek to start of file parameters.
			_bitfile.seekp(8, ios_base::beg);

			// Write start paramter value.
			val = htonl(_start >> 32);
			_bitfile.write((const char *) &val, sizeof(val));
			val = htonl(_start & ((uint64_t) 1 << 32) - 1);
			_bitfile.write((const char *) &val, sizeof(val));

			// Write end paramter value.
			val = htonl(_end >> 32);
			_bitfile.write((const char *) &val, sizeof(val));
			val = htonl(_end & ((uint64_t) 1 << 32) - 1);
			_bitfile.write((const char *) &val, sizeof(val));

			// Write num paramter value.
			val = htonl(_num >> 32);
			_bitfile.write((const char *) &val, sizeof(val));
			val = htonl(_num & ((uint64_t) 1 << 32) - 1);
			_bitfile.write((const char *) &val, sizeof(val));

			_bitfile.seekp(orig_pos, ios_base::beg);
		}

		if (!_bitfile) { return -1; }

		break;
	}

	default:
		break;
	}

	_bitfile.close();

	return 0;
}

vector<bool> PrimesFile::get_bits(size_t num)
{
	vector<bool> bv;

	bv = _bitfile.read_bits(num);

	return bv;
}

void PrimesFile::set_bit(uint64_t idx)
{
	ostringstream err_stream;

	if (_range_set) {
		if (idx < _start) {
			err_stream << "set idx of " << idx << " less than range start of "
				<< _start;
			throw invalid_argument(err_stream.str());
		}
		if (idx > _end) {
			err_stream << "set idx of " << idx << " greater than range end of "
				<< _end;
			throw invalid_argument(err_stream.str());
		}
	} else {
		if (idx < _start) { _start = idx; }
		if (idx > _end) { _end = idx; }
	}

	if (!_lowest_highest_set) {
		_lowest = idx;
		_highest = idx;
		_lowest_highest_set = true;
	}

	if (idx > _highest) { _highest = idx; }

	if (idx > _end) {
		assert(!_range_set);
		_end = idx;
	}

	if (_current_set && (idx <= _current)) {
		err_stream << "PrimesFile::set idx <= _current, idx: "
			<< hex << "0x" << idx
			<< " _current: " << hex << "0x" << _current << dec;
		throw runtime_error(err_stream.str());
	}

	switch (_format) {
	case PRIMES32: {
		if (_current_set && (idx <= _current)) {
			err_stream << "Index <= _current, idx: 0x"
				<< hex << idx << dec;
			throw runtime_error(err_stream.str());
		}
		if (idx >= (1ull << 32)) {
			err_stream << "set_bit() idx to great, idx: 0x"
				<< hex << idx << dec;
			throw runtime_error(err_stream.str());
		}
		uint32_t val = (uint32_t) idx;
		uint32_t buf = htonl(val);
		_bitfile.write((const char *) &buf, sizeof(buf));
		break;
	}

	case DELTA: {
assert(_current_set);
		uint64_t delta = idx - _current;
		vector<bool> bv = _delta2bv(delta);
		_bitfile.write_bits(bv);

		break;
	}

	case DELTA000: {
assert(_current_set);
		uint64_t delta = idx - _current;
		vector<bool> bv = _delta2bv(delta);
		_bitfile.write_bits(bv);

		break;
	}

	default:
		err_stream << "Unknown formation, format: 0x" << hex << _format << dec;
		throw runtime_error(err_stream.str());
	}

	_current = idx;
	_current_set = true;

	_num++;
}

vector<bool> PrimesFile::_delta2bv(uint64_t delta)
{
	vector<bool> bv;

	bool is_even = (delta % 2) == 0;

	for (auto& trait : PrimesFile::runlen_traits) {
		if (trait.even && !is_even) { continue; }

		if ((delta >= trait.delta_min) && (delta <= trait.delta_max)) {
			uint64_t val = delta - trait.delta_min;
			if (trait.even) { val /= 2; }

			bv = trait.opcode;
			for (uint64_t mask = 1ull << (trait.val_bits - 1); mask != 0;
				mask = mask >> 1) {
				bv.push_back(mask & val);
			}

			return bv;
		}
	}

	assert(false); // Not Reached
}

uint64_t PrimesFile::next_set(void)
{
	switch (_format) {
	case PrimesFile::SBIT000:
	case PrimesFile::SBIT:
		if (!_current_set) {
			if (sb_anyset(_sb)) {
				_current = sb_firstset(_sb);
				_current_set = true;
				return _current;
			} else {
				throw out_of_range("No set bits");
			}
		}
		_current = sb_nextset(_sb, _current);
		if (_current != 0) {
			return _current;
		}
		throw out_of_range("No more set bits");
		break;

	case PrimesFile::DELTA: {
		if (!_current_set) { _current = _start; _current_set = true; }
		vector<bool> opcode;

		// While no matching opcode;
		bool again = false;
		do {
			// Add another bit to the current partial opcode;
			vector<bool> bit = _bitfile.read_bits(1);
			if (_bitfile.bad()) {
				ostringstream err_stream;
				err_stream << "Error while reading from file: " << _filename;
				throw runtime_error(err_stream.str());
			}
			if (_bitfile.eof()) {
				ostringstream err_stream;
				err_stream << "Unexpected EOF from file: " << _filename;
				throw runtime_error(err_stream.str());
			}
			opcode.push_back(bit[0]);

			again = false;
			for (auto& trait : PrimesFile::runlen_traits) {
				if (trait.opcode.size() > opcode.size()) { again = true; }
				if (opcode == trait.opcode) {
					vector<bool> val_bv = _bitfile.read_bits(trait.val_bits);
					uint64_t val = 0;
					for (auto bit : val_bv) { val = (val << 1) | bit; }
					if (trait.even) { val *= 2; }
					uint64_t delta = trait.delta_min + val;
					_current += delta;

					return _current;
				}
			}

			if (opcode == PRIMESI0_OP_END) {
				throw out_of_range("No more set bits");
			}
		} while (again);

		ostringstream err_stream;
		err_stream << "Unknown RunLen opcode of: ";
			for (auto bit: opcode) { err_stream << bit; }
		throw runtime_error(err_stream.str());
		break;
	}

	case PrimesFile::DELTA000: {
		if (!_current_set) { _current = 0; _current_set = true; }
		vector<bool> opcode;

		// While no matching opcode;
		bool again = false;
		do {
			// Add another bit to the current partial opcode;
			vector<bool> bit = _bitfile.read_bits(1);
			if (_bitfile.bad()) {
				ostringstream err_stream;
				err_stream << "Error while reading from file: " << _filename;
				throw runtime_error(err_stream.str());
			}
			if (_bitfile.eof()) {
				ostringstream err_stream;
				err_stream << "Unexpected EOF from file: " << _filename;
				throw runtime_error(err_stream.str());
			}
			opcode.push_back(bit[0]);

			again = false;
			for (auto& trait : PrimesFile::runlen_traits) {
				if (trait.opcode.size() > opcode.size()) { again = true; }
				if (opcode == trait.opcode) {
					vector<bool> val_bv = _bitfile.read_bits(trait.val_bits);
					uint64_t val = 0;
					for (auto bit : val_bv) { val = (val << 1) | bit; }
					if (trait.even) { val *= 2; }
					uint64_t delta = trait.delta_min + val;
					_current += delta;

					return _current;
				}
			}

			if (opcode == PRIMESI0_OP_END) {
				throw out_of_range("No more set bits");
			}
		} while (again);

		ostringstream err_stream;
		err_stream << "Unknown RunLen opcode of: ";
			for (auto bit: opcode) { err_stream << bit; }
		throw runtime_error(err_stream.str());
		break;
	}

	case PrimesFile::PRIMES32: {
		if (!_current_set) { _current = 0; _current_set = true; }

		auto orig_pos = _bitfile.tellp();

		if (orig_pos >= (20 + _num * sizeof(uint32_t))) {
				throw out_of_range("No more set bits");
		}

		uint32_t val_buf;
		_bitfile.read((char *) &val_buf, sizeof(val_buf));
		if (_bitfile.bad()) {
			ostringstream err_stream;
			err_stream << "Error while reading from file: " << _filename;
			throw runtime_error(err_stream.str());
		}
		if (_bitfile.eof()) {
			ostringstream err_stream;
			err_stream << "Unexpected EOF from file: " << _filename;
			throw runtime_error(err_stream.str());
		}

		return (uint64_t) ntohl(val_buf);
	}

	default:
		ostringstream err_stream;
		err_stream << "Unknown format of: 0x" << hex << _format << dec;
		throw runtime_error(err_stream.str());
	}

	assert(false); // Not Reached
}

void PrimesFile::dump(void)
{
	cerr << "_bitfile: " << &_bitfile
		<< " _format: " << hex << "0x" << _format << dec
		<< " _num: " << _num << endl;
	cerr << "  _start: 0x" << hex << _start << dec
		<< " _end: 0x" << hex << _end << dec
		<< " _range_set: 0x" << hex << _range_set << dec << endl;
	cerr << "  _lowest: " << _lowest << " _hightest: " << _highest
		<< " _lowest_highest_set: " << _lowest_highest_set << endl;
	cerr << "  _num: " << _num << endl;
}

int PrimesFile::readline(FILE *file, string& result, size_t max)
{
	char ch;
	size_t sz;

	result.erase();

	do {
		sz = fread(&ch, sizeof(ch), 1, file);
		if (sz != 1) {
			if (feof(file)) { return 0; } // End-of-File

			return -1;
		}

		if (ch == '\n') { break; } // End-Of-Line

		result += ch;
		if (result.length() >= max) { break; }
	} while(true);

	return 0;
}

string PrimesFile::format_str(Format format)
{
	ostringstream ss;

	switch (format) {
	case SBIT000:
		ss << "SBIT000";
		break;

	case PRIMES32:
		ss << "PRIMES32";
		break;

	case PRIMES64:
		ss << "PRIMES64";
		break;

	case SBIT:
		ss << "SBIT";
		break;

	case DELTA:
		ss << "DELTA";
		break;

	case DELTA000:
		ss << "DELTA000";
		break;

	default:
		ss << "Unknown PrimesFile::Format of: 0x"
			<< hex << setfill('0') << setw(4) << format << dec;
		throw out_of_range(ss.str());
		break;
	}

	return ss.str();
}
