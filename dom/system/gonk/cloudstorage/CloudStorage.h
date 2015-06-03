/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_cloudstorage_h_
#define mozilla_system_cloudstorage_h_

#include "nsString.h"
#include "nsIThread.h"
#include "nsCOMPtr.h"
#include "nsRefPtr.h"
#include "nsDataHashtable.h"

namespace mozilla {
namespace system {
namespace cloudstorage {

class CloudStorageRequestData final
{
public:
  NS_INLINE_DECL_REFCOUNTING(CloudStorageRequestData)
  CloudStorageRequestData()
    : RequestID(0),
      RequestType(0),
      Path(""),
      Handle(0),
      Offset(0),
      NodeId(0),
      Size(-1),
      RawData(NULL)
  {}

  uint64_t RequestID;
  uint32_t RequestType;
  nsCString Path;
  uint64_t Handle;
  uint64_t Offset;
  uint64_t NodeId;
  uint32_t Size;
  void* RawData;
};

class CloudStorageResponseData final
{
public:
  NS_INLINE_DECL_REFCOUNTING(CloudStorageResponseData)
  CloudStorageResponseData()
    : ResponseID(0),
      IsDir(false),
      FileSize(0),
      MTime(0),
      CTime(0),
      EntryName(""),
      EntryType(0),
      Size(-1),
      RawData(NULL)
  {}

  uint64_t  ResponseID;
  bool      IsDir;
  uint64_t  FileSize;
  uint64_t  MTime;
  uint64_t  CTime;
  nsCString EntryName;
  uint32_t  EntryType;
  int32_t  Size;
  void* RawData;
};

class CloudStorage final
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
  bool IsWaitForRequest() { return mWaitForRequest; }
  void SetWaitForRequest(const bool aWait) { mWaitForRequest = aWait; }
  void SetRequestData(const CloudStorageRequestData& aData) { mRequestData = aData; }
  CloudStorageRequestData RequestData() { return mRequestData; }
  void SetResponseData(const CloudStorageResponseData& aData) { mResponseData = aData; }
  CloudStorageResponseData ResponseData() { return mResponseData; }

  
  nsCString GetPathByNId(uint64_t aKey);
  void PutPathByNId(uint64_t aKey, nsCString aPath);
  void RemovePathByNId(uint64_t aKey);
  uint64_t GetNIdByPath(nsCString aKey);
  void PutNIdByPath(nsCString aKey, uint64_t aNId);
  void RemoveNIdByPath(nsCString aKey);

  void StartStorage();
  void StopStorage();

private:
  nsCString mCloudStorageName;
  nsCString mMountPoint;
  eState    mState;
  nsCOMPtr<nsIThread> mRunnableThread;
  bool      mWaitForRequest;
  CloudStorageRequestData mRequestData;
  CloudStorageResponseData mResponseData;
  nsDataHashtable<nsUint64HashKey, nsCString> mNodeHashTable;
  nsDataHashtable<nsCStringHashKey, uint64_t> mPathHashTable;
};

} // end namespace cloudstorage
} // end namespace system
} // end namespace mozilla
#endif
