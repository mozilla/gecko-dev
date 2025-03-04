/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/GeckoArgs.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsPrintfCString.h"

#include "XPCSelfHostedShmem.h"

namespace mozilla {
namespace ipc {

SharedPreferenceSerializer::SharedPreferenceSerializer() {
  MOZ_COUNT_CTOR(SharedPreferenceSerializer);
}

SharedPreferenceSerializer::~SharedPreferenceSerializer() {
  MOZ_COUNT_DTOR(SharedPreferenceSerializer);
}

SharedPreferenceSerializer::SharedPreferenceSerializer(
    SharedPreferenceSerializer&& aOther)
    : mPrefMapHandle(std::move(aOther.mPrefMapHandle)),
      mPrefsHandle(std::move(aOther.mPrefsHandle)) {
  MOZ_COUNT_CTOR(SharedPreferenceSerializer);
}

bool SharedPreferenceSerializer::SerializeToSharedMemory(
    const GeckoProcessType aDestinationProcessType,
    const nsACString& aDestinationRemoteType) {
  mPrefMapHandle = Preferences::EnsureSnapshot();

  bool destIsWebContent =
      aDestinationProcessType == GeckoProcessType_Content &&
      (StringBeginsWith(aDestinationRemoteType, WEB_REMOTE_TYPE) ||
       StringBeginsWith(aDestinationRemoteType, PREALLOC_REMOTE_TYPE));

  // Serialize the early prefs.
  nsAutoCStringN<1024> prefs;
  Preferences::SerializePreferences(prefs, destIsWebContent);
  auto prefsLength = prefs.Length();

  // Set up the shared memory.
  auto handle = shared_memory::Create(prefsLength);
  if (!handle) {
    NS_ERROR("failed to create shared memory in the parent");
    return false;
  }
  auto mapping = handle.Map();
  if (!mapping) {
    NS_ERROR("failed to map shared memory in the parent");
    return false;
  }

  // Copy the serialized prefs into the shared memory.
  memcpy(mapping.DataAs<char>(), prefs.get(), prefsLength);

  mPrefsHandle = std::move(handle).ToReadOnly();
  return true;
}

void SharedPreferenceSerializer::AddSharedPrefCmdLineArgs(
    mozilla::ipc::GeckoChildProcessHost& procHost,
    geckoargs::ChildProcessArgs& aExtraOpts) const {
  auto prefsHandle = GetPrefsHandle().Clone();
  MOZ_RELEASE_ASSERT(prefsHandle, "failed to clone prefs handle");
  auto prefMapHandle = GetPrefMapHandle().Clone();
  MOZ_RELEASE_ASSERT(prefMapHandle, "failed to clone pref map handle");

  // Pass the handles via command line flags.
  geckoargs::sPrefsHandle.Put(std::move(prefsHandle), aExtraOpts);
  geckoargs::sPrefMapHandle.Put(std::move(prefMapHandle), aExtraOpts);
}

SharedPreferenceDeserializer::SharedPreferenceDeserializer() {
  MOZ_COUNT_CTOR(SharedPreferenceDeserializer);
}

SharedPreferenceDeserializer::~SharedPreferenceDeserializer() {
  MOZ_COUNT_DTOR(SharedPreferenceDeserializer);
}

bool SharedPreferenceDeserializer::DeserializeFromSharedMemory(
    ReadOnlySharedMemoryHandle&& aPrefsHandle,
    ReadOnlySharedMemoryHandle&& aPrefMapHandle) {
  if (!aPrefsHandle || !aPrefMapHandle) {
    return false;
  }

  mPrefMapHandle = std::move(aPrefMapHandle);

  // Init the shared-memory base preference mapping first, so that only changed
  // preferences wind up in heap memory.
  Preferences::InitSnapshot(mPrefMapHandle);

  // Set up early prefs from the shared memory.
  mShmem = aPrefsHandle.Map();
  if (!mShmem) {
    NS_ERROR("failed to map shared memory in the child");
    return false;
  }
  Preferences::DeserializePreferences(mShmem.DataAs<char>(), mShmem.Size());

  return true;
}

void ExportSharedJSInit(mozilla::ipc::GeckoChildProcessHost& procHost,
                        geckoargs::ChildProcessArgs& aExtraOpts) {
  auto& shmem = xpc::SelfHostedShmem::GetSingleton();
  auto handle = shmem.Handle().Clone();

  // If the file is not found or the content is empty, then we would start the
  // content process without this optimization.
  if (!handle) {
    NS_ERROR("Can't use SelfHosted shared memory handle.");
    return;
  }

  // command line: -jsInitHandle handle
  geckoargs::sJsInitHandle.Put(std::move(handle), aExtraOpts);
}

bool ImportSharedJSInit(ReadOnlySharedMemoryHandle&& aJsInitHandle) {
  // This is an optimization, and as such we can safely recover if the command
  // line argument are not provided.
  if (!aJsInitHandle) {
    return true;
  }

  // Initialize the shared memory with the file handle and size of the content
  // of the self-hosted Xdr.
  auto& shmem = xpc::SelfHostedShmem::GetSingleton();
  if (!shmem.InitFromChild(std::move(aJsInitHandle))) {
    NS_ERROR("failed to open shared memory in the child");
    return false;
  }

  return true;
}

}  // namespace ipc
}  // namespace mozilla
