/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_markers_h
#define memory_markers_h

#if defined(MOZ_PROFILER_MEMORY) && defined(MOZJEMALLOC_PROFILING_CALLBACKS)
namespace mozilla {

void register_profiler_memory_callbacks();
void unregister_profiler_memory_callbacks();

}  // namespace mozilla
#endif

#endif
