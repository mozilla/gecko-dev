/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_FontFeature
#define mozilla_gfx_FontFeature

#include <cstdint>

namespace mozilla::gfx {
// An OpenType feature tag and value pair
struct FontFeature {
  // see http://www.microsoft.com/typography/otspec/featuretags.htm
  uint32_t mTag;
  // 0 = off, 1 = on, larger values may be used as parameters
  // to features that select among multiple alternatives
  uint32_t mValue;
};

inline bool operator<(const FontFeature& a, const FontFeature& b) {
  return (a.mTag < b.mTag) || ((a.mTag == b.mTag) && (a.mValue < b.mValue));
}

inline bool operator==(const FontFeature& a, const FontFeature& b) {
  return (a.mTag == b.mTag) && (a.mValue == b.mValue);
}

}  // namespace mozilla::gfx

#endif
