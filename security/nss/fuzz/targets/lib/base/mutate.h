/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BASE_MUTATE_H_
#define BASE_MUTATE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

extern "C" size_t LLVMFuzzerMutate(uint8_t* data, size_t size, size_t maxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data, size_t size,
                                          size_t maxSize, unsigned int seed);

typedef std::vector<decltype(LLVMFuzzerCustomMutator)*> Mutators;

size_t CustomMutate(Mutators mutators, uint8_t* data, size_t size,
                    size_t maxSize, unsigned int seed);

#endif  // BASE_MUTATE_H_
