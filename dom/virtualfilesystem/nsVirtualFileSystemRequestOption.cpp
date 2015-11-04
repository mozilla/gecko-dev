/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVirtualFileSystemRequestOption.h"

#if defined(VIRTUAL_FILE_SYSTEM_LOG_TAG)
#undef VIRTUAL_FILE_SYSTEM_LOG_TAG
#endif
#define VIRTUAL_FILE_SYSTEM_LOG_TAG "VirtualFileSystemRequest"
#include "VirtualFileSystemLog.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

const char*
NS_RequestTypeStr(uint32_t aType)
{
  switch (aType) {
    case nsIVirtualFileSystemRequestManager::REQUEST_ABORT:         return "Abort";
    case nsIVirtualFileSystemRequestManager::REQUEST_GETMETADATA:   return "GetMetadata";
    case nsIVirtualFileSystemRequestManager::REQUEST_CLOSEFILE:     return "CloseFile";
    case nsIVirtualFileSystemRequestManager::REQUEST_OPENFILE:      return "OpenFile";
    case nsIVirtualFileSystemRequestManager::REQUEST_READDIRECTORY: return "ReadDirectory";
    case nsIVirtualFileSystemRequestManager::REQUEST_READFILE:      return "ReadFile";
    case nsIVirtualFileSystemRequestManager::REQUEST_UNMOUNT:       return "Unmount";
    default: return "Unknown";
  }
  return "???";
}

const char*
NS_RequestErrorStr(uint32_t aError)
{
  switch (aError) {
    case nsIVirtualFileSystemCallback::ERROR_FAILED:            return "Failed";
    case nsIVirtualFileSystemCallback::ERROR_IN_USE:            return "In_Use";
    case nsIVirtualFileSystemCallback::ERROR_EXISTS:            return "Exists";
    case nsIVirtualFileSystemCallback::ERROR_NOT_FOUND:         return "Not_Found";
    case nsIVirtualFileSystemCallback::ERROR_ACCESS_DENIED:     return "Access_Denied";
    case nsIVirtualFileSystemCallback::ERROR_TOO_MANY_OPENED:   return "Too_Many_Opened";
    case nsIVirtualFileSystemCallback::ERROR_NO_MEMORY:         return "No_Memory";
    case nsIVirtualFileSystemCallback::ERROR_NO_SPACE:          return "No_Space";
    case nsIVirtualFileSystemCallback::ERROR_NOT_A_DIRECTORY:   return "Not_A_Directory";
    case nsIVirtualFileSystemCallback::ERROR_INVALID_OPERATION: return "Invalid_Operation";
    case nsIVirtualFileSystemCallback::ERROR_SECURITY:          return "Security";
    case nsIVirtualFileSystemCallback::ERROR_ABORT:             return "Abort";
    case nsIVirtualFileSystemCallback::ERROR_NOT_A_FILE:        return "Not_A_File";
    case nsIVirtualFileSystemCallback::ERROR_NOT_EMPTY:         return "Not_Empty";
    case nsIVirtualFileSystemCallback::ERROR_INVALID_URL:       return "Invalid_URL";
    default: return "Unknown";
  }
  return "???";
}

NS_IMPL_ISUPPORTS(nsVirtualFileSystemRequestOption,
                  nsIVirtualFileSystemRequestOption)

nsVirtualFileSystemRequestOption::nsVirtualFileSystemRequestOption(const nsAString& aFileSystemId)
  : mFileSystemId(aFileSystemId)
{
}

