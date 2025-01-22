/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_ScalarGifftMap_h
#define mozilla_glean_ScalarGifftMap_h

#include "mozilla/Maybe.h"
#include "mozilla/Telemetry.h"

namespace mozilla::glean {

using Telemetry::ScalarID;

static inline bool IsSubmetricId(uint32_t aId) {
  // Submetrics have the 2^25 bit set.
  // (ID_BITS - ID_SIGNAL_BITS, keep it in sync with js.py).
  return (aId & (1 << 25)) > 0;
}

Maybe<ScalarID> ScalarIdForMetric(uint32_t aId);

}  // namespace mozilla::glean

#endif  // mozilla_glean_ScalarGifftMaps_h
