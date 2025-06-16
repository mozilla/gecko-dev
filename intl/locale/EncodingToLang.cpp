/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/EncodingToLang.h"
#include "nsGkAtoms.h"
#include "nsLanguageAtomService.h"

using namespace mozilla;
using namespace mozilla::intl;

// Parallel arrays of Encoding and corresponding Lang atoms,
// in rough order of frequency.

// Unfortunately, the `mozilla::NotNull` hack that was used to
// declare the encoding pointers in C++ does not allow putting
// the pointers in a static array without a run-time initializer,
// so our options are:
// 1. Putting the pointers in a static array in Rust, at a distance.
// 2. Run-time initializer.
// 3. Using pointer pointers, as seen here.
const mozilla::NotNull<const mozilla::Encoding *> *
    EncodingToLang::kEncodingsByRoughFrequency[] = {
#define _(encoding, lang) &encoding,
#include "EncodingsByFrequency.inc"
#undef _
};

// This one isn't constant, as it gets adjusted during Initialize().
// static
nsAtom* EncodingToLang::sLangs[] = {
#define _(encoding, lang) lang,
#include "EncodingsByFrequency.inc"
#undef _
};

// static
nsAtom* EncodingToLang::Lookup(NotNull<const mozilla::Encoding*> aEncoding) {
  // Linear search should be fine, since in the vast, vast majority of cases,
  // the search stops at the first or second item.
  unsigned int i = 0;
  for (; i < std::size(kEncodingsByRoughFrequency); i++) {
    if (*kEncodingsByRoughFrequency[i] == aEncoding) {
      return sLangs[i];
    }
  }
  MOZ_ASSERT(false, "The encoding is always supposed to be found in the array");
  return sLangs[0];
}

// static
void EncodingToLang::Initialize() {
  sLangs[0] = nsLanguageAtomService::GetService()->GetLocaleLanguage();
  // We logically hold a strong ref to the first occurrence
  // and a non-owning pointer to the rest.
  NS_ADDREF(sLangs[0]);
  for (size_t i = 1; i < std::size(sLangs); ++i) {
    if (!sLangs[i]) {
      sLangs[i] = sLangs[0];
    }
  }
}

// static
void EncodingToLang::Shutdown() { NS_RELEASE(sLangs[0]); }
