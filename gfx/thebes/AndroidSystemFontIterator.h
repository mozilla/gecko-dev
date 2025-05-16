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

typedef ASystemFontIterator* _Nullable (*_ASystemFontIterator_open)();
typedef AFont* _Nullable (*_ASystemFontIterator_next)(
    ASystemFontIterator* _Nonnull iterator);
typedef void (*_ASystemFontIterator_close)(
    ASystemFontIterator* _Nullable iterator);
typedef const char* _Nonnull (*_AFont_getFontFilePath)(
    const AFont* _Nonnull font);
typedef void (*_AFont_close)(AFont* _Nullable font);

class AndroidFont final {
 public:
  explicit AndroidFont(AFont* _Nullable aFont) : mFont(aFont) {};

  AndroidFont() = delete;
  AndroidFont(AndroidFont&) = delete;

  AndroidFont(AndroidFont&& aSrc) {
    mFont = aSrc.mFont;
    aSrc.mFont = nullptr;
  }

  ~AndroidFont();

  const char* _Nullable GetFontFilePath();

 private:
  AFont* _Nullable mFont;

  static _AFont_getFontFilePath _Nullable sFont_getFontFilePath;
  static _AFont_close _Nullable sFont_close;

  friend class AndroidSystemFontIterator;
};

class AndroidSystemFontIterator final {
 public:
  AndroidSystemFontIterator() = default;

  ~AndroidSystemFontIterator();

  bool Init();

  Maybe<AndroidFont> Next();

 private:
  ASystemFontIterator* _Nullable mIterator = nullptr;

  static _ASystemFontIterator_open _Nullable sSystemFontIterator_open;
  static _ASystemFontIterator_next _Nullable sSystemFontIterator_next;
  static _ASystemFontIterator_close _Nullable sSystemFontIterator_close;
};

}  // namespace mozilla

#endif
