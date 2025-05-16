/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidSystemFontIterator.h"

#include "mozilla/Assertions.h"
#include "nsDebug.h"

#include <android/system_fonts.h>

namespace mozilla {

_ASystemFontIterator_open AndroidSystemFontIterator::sSystemFontIterator_open =
    nullptr;
_ASystemFontIterator_next AndroidSystemFontIterator::sSystemFontIterator_next =
    nullptr;
_ASystemFontIterator_close
    AndroidSystemFontIterator::sSystemFontIterator_close = nullptr;

_AFont_getFontFilePath AndroidFont::sFont_getFontFilePath = nullptr;
_AFont_close AndroidFont::sFont_close = nullptr;

AndroidSystemFontIterator::~AndroidSystemFontIterator() {
  if (!sSystemFontIterator_open) {
    return;
  }

  if (!mIterator) {
    return;
  }

  sSystemFontIterator_close(mIterator);
}

bool AndroidSystemFontIterator::Init() {
  if (!sSystemFontIterator_open) {
    if (__builtin_available(android 29, *)) {
      sSystemFontIterator_open = ASystemFontIterator_open;
      sSystemFontIterator_next = ASystemFontIterator_next;
      sSystemFontIterator_close = ASystemFontIterator_close;
      AndroidFont::sFont_getFontFilePath = AFont_getFontFilePath;
      AndroidFont::sFont_close = AFont_close;
    } else {
      return NS_WARN_IF(false);
    }
  }

  mIterator = sSystemFontIterator_open();

  return true;
}

Maybe<AndroidFont> AndroidSystemFontIterator::Next() {
  if (NS_WARN_IF(!sSystemFontIterator_open)) {
    return Nothing();
  }

  if (!mIterator) {
    return Nothing();
  }

  AFont* font = sSystemFontIterator_next(mIterator);
  if (!font) {
    sSystemFontIterator_close(mIterator);
    mIterator = nullptr;
    return Nothing();
  }

  return Some(AndroidFont(font));
}

AndroidFont::~AndroidFont() {
  if (NS_WARN_IF(!sFont_close)) {
    return;
  }

  if (!mFont) {
    return;
  }

  sFont_close(mFont);
}

const char* AndroidFont::GetFontFilePath() {
  if (NS_WARN_IF(!sFont_getFontFilePath) || NS_WARN_IF(!mFont)) {
    return nullptr;
  }

  return sFont_getFontFilePath(mFont);
}

}  // namespace mozilla
