/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_WEBRTCLOCATION_H_
#define DOM_MEDIA_WEBRTC_LIBWEBRTCGLUE_WEBRTCLOCATION_H_

// See bug 1973646 for moving this file to use std::source_location.

#ifdef __has_builtin
#  define MOZ_HAS_BUILTIN(bi) __has_builtin(bi)
#else
// Note: For toolchains without __has_builtin, we define MOZ_HAS_BUILTIN to
// return true on modern-enough MSVC, and otherwise false.
#  define MOZ_HAS_BUILTIN(bi) _MSC_VER >= 1926
#endif

#if MOZ_HAS_BUILTIN(__builtin_FUNCTION)
#  define MOZ_BUILTIN_FUNCTION __builtin_FUNCTION
#else
#  define MOZ_BUILTIN_FUNCTION() nullptr
#endif

#if MOZ_HAS_BUILTIN(__builtin_FILE)
#  define MOZ_BUILTIN_FILE __builtin_FILE
#else
#  define MOZ_BUILTIN_FILE() nullptr
#endif

#if MOZ_HAS_BUILTIN(__builtin_LINE)
#  define MOZ_BUILTIN_LINE __builtin_LINE
#else
#  define MOZ_BUILTIN_LINE() 0
#endif

namespace mozilla {
class WebrtcLocation {
 private:
  WebrtcLocation(const char* aFunction, const char* aFile, int aLine)
      : mFunction(aFunction), mFile(aFile), mLine(aLine) {}

 public:
  static WebrtcLocation Current(const char* aFunction = MOZ_BUILTIN_FUNCTION(),
                                const char* aFile = MOZ_BUILTIN_FILE(),
                                int aLine = MOZ_BUILTIN_LINE()) {
    return WebrtcLocation(aFunction, aFile, aLine);
  }

  const char* const mFunction;
  const char* const mFile;
  const int mLine;
};

}  // namespace mozilla

#undef MOZ_BUILTIN_FUNCTION
#undef MOZ_BUILTIN_FILE
#undef MOZ_BUILTIN_LINE
#undef MOZ_HAS_BUILTIN

#endif
