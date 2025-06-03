/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidSystemFontIterator.h"

#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"
#include "nsDebug.h"

namespace mozilla {

AndroidSystemFontIterator::~AndroidSystemFontIterator() {
  ASystemFontIterator_close(mIterator);
}

AndroidSystemFontIterator::AndroidSystemFontIterator()
    : mIterator(ASystemFontIterator_open()) {}

void AndroidSystemFontIterator::Preload() {
  // Trigger first system font creation to fill system cache.
  AndroidSystemFontIterator iterator;
  Unused << iterator;
}

Maybe<AndroidFont> AndroidSystemFontIterator::Next() {
  if (mIterator) {
    if (AFont* font = ASystemFontIterator_next(mIterator)) {
      return Some(AndroidFont(font));
    } else {
      ASystemFontIterator_close(mIterator);
      mIterator = nullptr;
      return Nothing();
    }
  }
  return Nothing();
}

AndroidFont::~AndroidFont() { AFont_close(mFont); }

const char* AndroidFont::GetFontFilePath() {
  if (mFont) {
    return AFont_getFontFilePath(mFont);
  }
  return nullptr;
}

}  // namespace mozilla
