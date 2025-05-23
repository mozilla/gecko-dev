/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* an identifier for User Agent style sheets */

#ifndef mozilla_BuiltInStyleSheets_h
#define mozilla_BuiltInStyleSheets_h

#include <stdint.h>
#include "mozilla/TypedEnumBits.h"

namespace mozilla {

enum class BuiltInStyleSheetFlags : uint8_t {
  UA = 1,
  Author = 1 << 1,
  // By default sheets are shared, except xul.css which we only need in the
  // parent process.
  NotShared = 1 << 2,

  UAUnshared = (UA | NotShared),
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(BuiltInStyleSheetFlags);

enum class BuiltInStyleSheet : uint8_t {
#define STYLE_SHEET(identifier_, url_, flags_) identifier_,
#include "mozilla/BuiltInStyleSheetList.h"
#undef STYLE_SHEET
  Count
};

}  // namespace mozilla

#endif  // mozilla_BuiltInStyleSheets_h
