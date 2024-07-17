/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheControlParser.h"

namespace mozilla {
namespace net {

CacheControlParser::CacheControlParser(nsACString const& aHeader)
    : Tokenizer(aHeader, nullptr, "-_"),
      mMaxAgeSet(false),
      mMaxAge(0),
      mMaxStaleSet(false),
      mMaxStale(0),
      mMinFreshSet(false),
      mMinFresh(0),
      mStaleWhileRevalidateSet(false),
      mStaleWhileRevalidate(0),
      mNoCache(false),
      mNoStore(false),
      mPublic(false),
      mPrivate(false),
      mImmutable(false) {
  mDirectiveTokens[NO_CACHE] = AddCustomToken("no-cache", CASE_INSENSITIVE);
  mDirectiveTokens[NO_STORE] = AddCustomToken("no-store", CASE_INSENSITIVE);
  mDirectiveTokens[MAX_AGE] = AddCustomToken("max-age", CASE_INSENSITIVE);
  mDirectiveTokens[MAX_STALE] = AddCustomToken("max-stale", CASE_INSENSITIVE);
  mDirectiveTokens[MIN_FRESH] = AddCustomToken("min-fresh", CASE_INSENSITIVE);
  mDirectiveTokens[STALE_WHILE_REVALIDATE] =
      AddCustomToken("stale-while-revalidate", CASE_INSENSITIVE);
  mDirectiveTokens[PUBLIC] = AddCustomToken("public", CASE_INSENSITIVE);
  mDirectiveTokens[PRIVATE] = AddCustomToken("private", CASE_INSENSITIVE);
  mDirectiveTokens[IMMUTABLE] = AddCustomToken("immutable", CASE_INSENSITIVE);

  SkipWhites();
  if (!CheckEOF()) {
    Directive();
  }
}

void CacheControlParser::Directive() {
  do {
    SkipWhites();
    if (Check(mDirectiveTokens[NO_CACHE])) {
      mNoCache = true;
      IgnoreDirective();  // ignore any optionally added values
    } else if (Check(mDirectiveTokens[NO_STORE])) {
      mNoStore = true;
    } else if (Check(mDirectiveTokens[MAX_AGE])) {
      mMaxAgeSet = SecondsValue(&mMaxAge);
    } else if (Check(mDirectiveTokens[MAX_STALE])) {
      mMaxStaleSet = SecondsValue(&mMaxStale, PR_UINT32_MAX);
    } else if (Check(mDirectiveTokens[MIN_FRESH])) {
      mMinFreshSet = SecondsValue(&mMinFresh);
    } else if (Check(mDirectiveTokens[STALE_WHILE_REVALIDATE])) {
      mStaleWhileRevalidateSet = SecondsValue(&mStaleWhileRevalidate);
    } else if (Check(mDirectiveTokens[PUBLIC])) {
      mPublic = true;
    } else if (Check(mDirectiveTokens[PRIVATE])) {
      mPrivate = true;
    } else if (Check(mDirectiveTokens[IMMUTABLE])) {
      mImmutable = true;
    } else {
      IgnoreDirective();
    }

    SkipWhites();
    if (CheckEOF()) {
      return;
    }

  } while (CheckChar(','));

  NS_WARNING("Unexpected input in Cache-control header value");
}

bool CacheControlParser::SecondsValue(uint32_t* seconds, uint32_t defaultVal) {
  SkipWhites();
  if (!CheckChar('=')) {
    IgnoreDirective();
    *seconds = defaultVal;
    return !!defaultVal;
  }

  SkipWhites();
  if (!ReadInteger(seconds)) {
    NS_WARNING("Unexpected value in Cache-control header value");
    IgnoreDirective();
    *seconds = defaultVal;
    return !!defaultVal;
  }

  return true;
}

void CacheControlParser::IgnoreDirective() {
  Token t;
  while (Next(t)) {
    if (t.Equals(Token::Char(',')) || t.Equals(Token::EndOfFile())) {
      Rollback();
      break;
    }
    if (t.Equals(Token::Char('"'))) {
      SkipUntil(Token::Char('"'));
      if (!CheckChar('"')) {
        NS_WARNING(
            "Missing quoted string expansion in Cache-control header value");
        break;
      }
    }
  }
}

bool CacheControlParser::MaxAge(uint32_t* seconds) {
  *seconds = mMaxAge;
  return mMaxAgeSet;
}

bool CacheControlParser::MaxStale(uint32_t* seconds) {
  *seconds = mMaxStale;
  return mMaxStaleSet;
}

bool CacheControlParser::MinFresh(uint32_t* seconds) {
  *seconds = mMinFresh;
  return mMinFreshSet;
}

bool CacheControlParser::StaleWhileRevalidate(uint32_t* seconds) {
  *seconds = mStaleWhileRevalidate;
  return mStaleWhileRevalidateSet;
}

bool CacheControlParser::NoCache() { return mNoCache; }

bool CacheControlParser::NoStore() { return mNoStore; }

bool CacheControlParser::Public() { return mPublic; }

bool CacheControlParser::Private() { return mPrivate; }

bool CacheControlParser::Immutable() { return mImmutable; }

}  // namespace net
}  // namespace mozilla
