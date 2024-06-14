/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The Gecko Profiler is an always-on profiler that takes fast and low overhead
// samples of the program execution using only userspace functionality for
// portability. The goal of this module is to provide performance data in a
// generic cross-platform way without requiring custom tools or kernel support.
//
// Samples are collected to form a timeline with optional timeline event
// (markers) used for filtering. The samples include both native stacks and
// platform-independent "label stack" frames.

#ifndef ProfilerStackWalk_h
#define ProfilerStackWalk_h

#include "PlatformMacros.h"
#include "ProfilerNativeStack.h"

#include "mozilla/ProfileChunkedBuffer.h"

class StackWalkControl;

void DoNativeBacktraceDirect(const void* stackTop, NativeStack& aNativeStack,
                             StackWalkControl* aStackWalkControlIfSupported);

bool profiler_backtrace_into_buffer(
    mozilla::ProfileChunkedBuffer& aChunkedBuffer, NativeStack& aNativeStack);

#endif  // ProfilerStackWalk_h
