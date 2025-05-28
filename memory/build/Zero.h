/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ZERO_H
#define ZERO_H

#include "Globals.h"
#include "mozjemalloc.h"

#include <string.h>

namespace mozilla {

static inline void MaybePoison(void* aPtr, size_t aSize) {
  size_t size;
  switch (opt_poison) {
    case NONE:
      return;
    case SOME:
      size = std::min(aSize, opt_poison_size);
      break;
    case ALL:
      size = aSize;
      break;
  }
  MOZ_ASSERT(size != 0 && size <= aSize);
  memset(aPtr, kAllocPoison, size);
}

// Fill the given range of memory with zeroes or junk depending on opt_junk and
// opt_zero.
static inline void ApplyZeroOrJunk(void* aPtr, size_t aSize) {
  if (opt_junk) {
    memset(aPtr, kAllocJunk, aSize);
  } else if (opt_zero) {
    memset(aPtr, 0, aSize);
  }
}

}  // namespace mozilla

#endif  // ! ZERO_H
