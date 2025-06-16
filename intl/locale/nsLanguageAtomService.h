/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The nsILanguageAtomService provides a mapping from languages or charsets
 * to language groups, and access to the system locale language.
 */

#ifndef nsLanguageAtomService_h_
#define nsLanguageAtomService_h_

#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/RWLock.h"
#include "mozilla/StaticPtr.h"
#include "nsAtomHashKeys.h"
#include "nsTHashMap.h"

namespace mozilla {
class Encoding;
}

class nsLanguageAtomService final {
  using Encoding = mozilla::Encoding;
  template <typename T>
  using NotNull = mozilla::NotNull<T>;

 public:
  static nsLanguageAtomService* GetService();

  static void Shutdown();

  nsStaticAtom* LookupLanguage(const nsACString& aLanguage);
  nsAtom* GetLocaleLanguage();

  // Returns the language group that the specified language is a part of,
  // using a cache to avoid repeatedly doing full lookups.
  nsStaticAtom* GetLanguageGroup(nsAtom* aLanguage);

 private:
  // The core implementation of lang-tag to language-group lookup. (Now used
  // only internally by GetLanguageGroup.)
  nsStaticAtom* GetUncachedLanguageGroup(nsAtom* aLanguage) const;

  static mozilla::StaticAutoPtr<nsLanguageAtomService> sLangAtomService;

  nsTHashMap<RefPtr<nsAtom>, nsStaticAtom*> mLangToGroup MOZ_GUARDED_BY(mLock);
  RefPtr<nsAtom> mLocaleLanguage MOZ_GUARDED_BY(mLock);

  mozilla::RWLock mLock{"LanguageAtomService"};
};

#endif
