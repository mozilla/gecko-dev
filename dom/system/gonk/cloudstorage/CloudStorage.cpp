/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorage.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "CloudStorageLog.h"
#include "CloudStorageRequestHandler.h"
#include "Volume.h"
#include "VolumeManager.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace system {
namespace cloudstorage {

class CloudStorageRunnable : public nsRunnable
{
public:
  CloudStorageRunnable(CloudStorage* aCloudStorage)
    : mCloudStorage(aCloudStorage)
  {
  }

  nsresult Run()
  { 
    CloudStorageRequestHandler* handler = new CloudStorageRequestHandler(mCloudStorage->MountPoint());
    if (handler) {
      /*
      RefPtr<Volume> vol = VolumeManager::FindAddVolumeByName(mCloudStorage->Name());
      vol->SetCloudVolume(mCloudStorage->MountPoint());
      */
      while (mCloudStorage->State() == CloudStorage::STATE_RUNNING) {
        //handler->HandleOneRequest();
	sleep(1);
      }
    } else {
      LOG("Construct cloud storage handler fail");
    }
    delete handler;
    LOG("going to finish RequestHandler.");
    return NS_OK;
  }

private:
  CloudStorage* mCloudStorage;
};

CloudStorage::CloudStorage(const nsCString& aCloudStorageName)
  : mCloudStorageName(aCloudStorageName),
    mMountPoint(""),
    mState(CloudStorage::STATE_READY)
{
  mMountPoint.Append("/data/cloud");
  if ( -1 == mkdir(mMountPoint.get(), S_IRWXU|S_IRWXG)) {
    switch (errno) {
      case EEXIST: LOG("%s existed.", mMountPoint.get()); break;
      case ENOTDIR: LOG("Parent not a directory."); break;
      case EACCES: LOG("Search permission is denied."); break;
      case EROFS: LOG("Read-only filesystem."); break;
      default: LOG("Create %s failed with errno: %d.", mMountPoint.get(), errno);
    }
  } else {
    LOG("%s is created.", mMountPoint.get());
  }
  mMountPoint.Append("/");
  mMountPoint.Append(mCloudStorageName);
  if ( -1 == mkdir(mMountPoint.get(), S_IRWXU|S_IRWXG)) {
    switch (errno) {
      case EEXIST: LOG("%s existed.", mMountPoint.get()); break;
      case ENOTDIR: LOG("Parent not a directory."); break;
      case EACCES: LOG("Search permission is denied."); break;
      case EROFS: LOG("Read-only filesystem."); break;
      default: LOG("Create %s failed with errno: %d.", mMountPoint.get(), errno);
    }
  } else {
    LOG("%s is created.", mMountPoint.get());
  }
}

//static
const char* CloudStorage::StateStr(const CloudStorage::eState& aState)
{
  switch (aState) {
    case CloudStorage::STATE_READY: return "STATE_READY";
    case CloudStorage::STATE_RUNNING: return "STATE_RUNNING";
    default: return "STATE_UNKNOWN";
  }
  return "STATE_UNKNOWN";
}

void
CloudStorage::StartStorage()
{
  mState = CloudStorage::STATE_RUNNING;
  LOG("Start cloud storage %s", mCloudStorageName.get());
  nsresult rv = NS_NewNamedThread("CloudStorage", getter_AddRefs(mRunnableThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  MOZ_ASSERT(mRunnableThread);
  mRunnableThread->Dispatch(new CloudStorageRunnable(this), NS_DISPATCH_NORMAL);
}

void
CloudStorage::StopStorage()
{
  if (mState == CloudStorage::STATE_READY)
    return;
  LOG("Stop cloud storage %s", mCloudStorageName.get());
  mState = CloudStorage::STATE_READY;
}

} // end namespace cloudstorage
} // end namespace system
} // end namespace mozilla

