/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RAND_UTIL_H_
#define BASE_RAND_UTIL_H_

#include "base/basictypes.h"

namespace base {

// Returns a random number in range [0, kuint64max]. Thread-safe.
uint64_t RandUint64();

// Returns a random number between min and max (inclusive). Thread-safe.
int RandInt(int min, int max);

// Returns a random double in range [0, 1). Thread-safe.
double RandDouble();

// Fills |output_length| bytes of |output| with random data.
//
// WARNING:
// Do not use for security-sensitive purposes.
// See crypto/ for cryptographically secure random number generation APIs.
void RandBytes(void* output, size_t output_length);

}  // namespace base

#endif // BASE_RAND_UTIL_H_
