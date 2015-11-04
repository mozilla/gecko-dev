/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_virtualfilesystem_fusehandler_h__
#define mozilla_dom_virtualfilesystem_fusehandler_h__

#include "nsCOMPtr.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Monitor.h"
#include "nsString.h"
#include "nsIThread.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsArray.h"

// the android fuse related data structure from /system/core/sdcard
#include "fuse.h"

#define VIRTUAL_FILE_SYSTEM_MAX_WRITE (256 * 1024)
#define VIRTUAL_FILE_SYSTEM_MAX_READ (128 * 1024)
#define VIRTUAL_FILE_SYSTEM_MAX_REQUEST_SIZE \
        (sizeof(fuse_in_header)+sizeof(fuse_write_in)+VIRTUAL_FILE_SYSTEM_MAX_WRITE)

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

class MozFuse {
public:
  bool waitForResponse;
  uint64_t nextGeneration;
  uint64_t rootId;
  int fuseFd;
  int stopFds[2];
  int token;
  uint8_t requestBuffer[VIRTUAL_FILE_SYSTEM_MAX_REQUEST_SIZE];
};

class FuseHandler final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FuseHandler)
  FuseHandler(const nsAString& aFileSystemId,
              const nsAString& aMountPoint,
              const nsAString& aDisplayName);

  MozFuse& GetFuse();

  nsString MountPoint();
  const char* MountPointStr();
  nsString FileSystemId();
  const char* FileSystemIdStr();
  nsString DisplayName();
  const char* DisplayNameStr();

  uint64_t GetNodeIdByPath(const nsAString& aPath);
  nsString GetPathByNodeId(const uint64_t aNodeId);

  uint32_t GetOperationByRequestId(const uint64_t aRequestId);
  void SetOperationByRequestId(const uint64_t aRequestId,
                               const uint32_t aOperation);
  void RemoveOperationByRequestId(const uint64_t aRequestId);

  nsresult DispatchRunnable(nsIRunnable* aRunnable);

private:
  ~FuseHandler() = default;

  MozFuse mFuse;
  nsString mFileSystemId;
  nsString mMountPoint;
  nsString mDisplayName;
  nsTArray<nsString> mOpenedDirTable;
  nsTArray<nsString> mPathTable;
  nsDataHashtable<nsStringHashKey, uint64_t> mNodeIdTable;
  nsDataHashtable<nsUint64HashKey, uint32_t> mOperationTable;
  Monitor mMonitor;
  nsCOMPtr<nsIThread> mRunnableThread;
};

typedef nsRefPtrHashtable<nsStringHashKey, FuseHandler> FuseHandlerHashtable;
static FuseHandlerHashtable sFuseHandlerTable;

} // end namespace virtualfilesystem
} // end namespace dom
} // end namespace mozilla
#endif
