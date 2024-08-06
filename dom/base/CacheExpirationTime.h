/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CacheExpirationTime_h___
#define mozilla_dom_CacheExpirationTime_h___

#include <stdint.h>              // uint32_t
#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "prtime.h"              // PRTime, PR_USEC_PER_SEC

/*
 * The expiration time for sub resource cache.
 */
struct CacheExpirationTime {
 private:
  uint32_t mTime;

  static constexpr uint32_t kNever = 0;

  constexpr CacheExpirationTime() : mTime(kNever) {}

  explicit constexpr CacheExpirationTime(uint32_t aTime) : mTime(aTime) {}

  static uint32_t SecondsFromPRTime(PRTime aTime) {
    return uint32_t(int64_t(aTime) / int64_t(PR_USEC_PER_SEC));
  }

 public:
  static CacheExpirationTime AlreadyExpired() {
    return CacheExpirationTime(SecondsFromPRTime(PR_Now()) - 1);
  }

  static constexpr CacheExpirationTime Never() {
    return CacheExpirationTime(kNever);
  }

  static constexpr CacheExpirationTime ExpireAt(uint32_t aTime) {
    return CacheExpirationTime(aTime);
  }

  bool IsExpired() const {
    if (IsNever()) {
      return false;
    }
    return mTime <= SecondsFromPRTime(PR_Now());
  }

  bool IsNever() const { return mTime == kNever; }

  bool IsShorterThan(const CacheExpirationTime& aOther) const {
    MOZ_ASSERT(!IsNever());
    MOZ_ASSERT(!aOther.IsNever());
    return mTime < aOther.mTime;
  }

  void SetMinimum(const CacheExpirationTime& aOther) {
    if (aOther.IsNever()) {
      return;
    }

    if (IsNever() || aOther.IsShorterThan(*this)) {
      mTime = aOther.mTime;
    }
  }
};

#endif /* mozilla_dom_CacheExpirationTime_h___ */
