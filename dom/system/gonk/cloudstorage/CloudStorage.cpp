/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorage.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
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
    CloudStorageRequestHandler* handler = new CloudStorageRequestHandler(mCloudStorage);
    if (handler) {
      RefPtr<Volume> vol = VolumeManager::FindAddVolumeByName(mCloudStorage->Name());
      vol->SetFakeVolume(mCloudStorage->MountPoint());
      VolumeManager::Dump("CloudStorage");
      handler->HandleRequests();
    } else {
      LOG("Construct cloud storage handler fail");
    }
    VolumeManager::RemoveVolumeByName(mCloudStorage->Name());
    VolumeManager::Dump("CloudStorage");
    LOG("going to finish RequestHandler.");
    return NS_OK;
  }

private:
  CloudStorage* mCloudStorage;
};

CloudStorage::CloudStorage(const nsCString& aCloudStorageName)
  : mCloudStorageName(aCloudStorageName),
    mMountPoint(""),
    mState(CloudStorage::STATE_READY),
    mWaitForRequest(false),
    mRequestData(),
    mResponseData(),
    mNodeHashTable(),
    mPathHashTable(),
    mAttrHashTable(),
    mEntryListHashTable()
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
  mNodeHashTable.Put(1, NS_LITERAL_CSTRING("/"));
  mPathHashTable.Put(NS_LITERAL_CSTRING("/"), 1);
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
  if (mState == CloudStorage::STATE_RUNNING)
    return;
  
  mState = CloudStorage::STATE_RUNNING;
  mWaitForRequest = false;
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
  mWaitForRequest = false;
}

nsCString
CloudStorage::GetPathByNId(uint64_t aKey)
{
  nsCString path;
  if (mNodeHashTable.Get(aKey, &path)) {
    return path;
  } else {
    return NS_LITERAL_CSTRING("");
  }
}

void
CloudStorage::PutPathByNId(uint64_t aKey, nsCString aPath)
{
  mNodeHashTable.Put(aKey, aPath);
}

void
CloudStorage::RemovePathByNId(uint64_t aKey)
{
  mNodeHashTable.Remove(aKey);
}

uint64_t
CloudStorage::GetNIdByPath(nsCString aKey)
{
  uint64_t NId;
  if (mPathHashTable.Get(aKey, &NId)) {
    return NId;
  } else {
    return 0;
  }
}

void
CloudStorage::PutNIdByPath(nsCString aKey, uint64_t aNId)
{
  mPathHashTable.Put(aKey, aNId);
}

void
CloudStorage::RemoveNIdByPath(nsCString aKey)
{
  mPathHashTable.Remove(aKey);
}

FuseAttr
CloudStorage::GetAttrByPath(nsCString aPath)
{
  FuseAttr res;
  if (!mAttrHashTable.Get(aPath, &res)) {
    LOG("No attr for path %s", aPath.get());
    res.size = 0;
  }
  return res;
}

FuseAttr
CloudStorage::CreateAttr(bool aIsDir, uint64_t aSize, uint64_t aMTime, uint64_t aCTime)
{
  FuseAttr attr;
  if (aIsDir) {
    attr.size = 4096;
    attr.blocks = 8;
    attr.blksize = 512;
    attr.mode = 0x41ff;
  } else {
    attr.size = aSize;
    attr.blocks = aSize/512;
    attr.blksize = 512;
    attr.mode = 0x81fd;
  }
  if (aMTime != 0 && aCTime != 0) {
    attr.atime = aMTime/1000;
    attr.mtime = aMTime/1000;
    attr.ctime = aCTime/1000;
  } else {
    attr.atime = time(NULL);
    attr.mtime = attr.atime;
    attr.ctime = attr.ctime;
  }
  attr.uid = 0;
  attr.gid = 1015;

  return attr;
}

void
CloudStorage::SetAttrByPath(nsCString aPath, bool aIsDir, uint64_t aSize, uint64_t aMTime, uint64_t aCTime)
{
  FuseAttr attr = CreateAttr(aIsDir, aSize, aMTime, aCTime);
  mAttrHashTable.Put(aPath, attr);
}

void
CloudStorage::RemoveAttrByPath(nsCString aPath)
{
  mAttrHashTable.Remove(aPath);
}

void
CloudStorage::AddEntryByPath(nsCString aPath, nsCString aEntry)
{
  nsTArray<nsCString > entryList;
  if (!mEntryListHashTable.Get(aPath, &entryList)) {
    LOG("No entry list for path %s", aPath.get());
  }
  nsTArray<nsCString >::size_type numEntries = entryList.Length();
  nsTArray<nsCString >::index_type entryIndex;
  for (entryIndex = 0; entryIndex < numEntries; ++entryIndex) {
    if (entryList[entryIndex].Equals(aEntry)) {
      return;
    }
  }
  entryList.AppendElement(aEntry);
  mEntryListHashTable.Put(aPath, entryList);
}

void
CloudStorage::RemoveEntryByPath(nsCString aPath, nsCString aEntry)
{
  nsTArray<nsCString > entryList;
  if (!mEntryListHashTable.Get(aPath, &entryList)) {
    LOG("No entry list for path %s", aPath.get());
    return;
  }
  nsTArray<nsCString >::size_type numEntries = entryList.Length();
  nsTArray<nsCString >::index_type entryIndex;
  for (entryIndex = 0; entryIndex < numEntries; ++entryIndex) {
    if (entryList[entryIndex].Equals(aEntry)) {
      entryList.RemoveElementAt(entryIndex);
    }
  }
  if (entryList.Length() != 0) {
    mEntryListHashTable.Put(aPath, entryList);
  } else {
    mEntryListHashTable.Remove(aPath);
  }
}

nsCString
CloudStorage::GetEntryByPathAndOffset(nsCString aPath, uint64_t aOffset)
{
  nsTArray<nsCString > entryList;
  if (mEntryListHashTable.Get(aPath, &entryList)) {
    if (aOffset >= entryList.Length()) {
      return NS_LITERAL_CSTRING("");
    }
    return entryList[aOffset];
  }
  return NS_LITERAL_CSTRING("");
}

void
CloudStorage::SetDataBuffer(const char* aBuffer, int32_t aSize)
{
  mBufferSize = aSize;
  memset(mBuffer, 0, 8912);
  memcpy(mBuffer, aBuffer, aSize);
}

} // end namespace cloudstorage
} // end namespace system
} // end namespace mozilla

