/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidSystemFontIterator.h"

#include "mozilla/Assertions.h"
#include "nsDebug.h"

#include <dlfcn.h>

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
    void* handle = dlopen("libandroid.so", RTLD_LAZY | RTLD_LOCAL);
    MOZ_ASSERT(handle);

    sSystemFontIterator_open =
        (_ASystemFontIterator_open)dlsym(handle, "ASystemFontIterator_open");
    sSystemFontIterator_next =
        (_ASystemFontIterator_next)dlsym(handle, "ASystemFontIterator_next");
    sSystemFontIterator_close =
        (_ASystemFontIterator_close)dlsym(handle, "ASystemFontIterator_close");
    AndroidFont::sFont_getFontFilePath =
        (_AFont_getFontFilePath)dlsym(handle, "AFont_getFontFilePath");
    AndroidFont::sFont_close = (_AFont_close)dlsym(handle, "AFont_close");

    if (NS_WARN_IF(!sSystemFontIterator_open) ||
        NS_WARN_IF(!sSystemFontIterator_next) ||
        NS_WARN_IF(!sSystemFontIterator_close) ||
        NS_WARN_IF(!AndroidFont::sFont_getFontFilePath) ||
        NS_WARN_IF(!AndroidFont::sFont_close)) {
      sSystemFontIterator_open = nullptr;
      return false;
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

  void* font = sSystemFontIterator_next(mIterator);
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
