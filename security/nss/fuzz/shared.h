/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_H_
#define SHARED_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "nss.h"

extern "C" size_t LLVMFuzzerMutate(uint8_t* data, size_t size, size_t maxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data, size_t size,
                                          size_t maxSize, unsigned int seed);

class NSSDatabase {
 public:
  NSSDatabase() { assert(NSS_NoDB_Init(nullptr) == SECSuccess); }
  ~NSSDatabase() { assert(NSS_Shutdown() == SECSuccess); }
};

typedef std::vector<decltype(LLVMFuzzerCustomMutator)*> Mutators;

size_t CustomMutate(Mutators mutators, uint8_t* data, size_t size,
                    size_t maxSize, unsigned int seed);

#endif  // SHARED_H_
