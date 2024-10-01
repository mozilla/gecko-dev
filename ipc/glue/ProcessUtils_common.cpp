/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/GeckoArgs.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsPrintfCString.h"

#include "XPCSelfHostedShmem.h"

namespace mozilla {
namespace ipc {

SharedPreferenceSerializer::SharedPreferenceSerializer()
    : mPrefMapSize(0), mPrefsLength(0) {
  MOZ_COUNT_CTOR(SharedPreferenceSerializer);
}

SharedPreferenceSerializer::~SharedPreferenceSerializer() {
  MOZ_COUNT_DTOR(SharedPreferenceSerializer);
}

SharedPreferenceSerializer::SharedPreferenceSerializer(
    SharedPreferenceSerializer&& aOther)
    : mPrefMapSize(aOther.mPrefMapSize),
      mPrefsLength(aOther.mPrefsLength),
      mPrefMapHandle(std::move(aOther.mPrefMapHandle)),
      mPrefsHandle(std::move(aOther.mPrefsHandle)) {
  MOZ_COUNT_CTOR(SharedPreferenceSerializer);
}

bool SharedPreferenceSerializer::SerializeToSharedMemory(
    const GeckoProcessType aDestinationProcessType,
    const nsACString& aDestinationRemoteType) {
  mPrefMapHandle =
      Preferences::EnsureSnapshot(&mPrefMapSize).TakePlatformHandle();

  bool destIsWebContent =
      aDestinationProcessType == GeckoProcessType_Content &&
      (StringBeginsWith(aDestinationRemoteType, WEB_REMOTE_TYPE) ||
       StringBeginsWith(aDestinationRemoteType, PREALLOC_REMOTE_TYPE));

  // Serialize the early prefs.
  nsAutoCStringN<1024> prefs;
  Preferences::SerializePreferences(prefs, destIsWebContent);
  mPrefsLength = prefs.Length();

  base::SharedMemory shm;
  // Set up the shared memory.
  if (!shm.Create(prefs.Length())) {
    NS_ERROR("failed to create shared memory in the parent");
    return false;
  }
  if (!shm.Map(prefs.Length())) {
    NS_ERROR("failed to map shared memory in the parent");
    return false;
  }

  // Copy the serialized prefs into the shared memory.
  memcpy(static_cast<char*>(shm.memory()), prefs.get(), mPrefsLength);

  mPrefsHandle = shm.TakeHandle();
  return true;
}

void SharedPreferenceSerializer::AddSharedPrefCmdLineArgs(
    mozilla::ipc::GeckoChildProcessHost& procHost,
    geckoargs::ChildProcessArgs& aExtraOpts) const {
  UniqueFileHandle prefsHandle = DuplicateFileHandle(GetPrefsHandle());
  MOZ_RELEASE_ASSERT(prefsHandle, "failed to duplicate prefs handle");
  UniqueFileHandle prefMapHandle = DuplicateFileHandle(GetPrefMapHandle());
  MOZ_RELEASE_ASSERT(prefMapHandle, "failed to duplicate pref map handle");

  // Pass the handles and lengths via command line flags.
  geckoargs::sPrefsHandle.Put(std::move(prefsHandle), aExtraOpts);
  geckoargs::sPrefsLen.Put((uintptr_t)(GetPrefsLength()), aExtraOpts);
  geckoargs::sPrefMapHandle.Put(std::move(prefMapHandle), aExtraOpts);
  geckoargs::sPrefMapSize.Put((uintptr_t)(GetPrefMapSize()), aExtraOpts);
}

SharedPreferenceDeserializer::SharedPreferenceDeserializer() {
  MOZ_COUNT_CTOR(SharedPreferenceDeserializer);
}

SharedPreferenceDeserializer::~SharedPreferenceDeserializer() {
  MOZ_COUNT_DTOR(SharedPreferenceDeserializer);
}

bool SharedPreferenceDeserializer::DeserializeFromSharedMemory(
    UniqueFileHandle aPrefsHandle, UniqueFileHandle aPrefMapHandle,
    uint64_t aPrefsLen, uint64_t aPrefMapSize) {
  if (!aPrefsHandle || !aPrefMapHandle || !aPrefsLen || !aPrefMapSize) {
    return false;
  }

  mPrefMapHandle.emplace(std::move(aPrefMapHandle));

  mPrefsLen = Some((uintptr_t)(aPrefsLen));

  mPrefMapSize = Some((uintptr_t)(aPrefMapSize));

  // Init the shared-memory base preference mapping first, so that only changed
  // preferences wind up in heap memory.
  Preferences::InitSnapshot(mPrefMapHandle.ref(), *mPrefMapSize);

  // Set up early prefs from the shared memory.
  if (!mShmem.SetHandle(std::move(aPrefsHandle), /* read_only */ true)) {
    NS_ERROR("failed to open shared memory in the child");
    return false;
  }
  if (!mShmem.Map(*mPrefsLen)) {
    NS_ERROR("failed to map shared memory in the child");
    return false;
  }
  Preferences::DeserializePreferences(static_cast<char*>(mShmem.memory()),
                                      *mPrefsLen);

  return true;
}

const FileDescriptor& SharedPreferenceDeserializer::GetPrefMapHandle() const {
  MOZ_ASSERT(mPrefMapHandle.isSome());

  return mPrefMapHandle.ref();
}

void ExportSharedJSInit(mozilla::ipc::GeckoChildProcessHost& procHost,
                        geckoargs::ChildProcessArgs& aExtraOpts) {
#if defined(ANDROID) || defined(XP_IOS)
  // The code to support Android/iOS is added in a follow-up patch.
  return;
#else
  auto& shmem = xpc::SelfHostedShmem::GetSingleton();
  UniqueFileHandle handle = DuplicateFileHandle(shmem.Handle());
  size_t len = shmem.Content().Length();

  // If the file is not found or the content is empty, then we would start the
  // content process without this optimization.
  if (!handle || !len) {
    return;
  }

  // command line: -jsInitHandle handle -jsInitLen length
  geckoargs::sJsInitHandle.Put(std::move(handle), aExtraOpts);
  geckoargs::sJsInitLen.Put((uintptr_t)(len), aExtraOpts);
#endif
}

bool ImportSharedJSInit(UniqueFileHandle aJsInitHandle, uint64_t aJsInitLen) {
  // This is an optimization, and as such we can safely recover if the command
  // line argument are not provided.
  if (!aJsInitLen || !aJsInitHandle) {
    return true;
  }

  size_t len = (uintptr_t)(aJsInitLen);
  if (!len) {
    return false;
  }

  // Initialize the shared memory with the file handle and size of the content
  // of the self-hosted Xdr.
  auto& shmem = xpc::SelfHostedShmem::GetSingleton();
  if (!shmem.InitFromChild(std::move(aJsInitHandle), len)) {
    NS_ERROR("failed to open shared memory in the child");
    return false;
  }

  return true;
}

}  // namespace ipc
}  // namespace mozilla
