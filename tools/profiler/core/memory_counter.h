/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_counter_h
#define memory_counter_h

#include "mozilla/UniquePtr.h"

class BaseProfilerCount;

#if defined(MOZ_MEMORY) && defined(MOZ_PROFILER_MEMORY)

namespace mozilla {
namespace profiler {

UniquePtr<BaseProfilerCount> create_memory_counter();

}  // namespace profiler
}  // namespace mozilla
#endif

#endif
