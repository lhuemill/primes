/*
 * primes/primes_convert.cxx
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

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <arpa/inet.h>

#include "primes_util.h"
#include "sparsebit_c.h"

using namespace std;
using namespace primes_util;

#define MEMCLEAR(p, n) memset((p), 0, (n))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define CONST_STRLEN(str) (ARRAY_SIZE(str) - 1);

string arg_src_filename, arg_dst_filename;
string arg_format_str = "delta";
PrimesFile::Format arg_format = PrimesFile::DELTA;

static void usage(ostream& stream);
static void help(ostream& stream);

int main(int argc, char *argv[])
{
	int rv;
	string str;

	static const struct option options[] = {
		{"format", required_argument, NULL, 'f'},
		{"help", no_argument, NULL, 'h'},
		{} // End-Of-Array Marker
	};
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "f:h?",
		options, &option_index)) != EOF) {
		switch (opt) {

		case 'f': {
			arg_format_str = string(optarg);
			string format_upper(arg_format_str.size(), ' ');
			transform(arg_format_str.begin(), arg_format_str.end(),
				format_upper.begin(), ::toupper);
			if (format_upper == "SBIT") {
				arg_format = PrimesFile::SBIT;
			} else if (format_upper == "DELTA") {
				arg_format = PrimesFile::DELTA;
			} else if (format_upper == "DELTA000") {
				arg_format = PrimesFile::DELTA000;
			} else {
				cerr << "Unknown result format of: " << arg_format_str << endl;
				exit(21);
			}
			break;
		}

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

	arg_src_filename = argv[optind];
	arg_dst_filename = argv[optind + 1];

	PrimesFile src(PrimesFile::READ, arg_src_filename.c_str());

	PrimesFile dst(PrimesFile::CREATE, arg_dst_filename.c_str(),
		arg_format, src.start(), src.end());

	sb_idx_t idx;
	do {
		try {
			idx = src.next_set();
		}
		catch (const out_of_range& e) {
			break;
		}
		dst.set_bit(idx);
	} while (true);

	dst.close();

	return 0;
}

static void usage(ostream& stream)
{
	stream << "usage: primes_convert [options] src_filename dst_filename" << endl;
}

static void help(ostream& stream)
{
	stream << "Primes Convert" << endl;
	usage(stream);
	stream << "Options:" << endl;
	stream << "  -f,--format format (default: delta)" << endl;
	stream << "  -?h,--help Help" << endl;
}
