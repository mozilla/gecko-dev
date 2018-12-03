/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLanguageAtomService.h"
#include "nsUConvPropertySearch.h"
#include "nsUnicharUtils.h"
#include "nsAtom.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Encoding.h"
#include "mozilla/intl/OSPreferences.h"
#include "mozilla/ServoBindings.h"

using namespace mozilla;
using mozilla::intl::OSPreferences;

static constexpr nsUConvProp encodingsGroups[] = {
#include "encodingsgroups.properties.h"
};

// List of mozilla internal x-* tags that map to themselves (see bug 256257)
static constexpr const char* kLangGroups[] = {
    // This list must be sorted!
    "x-armn",  "x-cyrillic", "x-devanagari", "x-geor", "x-math",
    "x-tamil", "x-unicode",  "x-western"
    // These self-mappings are not necessary unless somebody use them to specify
    // lang in (X)HTML/XML documents, which they shouldn't. (see bug 256257)
    // x-beng=x-beng
    // x-cans=x-cans
    // x-ethi=x-ethi
    // x-guru=x-guru
    // x-gujr=x-gujr
    // x-khmr=x-khmr
    // x-mlym=x-mlym
};

// Map ISO 15924 script codes from BCP47 lang tag to mozilla's langGroups.
static constexpr struct {
  const char* mTag;
  nsAtom* mAtom;
} kScriptLangGroup[] = {
    // This list must be sorted by script code!
    {"Arab", nsGkAtoms::ar},
    {"Armn", nsGkAtoms::x_armn},
    {"Beng", nsGkAtoms::x_beng},
    {"Cans", nsGkAtoms::x_cans},
    {"Cyrl", nsGkAtoms::x_cyrillic},
    {"Deva", nsGkAtoms::x_devanagari},
    {"Ethi", nsGkAtoms::x_ethi},
    {"Geok", nsGkAtoms::x_geor},
    {"Geor", nsGkAtoms::x_geor},
    {"Grek", nsGkAtoms::el},
    {"Gujr", nsGkAtoms::x_gujr},
    {"Guru", nsGkAtoms::x_guru},
    {"Hang", nsGkAtoms::ko},
    {"Hani", nsGkAtoms::Japanese},
    {"Hans", nsGkAtoms::Chinese},
    // Hant is special-cased in code
    // Hant=zh-HK
    // Hant=zh-TW
    {"Hebr", nsGkAtoms::he},
    {"Hira", nsGkAtoms::Japanese},
    {"Jpan", nsGkAtoms::Japanese},
    {"Kana", nsGkAtoms::Japanese},
    {"Khmr", nsGkAtoms::x_khmr},
    {"Knda", nsGkAtoms::x_knda},
    {"Kore", nsGkAtoms::ko},
    {"Latn", nsGkAtoms::x_western},
    {"Mlym", nsGkAtoms::x_mlym},
    {"Orya", nsGkAtoms::x_orya},
    {"Sinh", nsGkAtoms::x_sinh},
    {"Taml", nsGkAtoms::x_tamil},
    {"Telu", nsGkAtoms::x_telu},
    {"Thai", nsGkAtoms::th},
    {"Tibt", nsGkAtoms::x_tibt}};

// static
nsLanguageAtomService* nsLanguageAtomService::GetService() {
  static UniquePtr<nsLanguageAtomService> gLangAtomService;
  if (!gLangAtomService) {
    gLangAtomService = MakeUnique<nsLanguageAtomService>();
    ClearOnShutdown(&gLangAtomService);
  }
  return gLangAtomService.get();
}

nsAtom* nsLanguageAtomService::LookupLanguage(const nsACString& aLanguage) {
  nsAutoCString lowered(aLanguage);
  ToLowerCase(lowered);

  RefPtr<nsAtom> lang = NS_Atomize(lowered);
  return GetLanguageGroup(lang);
}

