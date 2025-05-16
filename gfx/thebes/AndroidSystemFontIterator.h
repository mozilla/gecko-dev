/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidSystemFontIterator_h__
#define AndroidSystemFontIterator_h__

#include "mozilla/Maybe.h"

namespace mozilla {

typedef void* (*_ASystemFontIterator_open)();
typedef void* (*_ASystemFontIterator_next)(void*);
typedef void (*_ASystemFontIterator_close)(void*);

typedef const char* (*_AFont_getFontFilePath)(const void*);
typedef void (*_AFont_close)(void*);

class AndroidFont final {
 public:
  explicit AndroidFont(void* aFont) : mFont(aFont) {};

  AndroidFont() = delete;
  AndroidFont(AndroidFont&) = delete;

  AndroidFont(AndroidFont&& aSrc) {
    mFont = aSrc.mFont;
    aSrc.mFont = nullptr;
  }

  ~AndroidFont();

  const char* GetFontFilePath();

 private:
  void* mFont;

  static _AFont_getFontFilePath sFont_getFontFilePath;
  static _AFont_close sFont_close;

  friend class AndroidSystemFontIterator;
};

class AndroidSystemFontIterator final {
 public:
  AndroidSystemFontIterator() = default;

  ~AndroidSystemFontIterator();

  bool Init();

  Maybe<AndroidFont> Next();

 private:
  void* mIterator = nullptr;

  static _ASystemFontIterator_open sSystemFontIterator_open;
  static _ASystemFontIterator_next sSystemFontIterator_next;
  static _ASystemFontIterator_close sSystemFontIterator_close;
};

}  // namespace mozilla

#endif
