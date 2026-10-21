/*
 * primes/test_PrimesFile_runlen.cxx
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

#include <iostream>

#include "primes_util.h"

using namespace std;
using namespace primes_util;

int main(int argc, char *argv[])
{
	// Create a Primes file with delta from 1 to highest supported
	// by PRIMESI0_OP_EVEN10x code + 10 more.
	PrimesFile test_file1(PrimesFile::CREATE, "test_data000",
          PrimesFile::DELTA);
	uint64_t current = 0x10;
	for (size_t delta = 1; delta <= ((1 << 10) * 2) + 10; delta++) {
		test_file1.set_bit(current + delta);
		current += delta;
	}

	test_file1.close();

	PrimesFile test_file2(PrimesFile::READ, "test_data000");
	current = 0x10;
	for (size_t expected_delta = 1; expected_delta <= ((1 << 10) * 2) + 10;
		expected_delta++) {
		uint64_t idx = test_file2.next_set();
		if (idx != current + expected_delta) {
			cerr << "Failed: Unexpected delta, "
				<< "current: " << hex << "0x" << current << dec
				<< " delta: " << hex << "0x" << (idx - current) << dec
				<< " expected: " << hex << "0x" << expected_delta << dec << endl;
			exit(1);
		}
		current = idx;
	}


	// Validate that there are no more set bits.
	uint64_t idx;
	try {
 		idx = test_file2.next_set();
	}
	catch (const out_of_range& e) {
		cout << "---- Passed ----" << endl;
		return 0;
	}

	// Unexpected additionally set bit.
	cerr << "Failed: Unexpected additionally set bit, "
		<< "idx: 0x" << hex << idx << dec
		<< " delta: 0x" << hex << idx - current << dec << endl;

	return 2;
}
