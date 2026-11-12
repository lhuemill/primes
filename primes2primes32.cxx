/*
 * primes/primes2primes32.cxx
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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/stat.h>

#include "primes_util.h"
#include "sparsebit_c.h"

using namespace std;
using namespace primes_util;

#define MEMCLEAR(p, n) memset((p), 0, (n))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define CONST_STRLEN(str) (ARRAY_SIZE(str) - 1)

bool starts_with(const string& str, const string& prefix);
static int readline(FILE *file, string& result,
	size_t max = numeric_limits<size_t>::max());
static void usage(ostream& stream);
static void help(ostream& stream);

sb_idx_t param_start;
bool param_start_set = false;
sb_idx_t param_end;
bool param_end_set = false;
sb_idx_t param_num;
bool param_num_set = false;

int main(int argc, char *argv[])
{
	sb_t *sb = sb_create();
	char *filename_src, *filename_dst;
	string str;
	int rv;
	uint32_t val32;
	size_t sz;
	FILE *file;

	static const struct option options[] = {
		{"help", no_argument, NULL, 'h'},
		{} // End-Of-Array Marker
	};
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "gh?",
		options, &option_index)) != EOF) {
		switch (opt) {
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
	filename_src = argv[optind];
	filename_dst = argv[optind + 1];

	PrimesFile src(PrimesFile::READ, filename_src);

	PrimesFile dst(PrimesFile::CREATE, filename_dst, PrimesFile::PRIMES32,
		src.start(), src.end(), src.num());

	do {
		sb_idx_t idx;
		try {
			idx = src.next_set();
		}
		catch (const out_of_range& e) {
			break;
		}

		dst.set_bit(idx);
	} while (true);

return 5;

	return 0;
}

bool starts_with(const string& str, const string& prefix)
{
	return str.compare(0, prefix.length(), prefix) == 0;
}

static int readline(FILE *file, string& result, size_t max)
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

static void usage(ostream& stream)
{
	stream << "usage: primes2primes32 [options] primes_filename]" << endl;
}

static void help(ostream& stream)
{
	stream << "Primes to Primes32" << endl;
	usage(stream);
	stream << "Options:" << endl;
	stream << "  -?h,--help Help" << endl;
}
