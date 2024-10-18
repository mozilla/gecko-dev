/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFmtCString_h___
#define nsFmtCString_h___

#include <type_traits>
#include "fmt/format.h"
#include "fmt/xchar.h"
#include "nsString.h"

/**
 * nsTFmtString lets you create a nsTString using a C++20-style format
 * string.  For example:
 *
 *   NS_WARNING(nsFmtCString(FMT_STRING("Unexpected value: {}"), 13.917).get());
 *   NS_WARNING(nsFmtString(FMT_STRING(u"Unexpected value: {}"),
 *                                     u"weird").get());
 *
 * nsTFmtString has a small built-in auto-buffer.  For larger strings, it
 * will allocate on the heap.
 *
 * See also nsTSubstring::AppendFmt().
 */
template <typename T>
class nsTFmtString : public nsTAutoStringN<T, 16> {
 public:
  template <typename... Args>
  explicit nsTFmtString(
      fmt::basic_format_string<T, type_identity_t<Args>...> aFormatStr,
      Args&&... aArgs) {
    this->AppendFmt(aFormatStr, std::forward<Args>(aArgs)...);
  }
};

template <typename Char>
struct fmt::formatter<nsTFmtString<Char>, Char>
    : fmt::formatter<nsTString<Char>, Char> {};

#endif  // !defined(nsFmtString_h___)
