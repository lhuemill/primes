#
# primes/Makefile
#
# Copyright 2026 Louis Huemiller Jr.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

CXX = g++
CXXFLAGS = -O3 -std=gnu++17 # gnu++17 needed for fseeko()

all: tools utilities tests

tools : \
	./target/primes \
	./target/primes_dump \
	./target/primes_cmp \
	./target/primes_validate_first_n \
	./target/primes_convert \

utilities: \
	./target/primes_rm2nd_if_equiv \

tests : \
	./target/test_PrimesFile_runlen \
	./target/test_powermod \

.PHONY: clean
clean: ## Remove build artifacts
	rm -rf ./build/ *.pyc __pycache__/ *stackdump

.PHONY: clobber
clobber: clean  ## Remove built items and build artifacts
	rm -rf ./target/

.PHONY: testit
testit: ./tests
	./target/test_PrimesFile_runlen
	./target/test_powermod

./build/:
	mkdir -p ./build/

./target/:
	mkdir -p ./target/

./build/primes_util.o: ./build/ primes_util.cxx primes_util.h ./target/
	$(CXX) $(CXXFLAGS) -c -I ../sparsebit_c/ -o ./build/primes_util.o primes_util.cxx -L ../sparsebit_c/target/lib/ -l sparsebit_c

./target/primes: ./target/ primes.cxx ./build/primes_util.o ../sparsebit_c/target/lib/libsparsebit_c.a ../sparsebit_c/sparsebit_c.h
	$(CXX) $(CXXFLAGS) -I ../sparsebit_c/ -o ./target/primes primes.cxx ./build/primes_util.o -L ../sparsebit_c/target/lib/ -l sparsebit_c

./target/primes_dump: ./target/ primes_dump.cxx ./build/primes_util.o ../sparsebit_c/target/lib/libsparsebit_c.a ../sparsebit_c/sparsebit_c.h
	$(CXX) $(CXXFLAGS) -I ../sparsebit_c/ -o ./target/primes_dump primes_dump.cxx ./build/primes_util.o -L ../sparsebit_c/target/lib/ -l sparsebit_c

./target/primes_cmp: ./target/ primes_cmp.cxx ./build/primes_util.o ../sparsebit_c/target/lib/libsparsebit_c.a ../sparsebit_c/sparsebit_c.h
	$(CXX) $(CXXFLAGS) -I ../sparsebit_c/ -o ./target/primes_cmp primes_cmp.cxx ./build/primes_util.o -L ../sparsebit_c/target/lib/ -l sparsebit_c

./target/primes_validate_first_n: ./target/ primes_validate_first_n.cxx ./build/primes_util.o ../sparsebit_c/target/lib/libsparsebit_c.a ../sparsebit_c/sparsebit_c.h
	$(CXX) $(CXXFLAGS) -I ../sparsebit_c/ -o ./target/primes_validate_first_n primes_validate_first_n.cxx ./build/primes_util.o -L ../sparsebit_c/target/lib/ -l sparsebit_c

./target/primes_convert: ./target/ primes_convert.cxx ./build/primes_util.o ../sparsebit_c/target/lib/libsparsebit_c.a ../sparsebit_c/sparsebit_c.h
	$(CXX) $(CXXFLAGS) -I ../sparsebit_c/ -o ./target/primes_convert primes_convert.cxx ./build/primes_util.o -L ../sparsebit_c/target/lib/ -l sparsebit_c

./target/primes_rm2nd_if_equiv: ./target/ ./primes_rm2nd_if_equiv ./target/
	cp ./primes_rm2nd_if_equiv ./target/primes_rm2nd_if_equiv
	chmod +x ./target/primes_rm2nd_if_equiv

./target/test_PrimesFile_runlen : ./target/ ./test_PrimesFile_runlen.cxx ./build/primes_util.o
	$(CXX) $(CXXFLAGS) -I ../sparsebit_c/ -o ./target/test_PrimesFile_runlen test_PrimesFile_runlen.cxx ./build/primes_util.o -L ../sparsebit_c/target/lib/ -l sparsebit_c

./target/test_powermod : ./target/ ./test_powermod.cxx ./build/primes_util.o
	$(CXX) $(CXXFLAGS) -I ../sparsebit_c/ -o ./target/test_powermod \
		test_powermod.cxx ./build/primes_util.o \
		-L ../sparsebit_c/target/lib/ -l sparsebit_c
