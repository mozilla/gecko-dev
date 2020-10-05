/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_ProcessRecordReplay_h
#define mozilla_recordreplay_ProcessRecordReplay_h

#include "mozilla/Atomics.h"
#include "mozilla/PodOperations.h"
#include "mozilla/RecordReplay.h"
#include "nsString.h"

#include <algorithm>

namespace mozilla::recordreplay {

static inline bool TestEnv(const char* env) {
  const char* value = getenv(env);
  return value && value[0];
}

}  // namespace mozilla::recordreplay

#endif  // mozilla_recordreplay_ProcessRecordReplay_h
