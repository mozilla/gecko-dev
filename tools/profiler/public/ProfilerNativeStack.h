/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ProfilerNativeStack_h
#define ProfilerNativeStack_h

#include <stddef.h>

// Setting MAX_NATIVE_FRAMES too high risks the unwinder wasting a lot of time
// looping on corrupted stacks.

#ifdef __cplusplus
static const size_t MAX_NATIVE_FRAMES = 1024;
#else
#  define MAX_NATIVE_FRAMES 1024
#endif

struct NativeStack {
  void* mPCs[MAX_NATIVE_FRAMES];
  void* mSPs[MAX_NATIVE_FRAMES];
  size_t mCount;  // Number of frames filled.
};

#endif  // ProfilerNativeStack_h
