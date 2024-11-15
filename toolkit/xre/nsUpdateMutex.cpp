/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUpdateMutex.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticMutex.h"
#include "nsIFile.h"
#include "nsProfileLock.h"
#include "nsXULAppAPI.h"

mozilla::StaticMutex UpdateMutexImpl::sInProcessMutex;

bool UpdateMutexImpl::TryLock() {
  if (!sInProcessMutex.TryLock()) {
    return false;
  }

  bool success = [&crossProcessLock = mCrossProcessLock]() {
    nsCOMPtr<nsIFile> updRoot;
    nsresult nsrv =
        NS_GetSpecialDirectory(XRE_UPDATE_ROOT_DIR, getter_AddRefs(updRoot));
    if (NS_FAILED(nsrv)) {
      return false;
    }

    nsrv = updRoot->Create(nsIFile::DIRECTORY_TYPE, 0755);
    if (NS_FAILED(nsrv) && nsrv != NS_ERROR_FILE_ALREADY_EXISTS) {
      return false;
    }

    return NS_SUCCEEDED(crossProcessLock.Lock(updRoot, nullptr));
  }();

  if (!success) {
    sInProcessMutex.Unlock();
  }

  return success;
}

void UpdateMutexImpl::Unlock() {
  sInProcessMutex.AssertCurrentThreadOwns();

  mozilla::DebugOnly<nsresult> nsrv = mCrossProcessLock.Unlock();
#ifdef DEBUG
  if (!NS_SUCCEEDED(nsrv)) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "failed to unlock the update mutex's nsProfileLock -- got 0x%08x",
        static_cast<uint32_t>(nsrv.inspect()));
  }
#endif

  sInProcessMutex.Unlock();
}

NS_IMPL_ISUPPORTS(nsUpdateMutex, nsIUpdateMutex)

NS_IMETHODIMP nsUpdateMutex::IsLocked(bool* aResult) {
  *aResult = mIsLocked;
  return NS_OK;
}

NS_IMETHODIMP nsUpdateMutex::TryLock(bool* aResult) {
  if (!mIsLocked) {
    mIsLocked = mUpdateMutexImpl.TryLock();
  }
  *aResult = mIsLocked;
  return NS_OK;
}

NS_IMETHODIMP nsUpdateMutex::Unlock() MOZ_NO_THREAD_SAFETY_ANALYSIS {
  // Thread safety analysis cannot make sense out of a conditional release
  if (mIsLocked) {
    mUpdateMutexImpl.Unlock();
    mIsLocked = false;
  }
  return NS_OK;
}
