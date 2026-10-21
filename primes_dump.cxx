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
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/stat.h>

#include "primes_util.h"
#include "sparsebit_c.h"

using namespace std;
using namespace primes_util;

static void usage(ostream& stream);
static void help(ostream& stream);

char *arg_result_filename = NULL;
bool display_gap = false;
bool suppress_header = false;
bool header_only = false;
bool flag_hex = false;

int main(int argc, char *argv[])
{
	FILE *file;
	const char *filename;
	string str;
	int rv;

	static const struct option options[] = {
		{"gap", no_argument, NULL, 'g'},
		{"suppress_header", no_argument, NULL, 's'},
		{"header_only", no_argument, NULL, 'H'},
		{"hex", no_argument, NULL, 'x'},
		{"help", no_argument, NULL, 'h'},
		{} // End-Of-Array Marker
	};
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "gsHxh?",
		options, &option_index)) != EOF) {
		switch (opt) {
		case 'g':
			display_gap = true;
			break;

		case 's':
			suppress_header = true;
			break;

		case 'H':
			header_only = true;
			break;

		case 'x': {
			flag_hex = true;
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
	// if ((argc - optind) != 1) {
	if ((argc - optind) < 1) {
		cerr << "Unexpected number of positional command-line arguments" << endl;
		exit(21);
	}

	for (; (argc - optind) >= 1; optind++) {
		filename = argv[optind];
		cout << "---- " << filename << endl;
		file = fopen(filename, "rb");
		if (file == NULL) {
			cerr << "Error opening " << filename << ", errno: " << errno << endl;
			exit(22);
		}

		size_t sz;
		char buf[8];
		sz = fread(buf, 1, sizeof(buf), file);
		if (sz != sizeof(buf)) {
			stringstream err_stream;

			if (sz == -1) {
				err_stream << "Error while attempting to read PrimesFile header, "
					"errno: " << errno;
				throw runtime_error(err_stream.str());
			}
			err_stream << "Partial read of PrimesFile header, sz: " << sz;
			throw runtime_error(err_stream.str());
		}
		if (strncmp(buf, "primes", 6) != 0) {
			cerr << "Unexpected header" << endl;
			exit(23);
		}
		PrimesFile::Format format = (PrimesFile::Format)
			ntohs(*((uint16_t *) &buf[6]));

		if (!suppress_header) {
			cout << "Format: " << PrimesFile::format_str(format) << endl;
		}

		switch (format) {
		case PrimesFile::SBIT000: {
			sb_t *sb = sb_create();

			// Seek back one byte. The first byte of the two byte format is a newline,
			// while the second byte is the start of the parameters.
			rv = fseek(file, -1, SEEK_CUR);
			if (rv != 0) {
				stringstream err_stream;
				err_stream << "Error seeking back to start of SBIT000 parameters, "
					"rv: " << rv << " errno: " << errno;
				throw runtime_error(err_stream.str());
			}
			do {
				rv = PrimesFile::readline(file, str);
				if (rv != 0) {
					stringstream err_stream;
					err_stream << "Error reading SBIT0000 parameter line, "
						"rv: " << rv << " errno: " << errno;
					throw runtime_error(err_stream.str());
				}
				if (str.size() == 0) { break; } // Blank line marks end of parameters
				auto pos = str.rfind(":");
				if (pos == string::npos) {
					stringstream err_stream;
					err_stream << "colon not found in parameter string of: "
						<< str;
					throw runtime_error(err_stream.str());
				}
				if (!suppress_header) {
					if (!flag_hex) {
						cout << str << endl;
					} else {
						cout << string(str, 0, pos) << ": ";
						cout << hex << "0x" << std::stoull(string(str, pos + 2))
							<< dec << endl;
					}
				}
			} while (true);
			if (header_only) { break; }

			rv = sb_load(file, sb);
			if (rv != 0) {
				stringstream err_stream;
				err_stream << "Error while loading sparsebit data, rv: "
					<< rv << " errno: " << errno;
				throw runtime_error(err_stream.str());
			}

			if (sb_anyset(sb)) {
				sb_idx_t prev_idx;
				bool prev_idx_set = false;
				for (sb_idx_t idx = sb_firstset(sb); idx != 0;
					idx = sb_nextset(sb, idx)) {
					if (display_gap) {
						if (prev_idx_set) {
							cout << setw(4) << idx - prev_idx - 1 << " " << idx << endl;
						} else {
							cout << "     " << idx << endl;
						}
					} else {
						if (!flag_hex) {
							cout << idx << endl;
						} else {
							cout << hex << "0x" << idx << dec << endl;
						}
					}
					prev_idx = idx;
					prev_idx_set = true;
				}
			}
			break;
		}

		// case PrimesFile::SBIT:
		// 	break;

		case PrimesFile::PRIMES32:
		case PrimesFile::DELTA000:
		case PrimesFile::DELTA: {
			PrimesFile pfile(PrimesFile::READ, filename);
			cout << "Format: 0x" << hex << format << dec << endl;
			if (!flag_hex) {
				cout << "Start: " << pfile.start() << endl;
				cout << "End: " << pfile.end() << endl;
				cout << "Num: " << pfile.num() << endl;
			} else {
				cout << "Start: 0x" << hex << pfile.start() << dec << endl;
				cout << "End: 0x" << hex << pfile.end() << dec << endl;
				cout << "Num: 0x" << hex << pfile.num() << dec << endl;
			}
			if (header_only) { break; }

			do {
				uint64_t idx;
				try {
					idx = pfile.next_set();
				}
				catch (const out_of_range& e) {
					break;
				}
		
				if (!flag_hex) {
					cout << idx << endl;
				} else {
					cout << hex << "0x" << idx << dec << endl;
				}
			} while (true);
			break;
		}

		default:
			cerr << "Unknown format of: 0x" << hex << format << dec << endl;
			exit(29);
		}

		fclose(file);
	}

	return 0;
}

static void usage(ostream& stream)
{
	stream << "usage: primes_dump [options] filename [filename ...]" << endl;
}

static void help(ostream& stream)
{
	stream << "Primes Dump" << endl;
	usage(stream);
	stream << "Options:" << endl;
	stream << "  -g, --gap: Display gap" << endl;
	stream << "  -s, --suppress_header: Suppress Headers" << endl;
	stream << "  -H, --header_only: Header Only" << endl;
	stream << "  -x, --hex: Hexidecimal" << endl;
	stream << "  -?h, --help: Help" << endl;
}
