/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_LineBreakCache_h__
#define mozilla_intl_LineBreakCache_h__

#include "nsIObserver.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/MruCache.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/intl/Segmenter.h"

namespace mozilla {
namespace intl {

namespace detail {
struct LBCacheKey {
  const char16_t* mText;
  uint32_t mLength;
  // ICU4X segmenter results depend on these flags, so they need to be part
  // of the cache key. (Legacy ComplexBreaker just leaves them as default.)
  WordBreakRule mWordBreak = WordBreakRule::Normal;
  LineBreakRule mLineBreak = LineBreakRule::Auto;
  bool mIsChineseOrJapanese = false;
};

struct LBCacheEntry {
  nsString mText;
  nsTArray<uint8_t> mBreaks;
  WordBreakRule mWordBreak = WordBreakRule::Normal;
  LineBreakRule mLineBreak = LineBreakRule::Auto;
  bool mIsChineseOrJapanese = false;
};
}  // namespace detail

// Most-recently-used cache for line-break results, because finding line-
// breaks may be slow for complex writing systems (e.g. Thai, Khmer).
// The MruCache size should be a prime number that is slightly less than a
// power of two.
class LineBreakCache : public MruCache<detail::LBCacheKey, detail::LBCacheEntry,
                                       LineBreakCache, 4093> {
 public:
  static void Initialize();
  static void Shutdown();

  using KeyType = detail::LBCacheKey;
  using EntryType = detail::LBCacheEntry;

  static LineBreakCache* Cache() {
    if (!sBreakCache) {
      sBreakCache = new LineBreakCache();
    }
    return sBreakCache;
  }

  static HashNumber Hash(const KeyType& aKey) {
    HashNumber h = HashString(aKey.mText, aKey.mLength);
    h = AddToHash(h, aKey.mWordBreak);
    h = AddToHash(h, aKey.mLineBreak);
    h = AddToHash(h, aKey.mIsChineseOrJapanese);
    return h;
  }

  static bool Match(const KeyType& aKey, const EntryType& aEntry) {
    return nsDependentSubstring(aKey.mText, aKey.mLength)
               .Equals(aEntry.mText) &&
           aKey.mWordBreak == aEntry.mWordBreak &&
           aKey.mLineBreak == aEntry.mLineBreak &&
           aKey.mIsChineseOrJapanese == aEntry.mIsChineseOrJapanese;
  }

  static void CopyAndFill(const nsTArray<uint8_t>& aCachedBreakBefore,
                          uint8_t* aBreakBefore, uint8_t* aEndBreakBefore) {
    auto* startFill = std::copy(aCachedBreakBefore.begin(),
                                aCachedBreakBefore.end(), aBreakBefore);
    std::fill(startFill, aEndBreakBefore, false);
  }

  class Observer final : public nsIObserver {
    ~Observer() = default;

   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
  };

 private:
  static StaticAutoPtr<LineBreakCache> sBreakCache;
};

}  // namespace intl
}  // namespace mozilla

#endif /* mozilla_intl_LineBreakCache_h__ */
