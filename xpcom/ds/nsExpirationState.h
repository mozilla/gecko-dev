/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsExpirationState_h_
#define nsExpirationState_h_

#include <cstdint>

// Data used to track the expiration state of an object with nsExpirationTracker
struct nsExpirationState {
  enum {
    NOT_TRACKED = (1U << 4) - 1,
    MAX_INDEX_IN_GENERATION = (1U << 28) - 1
  };

  constexpr nsExpirationState()
      : mGeneration(NOT_TRACKED), mIndexInGeneration(0) {}
  bool IsTracked() const { return mGeneration != NOT_TRACKED; }

  // The generation that this object belongs to, or NOT_TRACKED.
  uint32_t mGeneration : 4;
  uint32_t mIndexInGeneration : 28;
};

// We promise that this is 32 bits so that objects that includes this as a
// field can pad and align efficiently.
static_assert(sizeof(nsExpirationState) == sizeof(uint32_t));

#endif
