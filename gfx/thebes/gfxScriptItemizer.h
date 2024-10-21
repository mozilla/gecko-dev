/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file is based on usc_impl.c from ICU 4.2.0.1, slightly adapted
 * for use within Mozilla Gecko, separate from a standard ICU build.
 *
 * The original ICU license of the code follows:
 *
 * ICU License - ICU 1.8.1 and later
 *
 * COPYRIGHT AND PERMISSION NOTICE
 *
 * Copyright (c) 1995-2009 International Business Machines Corporation and
 * others
 *
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, provided that the above copyright notice(s) and this
 * permission notice appear in all copies of the Software and that both the
 * above copyright notice(s) and this permission notice appear in supporting
 * documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
 * BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization of the
 * copyright holder.
 *
 * All trademarks and registered trademarks mentioned herein are the property
 * of their respective owners.
 */

#ifndef GFX_SCRIPTITEMIZER_H
#define GFX_SCRIPTITEMIZER_H

#include <stdint.h>
#include "mozilla/Attributes.h"
#include "mozilla/intl/UnicodeScriptCodes.h"

#define PAREN_STACK_DEPTH 32

class gfxScriptItemizer {
 public:
  using Script = mozilla::intl::Script;

  gfxScriptItemizer() = default;
  gfxScriptItemizer(const gfxScriptItemizer& aOther) = delete;
  gfxScriptItemizer(gfxScriptItemizer&& aOther) = delete;

  void SetText(const char16_t* aText, uint32_t aLength) {
    textPtr._2b = aText;
    textLength = aLength;
    textIs8bit = false;
  }

  void SetText(const unsigned char* aText, uint32_t aLength) {
    textPtr._1b = aText;
    textLength = aLength;
    textIs8bit = true;
  }

  struct Run {
    uint32_t mOffset = 0;
    uint32_t mLength = 0;
    Script mScript = Script::COMMON;

    MOZ_IMPLICIT operator bool() const { return mLength > 0; }
  };

  Run Next();

 protected:
  void push(uint32_t endPairChar, Script newScriptCode);
  void pop();
  void fixup(Script newScriptCode);

  struct ParenStackEntry {
    uint32_t endPairChar;
    Script scriptCode;
  };

  union {
    const char16_t* _2b;
    const unsigned char* _1b;
  } textPtr;
  uint32_t textLength;
  bool textIs8bit;

  uint32_t scriptStart = 0;
  uint32_t scriptLimit = 0;
  Script scriptCode = Script::INVALID;

  struct ParenStackEntry parenStack[PAREN_STACK_DEPTH];
  uint32_t parenSP = -1;
  uint32_t pushCount = 0;
  uint32_t fixupCount = 0;
};

#endif /* GFX_SCRIPTITEMIZER_H */
