/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TOOLKIT_COMPONENTS_REMOTE_REMOTEUTILS_H_
#define TOOLKIT_COMPONENTS_REMOTE_REMOTEUTILS_H_

#include "mozilla/HashFunctions.h"

#include "nsString.h"
#if defined XP_WIN
#  include "WinUtils.h"
#endif

#if defined XP_WIN || defined XP_MACOSX
static void BuildClassName(const char* aProgram, const char* aProfile,
                           nsString& aClassName) {
  // On Windows, the class name is used as the window class.
  // The window class name is limited to 256 characters, and it fails when
  // exceeds.
  //
  // https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-wndclassa
  //
  // On macOS, the class name is used as the name of message ports.
  // The message port's name is limited to 128 characters, and the characters
  // beyond the length is simply ignored.
  //
  // https://github.com/opensource-apple/CF/blob/3cc41a76b1491f50813e28a4ec09954ffa359e6f/CFMessagePort.c#L53

#  if defined XP_WIN
  constexpr size_t ClassNameMaxLength = 256;
#  else
  constexpr size_t ClassNameMaxLength = 128;
#  endif

  aClassName.AppendPrintf("Mozilla_%s", aProgram);
#  if defined XP_WIN
  nsString pfn = mozilla::widget::WinUtils::GetPackageFamilyName();
  if (!pfn.IsEmpty()) {
    aClassName.AppendPrintf("_%S", pfn.getW());
  }
#  endif
  aClassName.AppendPrintf("_%s_RemoteWindow", aProfile);

  if (aClassName.Length() > ClassNameMaxLength) {
    mozilla::HashNumber hash = mozilla::HashString(aClassName.get());
    aClassName.Truncate();
    aClassName.AppendPrintf("Mozilla_%08x_RemoteWindow", hash);
  }
}
#endif

char* ConstructCommandLine(int32_t argc, const char** argv,
                           const char* aStartupToken, int* aCommandLineLength);

#endif  // TOOLKIT_COMPONENTS_REMOTE_REMOTEUTILS_H_