already_AddRefed<nsAtom> nsLanguageAtomService::LookupCharSet(
    NotNull<const Encoding*> aEncoding) {
  nsAutoCString charset;
  aEncoding->Name(charset);
  nsAutoCString group;
  if (NS_FAILED(nsUConvPropertySearch::SearchPropertyValue(
          encodingsGroups, ArrayLength(encodingsGroups), charset, group))) {
    return RefPtr<nsAtom>(nsGkAtoms::Unicode).forget();
  }
  return NS_Atomize(group);
}

nsAtom* nsLanguageAtomService::GetLocaleLanguage() {
  do {
    if (!mLocaleLanguage) {
      AutoTArray<nsCString, 10> regionalPrefsLocales;
      if (NS_SUCCEEDED(OSPreferences::GetInstance()->GetRegionalPrefsLocales(
              regionalPrefsLocales))) {
        // use lowercase for all language atoms
        ToLowerCase(regionalPrefsLocales[0]);
        mLocaleLanguage = NS_Atomize(regionalPrefsLocales[0]);
      } else {
        nsAutoCString locale;
        OSPreferences::GetInstance()->GetSystemLocale(locale);

        ToLowerCase(locale);  // use lowercase for all language atoms
        mLocaleLanguage = NS_Atomize(locale);
      }
    }
  } while (0);

  return mLocaleLanguage;
}

nsAtom* nsLanguageAtomService::GetLanguageGroup(nsAtom* aLanguage,
                                                bool* aNeedsToCache) {
  nsAtom* retVal = mLangToGroup.GetWeak(aLanguage);

  if (!retVal) {
    if (aNeedsToCache) {
      *aNeedsToCache = true;
      return nullptr;
    }
    RefPtr<nsAtom> uncached = GetUncachedLanguageGroup(aLanguage);
    retVal = uncached.get();

    AssertIsMainThreadOrServoFontMetricsLocked();
    // The hashtable will keep an owning reference to the atom
    mLangToGroup.Put(aLanguage, uncached);
  }

  return retVal;
}

already_AddRefed<nsAtom> nsLanguageAtomService::GetUncachedLanguageGroup(
    nsAtom* aLanguage) const {
  nsAutoCString langStr;
  aLanguage->ToUTF8String(langStr);
  ToLowerCase(langStr);

  RefPtr<nsAtom> langGroup;
  if (langStr[0] == 'x' && langStr[1] == '-') {
    // Internal x-* langGroup codes map to themselves (see bug 256257)
    size_t unused;
    if (BinarySearchIf(
            kLangGroups, 0, ArrayLength(kLangGroups),
            [&langStr](const char* tag) -> int { return langStr.Compare(tag); },
            &unused)) {
      langGroup = NS_Atomize(langStr);
      return langGroup.forget();
    }
  } else {
    // If the lang code can be parsed as BCP47, look up its (likely) script
    Locale loc(langStr);
    if (loc.IsWellFormed()) {
      if (loc.GetScript().IsEmpty()) {
        loc.AddLikelySubtags();
      }
      if (loc.GetScript().EqualsLiteral("Hant")) {
        if (loc.GetRegion().EqualsLiteral("HK")) {
          langGroup = nsGkAtoms::HongKongChinese;
        } else {
          langGroup = nsGkAtoms::Taiwanese;
        }
        return langGroup.forget();
      } else {
        size_t foundIndex;
        const nsCString& script = loc.GetScript();
        if (BinarySearchIf(kScriptLangGroup, 0, ArrayLength(kScriptLangGroup),
                           [script](const auto& entry) -> int {
                             return script.Compare(entry.mTag);
                           },
                           &foundIndex)) {
          langGroup = kScriptLangGroup[foundIndex].mAtom;
          return langGroup.forget();
        }
      }
    }
  }

  // Fall back to x-unicode if no match was found
  langGroup = nsGkAtoms::Unicode;
  return langGroup.forget();
}
