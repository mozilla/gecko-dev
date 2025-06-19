/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef URLPatternGlue_h__
#define URLPatternGlue_h__

#include "mozilla/net/urlpattern_glue.h"
#include "nsTHashMap.h"
#include "mozilla/Maybe.h"
#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gUrlPatternLog;

inline bool operator==(const UrlpInnerMatcher& a, const UrlpInnerMatcher& b) {
  if (a.inner_type != b.inner_type) {
    return false;
  }

  switch (a.inner_type) {
    case UrlpInnerMatcherType::Literal: {
      return a.literal.Equals(b.literal);
    }
    case UrlpInnerMatcherType::SingleCapture: {
      if (a.allow_empty == b.allow_empty &&
          a.filter_exists == b.filter_exists) {
        if (a.filter_exists) {
          return a.filter == b.filter;
        }
        return true;
      }
      return false;
    }
    case UrlpInnerMatcherType::RegExp: {
      return a.regexp.Equals(b.regexp);
    }
    default: {
      return false;
    }
  }
}

inline bool operator==(const UrlpMatcher& a, const UrlpMatcher& b) {
  return a.prefix.Equals(b.prefix) && a.suffix.Equals(b.suffix) &&
         a.inner == b.inner;
}

namespace mozilla::net {

UrlpInput CreateUrlpInput(const nsACString& url);
UrlpInput CreateUrlpInput(const UrlpInit& init);

MaybeString CreateMaybeString(const nsACString& str, bool valid);
MaybeString CreateMaybeStringNone();

class UrlpComponentResult {
 public:
  UrlpComponentResult() = default;
  UrlpComponentResult(const UrlpComponentResult& aOther)
      : mInput(aOther.mInput) {
    for (auto iter = aOther.mGroups.ConstIter(); !iter.Done(); iter.Next()) {
      mGroups.InsertOrUpdate(iter.Key(), iter.Data());
    }
  }
  UrlpComponentResult(
      UrlpComponentResult&& aOther) noexcept  // move constructor
      : mInput(std::move(aOther.mInput)), mGroups(std::move(aOther.mGroups)) {}
  UrlpComponentResult& operator=(
      UrlpComponentResult&& aOther) noexcept {  // move assignment
    if (this != &aOther) {
      mInput = std::move(aOther.mInput);
      mGroups = std::move(aOther.mGroups);
    }
    return *this;
  }

  nsAutoCString mInput;
  nsTHashMap<nsCStringHashKey, MaybeString> mGroups;
};

class UrlpResult {
 public:
  UrlpResult() = default;
  Maybe<UrlpComponentResult> mProtocol;
  Maybe<UrlpComponentResult> mUsername;
  Maybe<UrlpComponentResult> mPassword;
  Maybe<UrlpComponentResult> mHostname;
  Maybe<UrlpComponentResult> mPort;
  Maybe<UrlpComponentResult> mPathname;
  Maybe<UrlpComponentResult> mSearch;
  Maybe<UrlpComponentResult> mHash;
  CopyableTArray<UrlpInput> mInputs;
};

Maybe<UrlpResult> UrlpPatternExec(UrlpPattern aPattern, const UrlpInput& aInput,
                                  Maybe<nsAutoCString> aMaybeBaseUrl,
                                  bool aIgnoreCase = false);

bool UrlpPatternTest(UrlpPattern aPattern, const UrlpInput& aInput,
                     Maybe<nsAutoCString> aMaybeBaseUrl,
                     bool aIgnoreCase = false);

nsAutoCString UrlpGetProtocol(const UrlpPattern& aPatternWrapper);
nsAutoCString UrlpGetUsername(const UrlpPattern& aPatternWrapper);
nsAutoCString UrlpGetPassword(const UrlpPattern& aPatternWrapper);
nsAutoCString UrlpGetHostname(const UrlpPattern& aPatternWrapper);
nsAutoCString UrlpGetPort(const UrlpPattern& aPatternWrapper);
nsAutoCString UrlpGetPathname(const UrlpPattern& aPatternWrapper);
nsAutoCString UrlpGetSearch(const UrlpPattern& aPatternWrapper);
nsAutoCString UrlpGetHash(const UrlpPattern& aPatternWrapper);

}  // namespace mozilla::net

#endif  // URLPatternGlue_h__
