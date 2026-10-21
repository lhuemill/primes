/*
 * primes/primes_validate_first_n.cxx
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
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "primes_util.h"
#include "sparsebit_c.h"

using namespace std;
using namespace primes_util;

#define MEMCLEAR(p, n) memset((p), 0, (n))
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define CONST_STRLEN(str) (ARRAY_SIZE(str) - 1);

const size_t fileid_maxlen = 8;
struct primes32 {
	char id[fileid_maxlen];
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

static bool is_prime(uint64_t idx);
static void usage(ostream& stream);
static void help(ostream& stream);

const unsigned long long num_default = 3;
unsigned long long arg_num = num_default;

const uint64_t first_prime = 2;
struct primes32_param mmaped_primes;
static vector<uint64_t> primes_cache = {2, 3, 5, 7, 11, 13};
static uint64_t highest_cached = 13;

int main(int argc, char *argv[])
{
	FILE *file;
	string str;
	int rv;
	stringstream err_stream;

	static const struct option options[] = {
		{"num", required_argument, NULL, 'n'},
		{"primes", required_argument, NULL, 'p'},
		{"help", no_argument, NULL, 'h'},
		{} // End-Of-Array Marker
	};
	int opt;
	int option_index;
	while ((opt = getopt_long(argc, argv, "n:p:h?",
		options, &option_index)) != EOF) {
		switch (opt) {
		case 'n': {
			unsigned long long val;
			char *endptr;
			val = strtoull(optarg, &endptr, 0);
			if ((endptr == optarg) || (*endptr != '\0') || (val <= 0)) {
				err_stream << "Invalid num of: " << optarg;
				throw invalid_argument(err_stream.str());
			}
      arg_num = val;

			break;
		}

		case 'p': {
			char *filename = optarg;
			int mmap_fd;
			FILE *mmap_file;
			string str;
			string parm_first_str;
			string parm_end_str;

			mmap_file = fopen(filename, "rb");
			if (mmap_file == NULL) {
				cerr << "Error opening: " << filename << endl;
				cerr << " errno: " << errno << endl;
				exit(21);
			}
			rv = PrimesFile::readline(mmap_file, str, fileid_maxlen);
			if (rv != 0) {
				cerr << "readline failed, rv: " << rv << " errno: " << errno << endl;
				exit(22);
			}

			if (str == "primes32") {
				uint32_t val32;
				struct primes32_param param;
				size_t sz;

				sz = fread(&val32, 1, sizeof(val32), mmap_file);
				if (sz != sizeof(val32)) {
					if ((sz > 0) || ((sz == 0) && feof(mmap_file))) {
						cerr << "Unexpected amount while reading primes32 start "
							<< "value from: " << filename << ", sz: " << sz << endl;
						exit(26);
					} else {
						cerr << "Error while reading primes32 start value from: "
							<< filename << ", sz: " << sz << " errno: " << errno << endl;
						exit(27);
					}
				}
				mmaped_primes.start = ntohl(val32);

				sz = fread(&val32, 1, sizeof(val32), mmap_file);
				if (sz != sizeof(val32)) {
					if ((sz > 0) || ((sz == 0) && feof(mmap_file))) {
						cerr << "Unexpected amount while reading primes32 end "
						<< "value from: " << filename << ", sz: " << sz << endl;
						exit(28);
					} else {
						cerr << "Error while reading primes32 end value from: "
							<< filename << ", sz: " << sz << " errno: " << errno << endl;
						exit(29);
					}
				}
				mmaped_primes.end = ntohl(val32);

				sz = fread(&val32, 1, sizeof(val32), mmap_file);
				if (sz != sizeof(val32)) {
					if ((sz > 0) || ((sz == 0) && feof(mmap_file))) {
						cerr << "Unexpected amount while reading primes32 num "
							<< "value from: " << filename << ", sz: " << sz << endl;
						exit(30);
					} else {
						cerr << "Error while reading primes32 num value from: "
							<< filename << ", sz: " << sz << " errno: " << errno << endl;
						exit(31);
					}
				}
				mmaped_primes.num = ntohl(val32);

				size_t file_size = offsetof(struct primes32, val)
					+ mmaped_primes.num * sizeof(uint32_t);

				mmap_fd = open(filename, O_RDONLY);
				if (mmap_fd < 0) {
					cerr << "Error opening: " << filename
						<< ",  errno: " << errno << endl;
					exit(32);
				}
				mmaped_primes.primes32 = (struct primes32 *) mmap(0, file_size,
					PROT_READ, MAP_PRIVATE, mmap_fd, 0);
				if (mmaped_primes.primes32 == MAP_FAILED) {
					cerr << "Failed to map primes32 file of: " << filename
						<< ", errno: " << errno << endl;
					exit(33);
				}
				assert(mmaped_primes.start == ntohl(mmaped_primes.primes32->start));
				assert(mmaped_primes.end == ntohl(mmaped_primes.primes32->end));
				assert(mmaped_primes.num == ntohl(mmaped_primes.primes32->num));

				// Intentionally don't close mmap_fd, so map to file is still open.
				// Possibly mmap placed a reference count on the open fd, so closing
				// it wouldn't be an issue, but for safety will just keep it open.
				fclose(mmap_file);
			} else {
				cerr << "Only primes32 format supported, file: " << filename << endl;
				exit(34);
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
	if ((argc - optind) < 1) {
		err_stream << "Unexpected number of positional command-line arguments";
		throw invalid_argument(err_stream.str());
		exit(21);
	}

	for (; (argc - optind) >= 1; optind++) {
		const char *filename = argv[optind];
		
		file = fopen(filename, "rb");
		if (file == NULL) {
			err_stream << "Error opening: " << filename;
			err_stream << ",  errno: " << errno << endl;
			throw runtime_error(err_stream.str());
			exit(22);
		}

		// Determine primes file format.
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

		vector<uint64_t> actual;
		uint64_t start;
		uint64_t end;
		uint64_t num;

		// Populate actual with first num primes.
		switch (format) {
		// case PrimesFile::SBIT000:
		// 	break;

		// case PrimesFile::SBIT:
		// 	break;

		case PrimesFile::PRIMES32:
		case PrimesFile::DELTA000:
		case PrimesFile::DELTA: {
			PrimesFile pfile(PrimesFile::READ, filename);

			start = pfile.start();
			end = pfile.end();
			num = pfile.num();

			// Populate actual with first num primes.
			for (size_t n = 0; n < arg_num; n++) {
				uint64_t idx;
				try {
					idx = pfile.next_set();
				}
				catch (const out_of_range& e) {
					break;
				}
				if (idx < pfile.start()) {
					cout << "---- FAILED ----: " << filename << endl;
					exit(30);
				}
				uint64_t offset = idx - pfile.start();
				actual.push_back(idx);
			}

			break;
		}

		default:
			err_stream << "Unknown format of: 0x" << hex << format << dec;
			throw runtime_error(err_stream.str());
		}

		// Determine idx of nth prime. If there wasn't any primes, use start
		// plus 100, to give a bit of a range to check for only composites.
		// If less than num primes, use index of last prime plus 100, again
		// to give a little range to check for only composities.
		uint64_t actual_nth_prime = (start + 100 > start)
			? start + 100 : UINT64_MAX;
		if (actual.size() >= 1) {
			actual_nth_prime = actual[actual.size() - 1];
		}
		if (actual.size() < arg_num) {
			actual_nth_prime = (actual_nth_prime + 100 < UINT64_MAX)
				? actual_nth_prime + 100 : UINT64_MAX;
		}
		if (actual_nth_prime > end) { actual_nth_prime = end; }

		fclose(file);

		// Independently determine first num primes starting from
		// primes file start value. Start with a bool vector, with
		// entries corresponding to index of start through actual_nth_prime.
		// Start with all entries equal to true and then clear each entry
		// as it is found to be composite.
		vector<bool> vb_expected(actual_nth_prime - start + 1, true);
		if (start < first_prime) {
			for (auto idx = start; idx < first_prime; idx++) {
				if ((idx - start) < vb_expected.size()) {
					vb_expected[idx - start] = false;
				}
			}
		}

		if (mmaped_primes.primes32 && mmaped_primes.start <= first_prime
			&& ((mmaped_primes.end == UINT64_MAX)
				|| ((mmaped_primes.end + 1) * (mmaped_primes.end + 1)
					> actual_nth_prime)
				|| ((mmaped_primes.end * mmaped_primes.end) <= actual_nth_prime)
			) ) {
			if (mmaped_primes.num >= 1) {
				for (size_t n1 = 0; n1 < mmaped_primes.num; n1++) {
					uint64_t prime = ntohl(mmaped_primes.primes32->val[n1]);
					if (((prime * prime) < prime)
						|| ((prime * prime) > actual_nth_prime)) { break; }

					for (uint64_t n2 = ((start % prime) == 0)
						? 0 : prime - (start % prime); n2 < vb_expected.size();
						n2 += prime) {
						if ((start + n2) != prime) vb_expected[n2] = false;
					}
				}
			}
		} else {
			for (uint64_t p = first_prime;
				(p * p <= actual_nth_prime) && (p * p > p); p++) {
				if ((p * p > actual_nth_prime) || (p * p < p)) { break; }
				for (size_t  n = (start % p == 0) ? 0 : p - (start % p);
					n < vb_expected.size(); n += p) {
					if (start + n != p) { vb_expected[n] = false; }
				}
			}
		}

		// Create a vector with just the indexes of the expected primes.
		vector<uint64_t> expected;
		for (auto it = vb_expected.cbegin(); it != vb_expected.cend(); ++it) {
			if (*it) {
				expected.push_back(start + it - vb_expected.cbegin());
			}
		}

		// Validate actual == expected.
		if (actual != expected) {
			cout << "BAD ----: " << filename << endl;

			cout << "  actual:";
			for (auto val: actual) {
				cout << ' ' << val;
			}
			cout << endl;

			cout << "expected:";
			for (auto val: expected) {
				cout << ' ' << val;
			}
			cout << endl;

			continue;
		}

		// Validate primes file specified num primes of at least the
		// number populated into the actual array. This catches cases
		// where the process generating a prime file was killed before
		// it finished.
		if (num < actual.size()) {
			cout << "BAD ---- num too small: " << filename << endl;
			continue;
		}

		cout << "Good: " << filename << endl;
	}

	return 0;
}

static bool is_prime(uint64_t idx)
{
//cerr << "is_prime(" << hex << idx << dec << ")" << endl;
	if (idx < first_prime) return false;

	// If needed grow vector of known_primes.
	while ((highest_cached * highest_cached < idx) 
		&& (highest_cached * highest_cached > highest_cached)) {
		(void) is_prime(highest_cached + 1);
	}

	for (auto prime : primes_cache) {
		if ((prime * prime > idx) || (prime * prime < prime)) { break; }
		if (idx % prime == 0) {
			if (idx == highest_cached + 1) {
				highest_cached = idx;
			}
			return false;
		}
	}

	if (idx == highest_cached + 1) {
		primes_cache.push_back(idx);
		highest_cached = idx;
	}
	return true;
}

static void usage(ostream& stream)
{
	stream << "usage: primes_validate_first_n [options] filename [filename ...]"
		<< endl;
}

static void help(ostream& stream)
{
	stream << "Primes Validate First N" << endl;
	usage(stream);
	stream << "Options:" << endl;
	stream << "-n,--num num # First num are checked (default: "
		<< num_default << ")" << endl;
	stream << "  -?h,--help Help" << endl;
}
