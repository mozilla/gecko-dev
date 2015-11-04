/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <fcntl.h>
#include <errno.h>
#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <math.h>
#include <ctype.h>
#include "FuseHandler.h"
#include "nsThreadUtils.h"

#ifdef VIRTUAL_FILE_SYSTEM_LOG_TAG
#undef VIRTUAL_FILE_SYSTEM_LOG_TAG
#endif
#define VIRTUAL_FILE_SYSTEM_LOG_TAG "FuseHandler"
#include "VirtualFileSystemLog.h"

#define VIRTUAL_FILE_SYSTEM_NO_STATUS 1

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

FuseHandler::FuseHandler(const nsAString& aFileSystemId,
                         const nsAString& aMountPoint,
                         const nsAString& aDisplayName)
  : mFuse(),
    mFileSystemId(aFileSystemId),
    mMountPoint(aMountPoint),
    mDisplayName(aDisplayName),
    mOpenedDirTable(),
    mPathTable(),
    mNodeIdTable(),
    mOperationTable(),
    mMonitor("FuseHandler"),
    mRunnableThread()
{
  mFuse.fuseFd = -1;
  mFuse.stopFds[0] = -1;
  mFuse.stopFds[1] = -1;
  mFuse.waitForResponse = false;

  char threadName[16];
  strcpy(threadName, FileSystemIdStr());
  nsresult rv = NS_NewNamedThread(threadName, getter_AddRefs(mRunnableThread));
  if (NS_FAILED(rv)) {
    ERR("Fail createing a new thread for request handling. [%x].", rv);
  }
  MOZ_ASSERT(mRunnableThread);
}

MozFuse&
FuseHandler::GetFuse()
{
  return mFuse;
}

nsString
FuseHandler::FileSystemId()
{
  return mFileSystemId;
}

const char*
FuseHandler::FileSystemIdStr()
{
  return NS_ConvertUTF16toUTF8(mFileSystemId).get();
}

nsString
FuseHandler::MountPoint()
{
  return mMountPoint;
}

const char*
FuseHandler::MountPointStr()
{
  return NS_ConvertUTF16toUTF8(mMountPoint).get();
}

nsString
FuseHandler::DisplayName()
{
  return mDisplayName;
}

const char*
FuseHandler::DisplayNameStr()
{
  return NS_ConvertUTF16toUTF8(mDisplayName).get();
}

uint32_t
FuseHandler::GetOperationByRequestId(const uint64_t aRequestId)
{
  MonitorAutoLock lock(mMonitor);
  uint32_t operation;
  if (!mOperationTable.Get(aRequestId, &operation)) {
    return 0;
  }
  return operation;
}

void
FuseHandler::SetOperationByRequestId(const uint64_t aRequestId,
                                     const uint32_t aOperation)
{
  MonitorAutoLock lock(mMonitor);
  uint32_t operation;
  if (mOperationTable.Get(aRequestId, &operation)) {
    LOG("The request id [%llu] had already set as operation %u.",
         aRequestId, operation);
    return;
  }
  mOperationTable.Put(aRequestId, aOperation);
}

void
FuseHandler::RemoveOperationByRequestId(const uint64_t aRequestId)
{
  MonitorAutoLock lock(mMonitor);
  mOperationTable.Remove(aRequestId);
}

uint64_t
FuseHandler::GetNodeIdByPath(const nsAString& aPath)
{
  MonitorAutoLock lock(mMonitor);
  uint64_t nodeId;
  if (!mNodeIdTable.Get(aPath, &nodeId)) {
    nodeId = mPathTable.Length();
    mPathTable.AppendElement(aPath);
    mNodeIdTable.Put(aPath, nodeId);
  }
  return nodeId;
}

nsString
FuseHandler::GetPathByNodeId(const uint64_t aNodeId)
{
  MonitorAutoLock lock(mMonitor);
  nsTArray<nsString>::size_type numPaths = mPathTable.Length();
  if (aNodeId >= numPaths) {
    return NS_LITERAL_STRING("");
  }
  return mPathTable[aNodeId];
}

nsresult
FuseHandler::DispatchRunnable(nsIRunnable* aRunnable)
{
  MOZ_ASSERT(aRunnable);
  MOZ_ASSERT(mRunnableThread);

  RefPtr<nsIRunnable> runnable = aRunnable;
  return mRunnableThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

} //end namespace virtualfilesystem
} //end namespace dom
} //end namespace mozilla
