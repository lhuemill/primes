/*
 * primes/primes_dump.cxx
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
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include <sys/stat.h>

#include "primes_util.h"
#include "sparsebit_c.h"

using namespace std;
using namespace primes_util;

static bool flag_ignore_format_diff = false;

#define MEMCLEAR(p, n) memset((p), 0, (n))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define CONST_STRLEN(str) (ARRAY_SIZE(str) - 1);

static void usage(ostream& stream);
static void help(ostream& stream);

const uint64_t first_prime = 2;
const uint64_t search_end_default = 1000000;
const long long burst_default = 10000;

int main(int argc, char *argv[])
{
	sb_t *sb = sb_create();
	FILE *file;
	string str;
	int rv;

	static const struct option options[] = {
		{"ignore_format_diff", no_argument, NULL, 'i'},
		{"help", no_argument, NULL, 'h'},
		{} // End-Of-Array Marker
	};
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "ih?",
		options, &option_index)) != EOF) {
		switch (opt) {
		case 'i':
			flag_ignore_format_diff = true;
			break;

		case 'h':
		case '?':
		default:
			bool error = ((optopt != 0) && (optopt != '?') && (optopt != 'h'));
			if (error) {
				usage(cerr);
			} else {
				help(cout);
				return 0;
			}
			exit(20);
		}
	}
	if ((argc - optind) != 2) {
		cerr << "Unexpected number of positional command-line arguments" << endl;
		exit(21);
	}

	const char *filename0 = argv[optind];
	const char *filename1 = argv[optind + 1];

	PrimesFile file0(PrimesFile::READ, filename0);
	PrimesFile file1(PrimesFile::READ, filename1);

	if (!flag_ignore_format_diff) {
		if (file0.format() != file1.format()) {
			cerr << "file format values differ," << endl
				<< "  file0 format: 0x" << hex << file0.format() << dec << endl
				<< "  file1 format: 0x" << hex << file1.format() << dec << endl;
			return 20;
		}
	}
	if (file0.start() != file1.start()) {
		cerr << "file start values differ," << endl
			<< "  file0 start: 0x" << hex << file0.start() << dec << endl
			<< "  file1 start: 0x" << hex << file1.start() << dec << endl;
		return 21;
	}
	if (file0.end() != file1.end()) {
		cerr << "file end values differ," << endl
			<< "  file0 end: 0x" << hex << file0.end() << dec << endl
			<< "  file1 end: 0x" << hex << file1.end() << dec << endl;
		return 22;
	}
	if (file0.num() != file1.num()) {
		cerr << "file num values differ," << endl
			<< "  file0 num: 0x" << hex << file0.num() << dec << endl
			<< "  file1 num: 0x" << hex << file1.num() << dec << endl;
		return 23;
	}

	bool file0_no_more_bits = false;
	bool file1_no_more_bits = false;
	do {
		sb_idx_t idx0, idx1;
		do { // Loop that catch will break out of.
			try {
				idx0 = file0.next_set();
			}
			catch (const out_of_range& e) {
				file0_no_more_bits = true;
				break;
			}
		} while (false);

		do { // Loop that catch will break out of.
			try {
				idx1 = file1.next_set();
			}
			catch (const out_of_range& e) {
				file1_no_more_bits = true;
				break;
			}
		} while (false);

		if (file0_no_more_bits != file1_no_more_bits) {
			if (file0_no_more_bits) {
				assert(!file1_no_more_bits);
				cerr << "file0 out of bits before file1 bit: 0x"
					<< hex << idx1 << dec << endl;
				exit(30);
			}
			assert(!file0_no_more_bits);
			cerr << "file1 out of bits before file0 bit: 0x"
				<< hex << idx0 << dec << endl;
			exit(31);
		}
		if (idx0 != idx1) {
			cerr << "next_set bits differ," << endl
				<< "  file0 next_set: 0x" << hex << idx0 << dec << endl
				<< "  file1 next_set: 0x" << hex << idx1 << dec << endl;
			exit(32);
		}
	} while (!(file0_no_more_bits || file1_no_more_bits));

	return 0;
}

bool starts_with(const string& str, const string& prefix)
{
	return str.compare(0, prefix.length(), prefix) == 0;
}

static void usage(ostream& stream)
{
	stream << "usage: primes_cmp [options] filename1 filename2" << endl;
}

static void help(ostream& stream)
{
	stream << "Primes Compare" << endl;
	usage(stream);
	stream << "Options:" << endl;
	stream << "  -i,--ignore_format_diff" << endl;
	stream << "  -?h,--help Help" << endl;
}
