/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/GeckoArgs.h"

namespace mozilla::geckoargs {

template <>
Maybe<UniqueFileHandle> CommandLineArg<UniqueFileHandle>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  if (Maybe<uint32_t> arg =
          CommandLineArg<uint32_t>::GetCommon(aMatch, aArgc, aArgv, aFlags)) {
#ifdef XP_WIN
    // Recover the pointer-sized HANDLE from the 32-bit argument received over
    // IPC by sign-extending to the full pointer width. See `PutCommon` for an
    // explanation.
    return Some(UniqueFileHandle{reinterpret_cast<HANDLE>(
        static_cast<uintptr_t>(static_cast<int32_t>(*arg)))});
#else
    MOZ_CRASH("not implemented yet");
#endif
  }
  return Nothing();
}

template <>
void CommandLineArg<UniqueFileHandle>::PutCommon(const char* aName,
                                                 UniqueFileHandle aValue,
                                                 ChildProcessArgs& aArgs) {
  if (aValue) {
#ifdef XP_WIN
    // On Windows, we'll inherit the handle by-identity, so pass down the
    // HANDLE's value. Handles are always 32-bits (potentially sign-extended),
    // so we explicitly truncate them before sending over IPC.
    HANDLE value = aValue.get();
    CommandLineArg<uint32_t>::PutCommon(
        aName, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value)),
        aArgs);
#else
    MOZ_CRASH("not implemented yet");
#endif
    aArgs.mFiles.push_back(std::move(aValue));
  }
}

}  // namespace mozilla::geckoargs
