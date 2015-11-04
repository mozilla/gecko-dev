/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVirtualFileSystemCallback.h"
#include "nsArrayUtils.h"
#ifdef VIRTUAL_FILE_SYSTEM_LOG_TAG
#undef VIRTUAL_FILE_SYSTEM_LOG_TAG
#endif
#define VIRTUAL_FILE_SYSTEM_LOG_TAG "VirtualFileSystemCallback"
#include "VirtualFileSystemLog.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

// nsVirtualFileSystemCallback

NS_IMPL_ISUPPORTS(nsVirtualFileSystemCallback, nsIVirtualFileSystemCallback)

nsVirtualFileSystemCallback::nsVirtualFileSystemCallback(nsIVirtualFileSystem* aVirtualFileSystem)
 : mVirtualFileSystem(aVirtualFileSystem)
{
}

NS_IMETHODIMP
nsVirtualFileSystemCallback::OnSuccess(const uint32_t aRequestId,
                                  nsIVirtualFileSystemRequestValue* aValue,
                                  const bool aHasMore)
{
  MOZ_ASSERT(mVirtualFileSystem);
  return mVirtualFileSystem->OnRequestSuccess(aRequestId, aValue);
}

NS_IMETHODIMP
nsVirtualFileSystemCallback::OnError(const uint32_t aRequestId,
                                const uint32_t aError)
{
  MOZ_ASSERT(mVirtualFileSystem);
  return mVirtualFileSystem->OnRequestError(aRequestId, aError);
}

// nsVirtualFileSystemOpenFileCallback

NS_IMPL_ISUPPORTS(nsVirtualFileSystemOpenFileCallback,
                  nsIVirtualFileSystemCallback)

nsVirtualFileSystemOpenFileCallback::nsVirtualFileSystemOpenFileCallback(
                                nsIVirtualFileSystem* aVirtualFileSystem,
                                nsIVirtualFileSystemOpenedFileInfo* aFileInfo)
 : mVirtualFileSystem(aVirtualFileSystem),
   mFileInfo(aFileInfo)
{
}

NS_IMETHODIMP
nsVirtualFileSystemOpenFileCallback::OnSuccess(const uint32_t aRequestId,
                                            nsIVirtualFileSystemRequestValue* aValue,
                                            const bool aHasMore)
{
  MOZ_ASSERT(mVirtualFileSystem);
  MOZ_ASSERT(mFileInfo);

  return mVirtualFileSystem->OnOpenFileSuccess(aRequestId, aValue, mFileInfo);
}

NS_IMETHODIMP
nsVirtualFileSystemOpenFileCallback::OnError(const uint32_t aRequestId,
                                           const uint32_t aError)
{
  MOZ_ASSERT(mVirtualFileSystem);
  return mVirtualFileSystem->OnRequestError(aRequestId, aError);
}

// nsVirtualFileSystemCloseFileCallback

NS_IMPL_ISUPPORTS(nsVirtualFileSystemCloseFileCallback,
                  nsIVirtualFileSystemCallback)

nsVirtualFileSystemCloseFileCallback::nsVirtualFileSystemCloseFileCallback(
                                nsIVirtualFileSystem* aVirtualFileSystem,
                                const uint32_t aOpenedFileId)
 : mVirtualFileSystem(aVirtualFileSystem),
   mOpenedFileId(aOpenedFileId)
{
}

NS_IMETHODIMP
nsVirtualFileSystemCloseFileCallback::OnSuccess(const uint32_t aRequestId,
                                            nsIVirtualFileSystemRequestValue* aValue,
                                            const bool aHasMore)
{
  MOZ_ASSERT(mVirtualFileSystem);
  return mVirtualFileSystem->OnCloseFileSuccess(aRequestId, aValue, mOpenedFileId);
}

NS_IMETHODIMP
nsVirtualFileSystemCloseFileCallback::OnError(const uint32_t aRequestId,
                                           const uint32_t aError)
{
  MOZ_ASSERT(mVirtualFileSystem);
  return mVirtualFileSystem->OnRequestError(aRequestId, aError);
}

const char* NS_RequestErrorStr(const uint32_t aError)
{
  switch (aError) {
  case nsIVirtualFileSystemCallback::ERROR_FAILED : { return "ERROR_FAILED"; }
  case nsIVirtualFileSystemCallback::ERROR_IN_USE : { return "ERROR_IN_USE"; }
  case nsIVirtualFileSystemCallback::ERROR_EXISTS : { return "ERROR_EXISTS"; }
  case nsIVirtualFileSystemCallback::ERROR_NOT_FOUND :
                                             { return "ERROR_NOT_FOUND"; }
  case nsIVirtualFileSystemCallback::ERROR_ACCESS_DENIED :
                                             { return "ERROR_ACCESS_DENIED"; }
  case nsIVirtualFileSystemCallback::ERROR_TOO_MANY_OPENED :
                                             { return "ERROR_TOO_MANY_OPENED"; }
  case nsIVirtualFileSystemCallback::ERROR_NO_MEMORY : { return "ERROR_NO_MONEY"; }
  case nsIVirtualFileSystemCallback::ERROR_NO_SPACE : { return "ERROR_NO_SPACE"; }
  case nsIVirtualFileSystemCallback::ERROR_NOT_A_DIRECTORY :
                                             { return "ERROR_NOT_A_DIRECTORY"; }
  case nsIVirtualFileSystemCallback::ERROR_INVALID_OPERATION :
                                            { return "ERROR_INVALID_OPERATION";}
  case nsIVirtualFileSystemCallback::ERROR_SECURITY : { return "ERROR_SECURITY"; }
  case nsIVirtualFileSystemCallback::ERROR_ABORT : { return "ERROR_ABORT"; }
  case nsIVirtualFileSystemCallback::ERROR_NOT_A_FILE :
                                             { return "ERROR_NOT_A_FILE"; }
  case nsIVirtualFileSystemCallback::ERROR_NOT_EMPTY : { return "ERROR_NOT_EMPTY"; }
  case nsIVirtualFileSystemCallback::ERROR_INVALID_URL :
                                                { return "ERROR_INVALID_URL"; }
  case nsIVirtualFileSystemCallback::ERROR_TIME_OUT : { return "ERROR_TIME_OUT"; }
  default : {
    ERR("Unknown error [%d].", aError);
    return "Unknown Error";
  }
  }
  return "";
}

} // end of namespace virtualfilesystem
} // end of namespace dom
} // end of namespace mozilla
