/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidSystemFontIterator_h__
#define AndroidSystemFontIterator_h__

#include "mozilla/Maybe.h"

#include <android/font.h>
#include <android/system_fonts.h>

namespace mozilla {

class __attribute__((
    availability(android, introduced = 29))) AndroidFont final {
 public:
  explicit AndroidFont(AFont* _Nullable aFont) : mFont(aFont) {};

  AndroidFont() = delete;
  AndroidFont(const AndroidFont&) = delete;

  AndroidFont(AndroidFont&& aSrc) {
    mFont = aSrc.mFont;
    aSrc.mFont = nullptr;
  }

  ~AndroidFont();

  const char* _Nullable GetFontFilePath();

 private:
  AFont* _Nullable mFont;
};

class __attribute__((
    availability(android, introduced = 29))) AndroidSystemFontIterator final {
 public:
  AndroidSystemFontIterator();
  ~AndroidSystemFontIterator();

  static void Preload();

  Maybe<AndroidFont> Next();

 private:
  ASystemFontIterator* _Nullable mIterator;
};

}  // namespace mozilla

#endif
