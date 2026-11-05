/*
 * primes/test_powermod.cxx
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
#include <iomanip>

#include "primes_util.h"

using namespace std;
using namespace primes_util;

uint64_t trial_bases[] = {
	// First 5 usable bases greater than 0.
	2,
	3,
	4,
	5,
	6,

	// Some random, fairly small bases.
	23,
	85,
	92,
	143,
	187,
	228,
	239,
	278,
	3234,
	5721,
	8921,
	9874,

	// 2^31 and five values before and after it.
	0x7ffffffb,
	0x7ffffffc,
	0x7ffffffd,
	0x7ffffffe,
	0x7fffffff,
	0x80000000,
	0x80000001,
	0x80000002,
	0x80000003,
	0x80000004,
	0x80000005,

	// 2^32 and five values before and after it.
	0xfffffffb,
	0xfffffffc,
	0xfffffffd,
	0xfffffffe,
	0xffffffff,
	0x100000000,
	0x100000001,
	0x100000002,
	0x100000003,
	0x100000004,
	0x100000005,

	// 2^63 and five values before and after it.
	0x7ffffffffffffffb,
	0x7ffffffffffffffc,
	0x7ffffffffffffffd,
	0x7ffffffffffffffe,
	0x7fffffffffffffff,
	0x8000000000000000,
	0x8000000000000001,
	0x8000000000000002,
	0x8000000000000003,
	0x8000000000000004,
	0x8000000000000005,

  0xffffffffffffffa1,
  0xffffffffffffffad,
  0xffffffffffffffc1,
  0xffffffffffffffc2,
  0xffffffffffffffc3,
  0xffffffffffffffc4,
  0xffffffffffffffc5, // Greatest prime <= 2^ 64
};

uint64_t trial_primes[] = {
	// Primes less than 100
	2,
	3,
	5,
	7,
	11,
	13,
	17,
	19,
	23,
	29,
	31,
	37,
	41,
	43,
	47,
	53,
	59,
	61,
	67,
	71,
	73,
	79,
	83,
	89,
	97,

	// Five primes immediately before and after 2^31
	0x7fffffab,
	0x7fffffbb,
	0x7fffffc3,
	0x7fffffed,
	0x7fffffff,
	0x8000000b,
	0x8000002d,
	0x80000041,
	0x8000005f,
	0x80000081,

	// Five primes immediately before and after 2^32
	0xffffff95,
	0xffffff9d,
	0xffffffbf,
	0xffffffef,
	0xfffffffb,
	0x10000000f,
	0x10000003d,
	0x10000004b,
	0x100000051,
	0x10000005b,

	// Five primes immediately before and after 2^63
	0x7ffffffffffffe89,
	0x7ffffffffffffed3,
	0x7ffffffffffffefd,
	0x7fffffffffffff5b,
	0x7fffffffffffffe7,
	0x800000000000001d,
	0x8000000000000063,
	0x800000000000007b,
	0x8000000000000083,
	0x800000000000009b,

	// Five greatest primes less than 2^64.
	0xffffffffffffff43,
  0xffffffffffffff4d,
  0xffffffffffffffa1,
  0xffffffffffffffad,
  0xffffffffffffffc5, // Greatest prime <= 2^ 64
};

bool failed_any = false;

int main(int argc, char *argv[])
{
	for (auto prime : trial_primes) {
		for (auto base : trial_bases) {
			// Skip cases where base is equal to zero or greater than prime.
			if ((base == 0) || (base > prime)) { continue; }

			// Skip cases where base is divisable by prime. Because prime is prime,
			// this only occurs when base is equal to 1 or to the prime value.
			if ((base == 1) || (base == prime)) { continue; }

			auto rv = powerMod(base, prime - 1, prime);

			cout << hex << setfill('0');
			cout << "base: 0x" << setw(16) << base;
			cout << " prime: 0x" << setw(16) << prime;
			cout << dec << setfill(' ');
			cout << " rv: " << rv;
			if (rv != 1) {
				cout << " ---- Failed ---- ";
				failed_any = true;
			}
			cout << endl;
		}
	}

	if (failed_any) {
		cout << "---- Failed ----" << endl;
		return 1;
	}

	cout << "---- Passed ----" << endl;
	return 0;
}