NS_IMETHODIMP
nsVirtualFileSystemRequestOption::GetFileSystemId(nsAString& aFileSystemId)
{
  aFileSystemId = mFileSystemId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemRequestOption::SetFileSystemId(const nsAString& aFileSystemId)
{
  mFileSystemId = aFileSystemId;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsVirtualFileSystemAbortRequestOption,
                            nsVirtualFileSystemRequestOption,
                            nsIVirtualFileSystemAbortRequestOption)

nsVirtualFileSystemAbortRequestOption::nsVirtualFileSystemAbortRequestOption(
  const nsAString& aFileSystemId,
  uint32_t aOperationRequestId)
  : nsVirtualFileSystemRequestOption(aFileSystemId)
  , mOperationRequestId(aOperationRequestId)
{
}

NS_IMETHODIMP
nsVirtualFileSystemAbortRequestOption::GetOperationRequestId(uint32_t *aOperationRequestId)
{
  NS_ENSURE_ARG_POINTER(aOperationRequestId);

  *aOperationRequestId = mOperationRequestId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemAbortRequestOption::SetOperationRequestId(uint32_t aOperationRequestId)
{
  mOperationRequestId = aOperationRequestId;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsVirtualFileSystemCloseFileRequestOption,
                            nsVirtualFileSystemRequestOption,
                            nsIVirtualFileSystemCloseFileRequestOption)

nsVirtualFileSystemCloseFileRequestOption::nsVirtualFileSystemCloseFileRequestOption(
  const nsAString& aFileSystemId,
  const uint32_t aOpenRequestId)
  : nsVirtualFileSystemRequestOption(aFileSystemId)
  , mOpenRequestId(aOpenRequestId)
{
}

NS_IMETHODIMP
nsVirtualFileSystemCloseFileRequestOption::GetOpenRequestId(uint32_t* aOpenRequestId)
{
  NS_ENSURE_ARG_POINTER(aOpenRequestId);

  *aOpenRequestId = mOpenRequestId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemCloseFileRequestOption::SetOpenRequestId(uint32_t aOpenRequestId)
{
   mOpenRequestId = aOpenRequestId;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsVirtualFileSystemOpenFileRequestOption,
                            nsVirtualFileSystemRequestOption,
                            nsIVirtualFileSystemOpenFileRequestOption)

nsVirtualFileSystemOpenFileRequestOption::nsVirtualFileSystemOpenFileRequestOption(
  const nsAString& aFileSystemId,
  const nsAString& aFilePath,
  uint16_t aMode)
  : nsVirtualFileSystemRequestOption(aFileSystemId)
  , mFilePath(aFilePath)
  , mOpenMode(aMode)
{
}

NS_IMETHODIMP
nsVirtualFileSystemOpenFileRequestOption::GetFilePath(nsAString& aFilePath)
{
  aFilePath = mFilePath;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemOpenFileRequestOption::SetFilePath(const nsAString & aFilePath)
{
  mFilePath = aFilePath;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemOpenFileRequestOption::GetMode(uint16_t* aMode)
{
  NS_ENSURE_ARG_POINTER(aMode);

  *aMode = mOpenMode;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemOpenFileRequestOption::SetMode(uint16_t aMode)
{
  mOpenMode = aMode;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsVirtualFileSystemGetMetadataRequestOption,
                            nsVirtualFileSystemRequestOption,
                            nsIVirtualFileSystemGetMetadataRequestOption)

nsVirtualFileSystemGetMetadataRequestOption::nsVirtualFileSystemGetMetadataRequestOption(
  const nsAString& aFileSystemId,
  const nsAString& aEntryPath)
  : nsVirtualFileSystemRequestOption(aFileSystemId)
  , mEntryPath(aEntryPath)
{
}

NS_IMETHODIMP
nsVirtualFileSystemGetMetadataRequestOption::GetEntryPath(nsAString& aEntryPath)
{
  aEntryPath = mEntryPath;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemGetMetadataRequestOption::SetEntryPath(const nsAString& aEntryPath)
{
  mEntryPath = aEntryPath;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsVirtualFileSystemReadDirectoryRequestOption,
                            nsVirtualFileSystemRequestOption,
                            nsIVirtualFileSystemReadDirectoryRequestOption)

nsVirtualFileSystemReadDirectoryRequestOption::nsVirtualFileSystemReadDirectoryRequestOption(
  const nsAString& aFileSystemId,
  const nsAString& aDirPath)
  : nsVirtualFileSystemRequestOption(aFileSystemId)
  ,  mDirPath(aDirPath)
{
}

NS_IMETHODIMP
nsVirtualFileSystemReadDirectoryRequestOption::GetDirPath(nsAString& aDirPath)
{
  aDirPath = mDirPath;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadDirectoryRequestOption::SetDirPath(const nsAString& aDirPath)
{
  mDirPath = aDirPath;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsVirtualFileSystemReadFileRequestOption,
                            nsVirtualFileSystemRequestOption,
                            nsIVirtualFileSystemReadFileRequestOption)

nsVirtualFileSystemReadFileRequestOption::nsVirtualFileSystemReadFileRequestOption(
  const nsAString& aFileSystemId,
  const uint32_t aOpenRequestId,
  const uint64_t aOffset,
  const uint64_t aLength)
  : nsVirtualFileSystemRequestOption(aFileSystemId)
  , mOpenRequestId(aOpenRequestId)
  , mOffset(aOffset)
  , mLength(aLength)
{
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestOption::GetOpenRequestId(uint32_t* aOpenRequestId)
{
  NS_ENSURE_ARG_POINTER(aOpenRequestId);

  *aOpenRequestId = mOpenRequestId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestOption::SetOpenRequestId(uint32_t aOpenRequestId)
{
  mOpenRequestId = aOpenRequestId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestOption::GetOffset(uint64_t* aOffset)
{
  NS_ENSURE_ARG_POINTER(aOffset);

  *aOffset = mOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestOption::SetOffset(uint64_t aOffset)
{
   mOffset = aOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestOption::GetLength(uint64_t* aLength)
{
  NS_ENSURE_ARG_POINTER(aLength);

  *aLength = mLength;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemReadFileRequestOption::SetLength(uint64_t aLength)
{
  mLength = aLength;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsVirtualFileSystemUnmountRequestOption,
                            nsVirtualFileSystemRequestOption,
                            nsIVirtualFileSystemUnmountRequestOption)

nsVirtualFileSystemUnmountRequestOption::nsVirtualFileSystemUnmountRequestOption(
  const nsAString& aFileSystemId)
  : nsVirtualFileSystemRequestOption(aFileSystemId)
{
}

} // end of namespace mozilla
} // end of namespace dom
} // end of namespace virtualfilesystem
