/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "mozilla/GeckoArgs.h"

namespace mozilla::geckoargs {

#ifdef XP_UNIX
// Table of file handles which have been passed from another process.
// The default mapping is hard-coded here, but can be overridden for platforms
// where that is necessary.
//
// NOTE: If we ever need to inherit more than 15 handles during process
// creation, we will need to extend this static array by adding more unique
// entries.
static int gInitialFileHandles[]{3,  4,  5,  6,  7,  8,  9, 10,
                                 11, 12, 13, 14, 15, 16, 17};

void SetPassedFileHandles(Span<int> aFiles) {
  MOZ_RELEASE_ASSERT(aFiles.Length() <= ArrayLength(gInitialFileHandles));
  for (size_t i = 0; i < ArrayLength(gInitialFileHandles); ++i) {
    if (i < aFiles.Length()) {
      gInitialFileHandles[i] = aFiles[i];
    } else {
      gInitialFileHandles[i] = -1;
    }
  }
}

void SetPassedFileHandles(std::vector<UniqueFileHandle>&& aFiles) {
  MOZ_RELEASE_ASSERT(aFiles.size() <= ArrayLength(gInitialFileHandles));
  for (size_t i = 0; i < ArrayLength(gInitialFileHandles); ++i) {
    if (i < aFiles.size()) {
      gInitialFileHandles[i] = aFiles[i].release();
    } else {
      gInitialFileHandles[i] = -1;
    }
  }
}

void AddToFdsToRemap(const ChildProcessArgs& aArgs,
                     std::vector<std::pair<int, int>>& aFdsToRemap) {
  MOZ_RELEASE_ASSERT(aArgs.mFiles.size() <= ArrayLength(gInitialFileHandles));
  for (size_t i = 0; i < aArgs.mFiles.size(); ++i) {
    aFdsToRemap.push_back(
        std::pair{aArgs.mFiles[i].get(), gInitialFileHandles[i]});
  }
}
#endif

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
    // See the comment on gInitialFileHandles for an explanation of the
    // behaviour here.
    MOZ_RELEASE_ASSERT(*arg < ArrayLength(gInitialFileHandles));
    return Some(UniqueFileHandle{std::exchange(gInitialFileHandles[*arg], -1)});
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
    CommandLineArg<uint32_t>::PutCommon(
        aName, static_cast<uint32_t>(aArgs.mFiles.size()), aArgs);
#endif
    aArgs.mFiles.push_back(std::move(aValue));
  }
}

}  // namespace mozilla::geckoargs
