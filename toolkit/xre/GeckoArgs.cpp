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
  MOZ_RELEASE_ASSERT(aFiles.Length() <= std::size(gInitialFileHandles));
  for (size_t i = 0; i < std::size(gInitialFileHandles); ++i) {
    if (i < aFiles.Length()) {
      gInitialFileHandles[i] = aFiles[i];
    } else {
      gInitialFileHandles[i] = -1;
    }
  }
}

void SetPassedFileHandles(std::vector<UniqueFileHandle>&& aFiles) {
  MOZ_RELEASE_ASSERT(aFiles.size() <= std::size(gInitialFileHandles));
  for (size_t i = 0; i < std::size(gInitialFileHandles); ++i) {
    if (i < aFiles.size()) {
      gInitialFileHandles[i] = aFiles[i].release();
    } else {
      gInitialFileHandles[i] = -1;
    }
  }
}

void AddToFdsToRemap(const ChildProcessArgs& aArgs,
                     std::vector<std::pair<int, int>>& aFdsToRemap) {
  MOZ_RELEASE_ASSERT(aArgs.mFiles.size() <= std::size(gInitialFileHandles));
  for (size_t i = 0; i < aArgs.mFiles.size(); ++i) {
    aFdsToRemap.push_back(
        std::pair{aArgs.mFiles[i].get(), gInitialFileHandles[i]});
  }
}
#endif

#ifdef XP_DARWIN
// Table of mach send rights which have been sent by the parent process.
static mach_port_t gMachSendRights[kMaxPassedMachSendRights] = {MACH_PORT_NULL};

void SetPassedMachSendRights(std::vector<UniqueMachSendRight>&& aSendRights) {
  MOZ_RELEASE_ASSERT(aSendRights.size() <= std::size(gMachSendRights));
  for (size_t i = 0; i < aSendRights.size(); ++i) {
    gMachSendRights[i] = aSendRights[i].release();
  }
}
#endif

static void ParseHandleArgument(uint32_t aArg, UniqueFileHandle& aOutHandle) {
#ifdef XP_WIN
  // Recover the pointer-sized HANDLE from the 32-bit argument received over IPC
  // by sign-extending to the full pointer width. See `SerializeHandleArgument`
  // for an explanation.
  aOutHandle = UniqueFileHandle{reinterpret_cast<HANDLE>(
      static_cast<uintptr_t>(static_cast<int32_t>(aArg)))};
#else
  // See the comment on gInitialFileHandles for an explanation of the
  // behaviour here.
  MOZ_RELEASE_ASSERT(aArg < std::size(gInitialFileHandles));
  aOutHandle = UniqueFileHandle{std::exchange(gInitialFileHandles[aArg], -1)};
#endif
}

static Maybe<uint32_t> SerializeHandleArgument(UniqueFileHandle&& aValue,
                                               ChildProcessArgs& aArgs) {
  if (aValue) {
#ifdef XP_WIN
    // On Windows, we'll inherit the handle by-identity, so pass down the
    // HANDLE's value. Handles are always 32-bits (potentially sign-extended),
    // so we explicitly truncate them before sending over IPC.
    HANDLE value = aValue.get();
    uint32_t arg = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value));
#else
    uint32_t arg = static_cast<uint32_t>(aArgs.mFiles.size());
#endif
    aArgs.mFiles.push_back(std::move(aValue));
    return Some(arg);
  }
  return Nothing();
}

template <>
Maybe<UniqueFileHandle> CommandLineArg<UniqueFileHandle>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  if (Maybe<uint32_t> arg =
          CommandLineArg<uint32_t>::GetCommon(aMatch, aArgc, aArgv, aFlags)) {
    UniqueFileHandle h;
    ParseHandleArgument(*arg, h);
    return Some(std::move(h));
  }
  return Nothing();
}

