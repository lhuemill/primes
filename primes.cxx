/*
 * primes/primes.cxx
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
#include <bitset>
#include <climits>
#include <fstream>
#include <string>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <cstdint>
#include <iostream>
#include <unistd.h>
#include <vector>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>

#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "primes_util.h"
#include "sparsebit_c.h"

using namespace std;
using namespace primes_util;

static bool is_prime(uint64_t idx);
static void primes(uint64_t start, vector<bool>& result);
static uint32_t sqrt(uint64_t val);
static void usage(ostream& stream);
static void help(ostream& stream);

const size_t fileid_maxlen = 8;
const uint64_t first_prime = 2;
const uint64_t search_end_default = 1000000;
const unsigned long long burst_default = 0x1000000;

unsigned long long arg_burst = burst_default;
char *arg_result_filename = NULL;
string arg_format_str = "delta";
PrimesFile::Format arg_format = PrimesFile::DELTA;
bool flag_hex = false;

static sb_t *primes_known = sb_create();
static vector<struct primes32_param> primes32_known;
static sb_t *primes_known_range = sb_create();
static sb_idx_t primes_cache_limit = SB_IDX_MAX;

struct primes32 {
	char id[8];
	uint32_t start;
	uint32_t end;
	uint32_t num;
	uint32_t val[];
};

struct primes32_param {
	uint32_t start;
	uint32_t end;
	uint32_t num;
	struct primes32 *primes32;
};

int main(int argc, char *argv[])
{
	unsigned long long search_start = first_prime;
	unsigned long long search_end = search_end_default;
	FILE *result_file = NULL;
	PrimesFile *primes_file_p = NULL;
	int rv;
	size_t sz;

	sb_t *result = NULL;

	static const struct option options[] = {
		{"burst", required_argument, NULL, 'b'},
		{"format", required_argument, NULL, 'f'},
		{"primes", required_argument, NULL, 'p'},
		{"result", required_argument, NULL, 'r'},
		{"hex", no_argument, NULL, 'x'},
		{"help", 0, NULL, 'h'},
		{} // End-Of-Array Marker
	};
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "b:f:p:r:xh?",
		options, &option_index)) != EOF) {
		switch (opt) {

		case 'b': {
			unsigned long long val;
			char *endptr;
			val = strtoull(optarg, &endptr, 0);
			if ((endptr == optarg) || (*endptr != '\0') || (val <= 0)) {
				cerr << "Invalid -s/--burst value of: " << optarg << endl;
				exit(20);
			}
			arg_burst = val;
			break;
		}

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

		case 'p': {
 			char *filename = optarg;
			int fd;
			FILE *file;
			string str;
			string parm_first_str;
			string parm_end_str;

			file = fopen(filename, "rb");
			if (file == NULL) {
				cerr << "Error opening: " << filename << endl;
				cerr << " errno: " << errno << endl;
				exit(21);
			}
			rv = PrimesFile::readline(file, str, fileid_maxlen);
			if (rv != 0) {
				cerr << "readline failed, rv: " << rv << " errno: " << errno << endl;
				exit(22);
			}

			if (str == "primes") {
				// Read in primes parameters
				do {
					rv = PrimesFile::readline(file, str);
					if (rv != 0) {
						cerr << "readline failed, rv: " << rv << " errno: "
							<< errno << endl;
						exit(23);
					}
					if (str.size() == 0) { break; } // Blank line marks end of parameters
					auto pos = str.find(":");
					if (pos != string::npos) {
						string name = str.substr(0, pos);
						string val = str.substr(pos + 2);
						if (name == "start") {
							parm_first_str = val;
						}
						if (name == "end") {
							parm_end_str = val;
						}
					}
				} while (true);
				rv = sb_load(file, primes_known);
				if (rv != 0) {
					cerr << "sb_load failed, rv: " << rv << " errno: " << errno << endl;
					exit(24);
				}
				rv = fclose(file);
				if (rv != 0) {
					cerr << "Error closing: " << filename << ", rv: " << rv
						<< " errno: " << errno << endl;
					exit(25);
				}

				sb_idx_t first = stoull(parm_first_str);
				sb_idx_t last = stoull(parm_end_str);
				sb_setnum(primes_known_range, first, last - first + 1);
			} else if (str == "primes32") {
				uint32_t val32;
				struct primes32_param param;

				sz = fread(&val32, 1, sizeof(val32), file);
				if (sz != sizeof(val32)) {
					if ((sz > 0) || ((sz == 0) && feof(file))) {
						cerr << "Unexpected amount while reading primes32 start "
							<< "value from: " << filename << ", sz: " << sz << endl;
						exit(26);
					} else {
						cerr << "Error while reading primes32 start value from: "
							<< filename << ", sz: " << sz << " errno: " << errno << endl;
						exit(27);
					}
				}
				param.start = ntohl(val32);

				sz = fread(&val32, 1, sizeof(val32), file);
				if (sz != sizeof(val32)) {
					if ((sz > 0) || ((sz == 0) && feof(file))) {
						cerr << "Unexpected amount while reading primes32 end "
							<< "value from: " << filename << ", sz: " << sz << endl;
						exit(28);
					} else {
						cerr << "Error while reading primes32 end value from: "
							<< filename << ", sz: " << sz << " errno: " << errno << endl;
						exit(29);
					}
				}
				param.end = ntohl(val32);

				sz = fread(&val32, 1, sizeof(val32), file);
				if (sz != sizeof(val32)) {
					if ((sz > 0) || ((sz == 0) && feof(file))) {
						cerr << "Unexpected amount while reading primes32 num "
							<< "value from: " << filename << ", sz: " << sz << endl;
						exit(30);
					} else {
						cerr << "Error while reading primes32 num value from: "
							<< filename << ", sz: " << sz << " errno: " << errno << endl;
						exit(31);
					}
				}
				param.num = ntohl(val32);
				
				size_t file_size = offsetof(struct primes32, val)
					+ param.num * sizeof(uint32_t);

				fd = open(filename, O_RDONLY);
				if (fd < 0) {
					cerr << "Error opening: " << filename
						<< ",  errno: " << errno << endl;
					exit(32);
				}
				param.primes32 = (struct primes32 *) mmap(0, file_size,
					PROT_READ, MAP_PRIVATE, fd, 0);
				if (param.primes32 == MAP_FAILED) {
					cerr << "Failed to map primes32 file of: " << filename
						<< ", errno: " << errno << endl;
					exit(33);
				}
				assert(param.start == ntohl(param.primes32->start));
				assert(param.end == ntohl(param.primes32->end));
				assert(param.num == ntohl(param.primes32->num));

				primes32_known.push_back(param);
				sb_setnum(primes_known_range, param.start,
					(sb_idx_t) param.end - (sb_idx_t) param.start + 1);
				// Intentionally don't close fd, so map to file is still open.
				// Possibly mmap placed a reference count on the open fd, so closing
				// it wouldn't be an issue, but for safety will just keep it open.
			} else {
				cerr << "Unknown file ID of " << str
					<< " at start of: " << filename << endl;
				exit(34);
			}

			break;
		}

		case 'r': {
			if (arg_result_filename) {
				cerr << "-r/--result specified multiple times" << endl;
				exit(35);
			}
			arg_result_filename = optarg;
			break;
		}

		case 'x':
		{
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
			exit(38);
		}
	}
	if ((argc - optind) >= 3) {
		cerr << "Unexpected positional command-line arguments" << endl;
		exit(50);
	}
	if ((argc - optind) >= 1) {
		char *endptr;
		if ((argc - optind) == 2) {
			search_start = strtoull(argv[optind], &endptr, 0);
			optind++;
		}
		search_end = strtoull(argv[optind], &endptr, 0);
		optind++;
	}
	if (search_start > search_end) {
		cerr << "Search start of " << search_start
			<< " greater than search end of " << search_end << endl;
		exit(51);
	}

	if (arg_result_filename) {
		switch (arg_format) {
			case PrimesFile::SBIT:
				result_file = fopen(arg_result_filename, "wb");
				if (result_file == NULL) {
					cerr << "Failed to open/create result file of: "
						<< arg_result_filename << endl;
					exit(36);
				}

				result = sb_create();
				if (result == NULL) {
					cerr << "Failed to create result sparsebit" << endl;
					exit(37);
				}
				break;

// TODO: Can case DELTA and case DELTA000 be combined by using
//       args_format variable, instead PrimesFile::DELTA
//       and PrimesFile::DELTA000
			case PrimesFile::DELTA:
				primes_file_p = new PrimesFile(PrimesFile::CREATE, arg_result_filename,
					PrimesFile::DELTA, search_start, search_end);
				break;

			case PrimesFile::DELTA000:
				primes_file_p = new PrimesFile(PrimesFile::CREATE, arg_result_filename,
					PrimesFile::DELTA000, search_start, search_end);
				break;

			default:
				assert(false);
		}
	}

	rv = sb_set(primes_known, first_prime);
	assert(rv == 0);
	rv = sb_setnum(primes_known_range, 0, first_prime + 1);
	assert(rv == 0);

	primes_cache_limit = sqrt(search_end);
	if (primes_cache_limit < SB_IDX_MAX) {
		rv = sb_clearnum(primes_known, primes_cache_limit + 1,
			SB_IDX_MAX - primes_cache_limit);
		if (rv != 0) {
			cerr << "sb_clearnum failed, rv: " << rv << " errno: " << errno << endl;
			exit(52);
		}
		rv = sb_clearnum(primes_known_range, primes_cache_limit + 1,
			SB_IDX_MAX - primes_cache_limit);
		if (rv != 0) {
			cerr << "sb_clearnum failed, rv: " << rv << " errno: " << errno << endl;
			exit(53);
		}
	}

	// Erase from primes known spasebit the ranges described by each
	// of the primes32 ranges. That way the more efficient primes32
	// values will be used before the primes specified in the sparsebit
	// array.
	for (auto val : primes32_known) {
		rv = sb_clearnum(primes_known, (sb_idx_t) val.start,
			(sb_idx_t) val.end - (sb_idx_t) val.start + 1);
		if (rv != 0) {
			cerr << "sb_clearnum failed, rv: " << rv << " errno: " << errno << endl;
			exit(60);
		}
	}

	uint64_t num;
	vector<bool> rv_primes;
	for (unsigned long long n1 = search_start; n1 <= search_end;
		n1 += num) {
		// vector<bool> rv_primes;

		// If primes are being written to result file instead of stdout,
		// show progress by printing the starting index of this burst.
		if ((result_file != NULL) || (primes_file_p != NULL)) {
			if (!flag_hex) {
				cout << n1 << endl;
			} else {
				cout << hex << "0x" << n1 << dec << endl;
			}
		}

		assert(n1 >= search_start);
		num = (((search_end - n1) + 1) < arg_burst)
			? (search_end - n1) + 1 : arg_burst;
		if (num != rv_primes.size()) {
			rv_primes.resize(num);
		}
		primes(n1, rv_primes);
		for (auto i1 = rv_primes.begin(); i1 != rv_primes.end(); i1++) {
			if (*i1) {
				sb_idx_t idx = n1 + (i1 - rv_primes.begin());
				if (result) {
					sb_set(result, idx);
				}
				if (primes_file_p) {
					primes_file_p->set_bit(idx);
				}

				// If no result file, then display prime to stdout.
				if ((result_file == NULL) && (primes_file_p == NULL)) {
					if (!flag_hex) {
						cout << idx << endl;
					} else {
						cout << hex << "0x" << idx << dec << endl;
					}
				}
			}
		}
		if ((n1 + num) == 0) { break; }
	}

	if (result_file != NULL) {
		stringstream ss;
		ss << "primes" << endl;
		ss << "start: " << search_start << endl;
		ss << "end: " << search_end << endl;
		ss << "num: " << sb_numset(result) << endl;
		ss << endl; // Blank line used to identify end of primes parameters.
	            	// Next byte will be start of sparsebit.

		sz = fwrite(ss.str().c_str(), 1, ss.str().size(), result_file);
		if (sz != ss.str().size()) {
			cerr << "Error writing header to primes sbit file" << endl;
			exit(70);
		}
		rv = sb_save(result_file, result);
	}

	if (primes_file_p) { primes_file_p->close(); }

	return 0;
}

static bool is_prime(uint64_t idx)
{
	if (idx < first_prime) { return false; }

	// As needed expand range of known primes up to square
	// root of idx.
	sb_idx_t first_unknown = sb_firstcleared(primes_known_range);
	while (((first_unknown * first_unknown)) <= idx) {
		(void) is_prime(first_unknown);
		first_unknown++;
	}

	for (sb_idx_t prime = sb_firstset(primes_known); prime != 0;
		prime = sb_nextset(primes_known, prime)) {
		if ((prime == 0) || ((prime * prime) > idx)) { break; }
		if ((idx % prime) == 0) {
			sb_set(primes_known_range, idx);
			return false;
		}
	}

	sb_set(primes_known, idx);
	sb_set(primes_known_range, idx);
	return true;
}

static void primes(uint64_t start, vector<bool>& result)
{
	uint64_t num = result.size();

	// Start out by setting all the bits. A set bit indicates it is prime.
	// Will clear each bit as it is found to be composite.
	// Setting all the bits via:
	//
	//   result.assign(num, true);
	//
	// or:
	//
	//   fill(result.begin(), result.end(), true);
	//
	// also works, but in both cases is slightly slower than
	// resizing to 0 and then resizeing back to the original
	// size with an initial value of true.
	result.resize(0);
	result.resize(num, true);

	if (start < first_prime) {
		for (uint64_t idx = 0; idx < num; idx++) {
			if ((start + idx) < first_prime) {
				result[idx] = false;
			} else {
				break;
			}
		}
	}

	if (sb_iscleared(primes_known, first_prime)) {
		sb_set(primes_known, first_prime);
		sb_setnum(primes_known_range, 0, first_prime + 1); 
	}

	// As needed expand range of known primes up to square root of
	// (start + num - 1).
	sb_idx_t first_unknown = sb_firstcleared(primes_known_range);
	while (((first_unknown * first_unknown) > first_unknown)
		&& (((first_unknown * first_unknown)) <= (start + num - 1))) {
		(void) is_prime(first_unknown);
		first_unknown++;
	}

	for (sb_idx_t prime = sb_firstset(primes_known); prime != 0;
		prime = sb_nextset(primes_known, prime)) {
		if ((start + num > start)
			&& ((prime * prime) >= (start + num))) { break; }

		for (uint64_t n1 = ((start % prime) == 0) ? 0 : prime - (start % prime);
			n1 < num; n1 += prime) {
			if ((start + n1) != prime) result[n1] = false;
		}
	}

	for (auto val : primes32_known) {
		if (val.num >= 1) {
			for (size_t n1 = 0; n1 < val.num; n1++) {
				sb_idx_t prime = ntohl(val.primes32->val[n1]); 
				if ((start + num > start) // TODO: Why isn't this start + prime.
					&& (prime * prime) >= (start + num)) { break; }

				for (uint64_t n2 = (( start % prime) == 0)
					? 0 : prime - (start % prime); n2 < num; n2 += prime) {
      		if ((start + n2) != prime) result[n2] = false;
				}
			}
    }
	}

	if (start <= primes_cache_limit) {
		first_unknown = sb_firstcleared(primes_known_range);
		if (first_unknown <= (start + num - 1)) {
			for (uint64_t n1 = 0; n1 < num; n1++) {
				sb_idx_t idx = start + n1;
				if (idx > primes_cache_limit) { break; }
				first_unknown = sb_firstcleared(primes_known_range);

				if (sb_iscleared(primes_known_range, idx)) {
					if (result[n1]) {
						sb_set(primes_known, idx);
					}
					sb_set(primes_known_range, idx);
					first_unknown = sb_firstcleared(primes_known_range);
				}
			}
		}
	}

	// return rv;
	return;
}

static uint32_t sqrt(uint64_t val)
{
	if (val == 0 || val == 1) { return val; }

	uint32_t low = 1;
	uint32_t high = (val < 1ull << 32) ? val / 2 : ((uint64_t) 1 << 32) - 1;
	while (low <= high) {
		uint32_t mid = low + (high - low) / 2;
		uint64_t product = (uint64_t) mid * (uint64_t) mid;

		if (product == val)  { return mid; }
		if (product < val) {
			low = mid + 1;
			// ans = mid;
		} else {
			high = mid - 1;
		}
		if (low >= high) { return high; }
	}

	assert(false); // Not Reached
}

static void usage(ostream& stream)
{
	stream << "usage: primes [options] [[start] end]" << endl;
}

static void help(ostream& stream)
{
	stream << "Primes Find" << endl;
	usage(stream);
	stream << "Options:" << endl;
	stream << "  -b,--burst number (default: " << burst_default << ")" << endl;
	stream << "  -f,--format format (default: delta)" << endl;
	stream << "  -p,--primes filename" << endl;
	stream << "  -r,--result filename" << endl;
	stream << "  -x,--hex" << endl;

	stream << "  -?h,--help Help" << endl;
  stream << "start - Start of search (default: " << first_prime << ")" << endl;
	stream << "end - End of search (default: " << search_end_default << endl;
}
