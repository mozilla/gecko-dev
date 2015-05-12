/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_cloudstorage_h_
#define mozilla_system_cloudstorage_h_

#include "nsString.h"
#include "nsIThread.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace system {
namespace cloudstorage {

class CloudStorage
{
public:
//  NS_INLINE_DECL_REFCOUNTING(CloudStorage)
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CloudStorage)

  CloudStorage(const nsCString& aCloudStorageName);

public:
  enum eState {
    STATE_READY = 0,
    STATE_RUNNING = 1
  };

  const nsCString& Name() { return mCloudStorageName; }
  const char* NameStr() { return mCloudStorageName.get(); }
  const nsCString& MountPoint() { return mMountPoint; }
  const char* MountPointStr() { return mMountPoint.get(); }
  static const char* StateStr(const CloudStorage::eState& aState);
  const CloudStorage::eState& State() { return mState; }
  const char* StateStr() { return StateStr(mState); }

  void StartStorage();
  void StopStorage();

private:
  nsCString mCloudStorageName;
  nsCString mMountPoint;
  eState    mState;
  nsCOMPtr<nsIThread> mRunnableThread;
};

} // end namespace cloudstorage
} // end namespace system
} // end namespace mozilla
#endif