template <>
void CommandLineArg<UniqueFileHandle>::PutCommon(const char* aName,
                                                 UniqueFileHandle aValue,
                                                 ChildProcessArgs& aArgs) {
  if (auto arg = SerializeHandleArgument(std::move(aValue), aArgs)) {
    CommandLineArg<uint32_t>::PutCommon(aName, *arg, aArgs);
  }
}

#ifdef XP_DARWIN
static void ParseHandleArgument(uint32_t aArg,
                                UniqueMachSendRight& aOutHandle) {
  MOZ_RELEASE_ASSERT(aArg < std::size(gMachSendRights));
  aOutHandle =
      UniqueMachSendRight{std::exchange(gMachSendRights[aArg], MACH_PORT_NULL)};
}

static Maybe<uint32_t> SerializeHandleArgument(UniqueMachSendRight&& aValue,
                                               ChildProcessArgs& aArgs) {
  if (aValue) {
    aArgs.mSendRights.push_back(std::move(aValue));
    return Some(static_cast<uint32_t>(aArgs.mSendRights.size() - 1));
  }
  return Nothing();
}

template <>
Maybe<UniqueMachSendRight> CommandLineArg<UniqueMachSendRight>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  if (Maybe<uint32_t> arg =
          CommandLineArg<uint32_t>::GetCommon(aMatch, aArgc, aArgv, aFlags)) {
    UniqueMachSendRight h;
    ParseHandleArgument(*arg, h);
    return Some(std::move(h));
  }
  return Nothing();
}

template <>
void CommandLineArg<UniqueMachSendRight>::PutCommon(const char* aName,
                                                    UniqueMachSendRight aValue,
                                                    ChildProcessArgs& aArgs) {
  if (auto arg = SerializeHandleArgument(std::move(aValue), aArgs)) {
    CommandLineArg<uint32_t>::PutCommon(aName, *arg, aArgs);
  }
}
#endif

// Shared memory handles are passed as a (handle, size) pair, which both turn
// into numeric CLI arguments, so it's safe to use ":" as a separator.
constexpr const char* kSharedMemoryHandleSeparator = ":";

template <>
Maybe<ipc::ReadOnlySharedMemoryHandle>
CommandLineArg<ipc::ReadOnlySharedMemoryHandle>::GetCommon(
    const char* aMatch, int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  auto arg =
      CommandLineArg<const char*>::GetCommon(aMatch, aArgc, aArgv, aFlags);
  if (!arg) {
    return Nothing();
  }

  std::string_view str = *arg;
  auto position = str.find(kSharedMemoryHandleSeparator);
  if (position == std::string_view::npos) {
    return Nothing();
  }

  auto handleId = ParseIntArgument(str.substr(0, position));
  auto size = ParseIntArgument(str.substr(position + 1));
  if (!handleId || !size) {
    return Nothing();
  }

  ipc::shared_memory::PlatformHandle handle;
  ParseHandleArgument(*handleId, handle);
  if (!handle) {
    return Nothing();
  }

  mozilla::ipc::ReadOnlySharedMemoryHandle rv;
  rv.mHandle = std::move(handle);
  rv.SetSize(*size);

  return Some(std::move(rv));
}

template <>
void CommandLineArg<ipc::ReadOnlySharedMemoryHandle>::PutCommon(
    const char* aName, ipc::ReadOnlySharedMemoryHandle aValue,
    ChildProcessArgs& aArgs) {
  if (!aValue) {
    return;
  }
  auto size = aValue.Size();
  auto handle = std::move(aValue).TakePlatformHandle();
  MOZ_ASSERT(handle, "shmem platform handle is invalid");

  auto handleId = SerializeHandleArgument(std::move(handle), aArgs);
  if (!handleId) {
    return;
  }

  auto arg = std::to_string(*handleId) + kSharedMemoryHandleSeparator +
             std::to_string(size);

  CommandLineArg<const char*>::PutCommon(aName, arg.c_str(), aArgs);
}

}  // namespace mozilla::geckoargs
