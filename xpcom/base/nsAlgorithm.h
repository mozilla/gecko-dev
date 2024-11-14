/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlgorithm_h___
#define nsAlgorithm_h___

#include <cstdint>
#include "mozilla/Assertions.h"

// We use these instead of std::min/max because we can't include the algorithm
// header in all of XPCOM because the stl wrappers will error out when included
// in parts of XPCOM. These functions should never be used outside of XPCOM.
template <class T>
inline const T& XPCOM_MIN(const T& aA, const T& aB) {
  return aB < aA ? aB : aA;
}

// Must return b when a == b in case a is -0
template <class T>
inline const T& XPCOM_MAX(const T& aA, const T& aB) {
  return aA > aB ? aA : aB;
}

#endif  // !defined(nsAlgorithm_h___)
