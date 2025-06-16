/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_EncodingToLang_h
#define mozilla_intl_EncodingToLang_h

#include "nsAtom.h"
#include "mozilla/Encoding.h"

namespace mozilla::intl {

class EncodingToLang {
 public:
  // Call once from nsLayoutStatics::Initialize()
  static void Initialize();
  // Call once from nsLayoutStatics::Shutdown()
  static void Shutdown();

  // Looks up a font matching language atom by encoding.
  // The atom will be kept alive until nsLayoutStatics::Shutdown(),
  // which is why it's a raw pointer.
  static nsAtom* Lookup(mozilla::NotNull<const mozilla::Encoding*> aEncoding);

 private:
  static nsAtom* sLangs[];
  static const mozilla::NotNull<const mozilla::Encoding *> *
      kEncodingsByRoughFrequency[];
};

};  // namespace mozilla::intl

#endif  // mozilla_intl_EncodingToLang_h
