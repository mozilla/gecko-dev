/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVirtualFileSystem.h"
#include "nsIVirtualFileSystemRequestOption.h"
#include "nsISupportsUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsIMutableArray.h"
#include "nsThreadUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"
#include "nsVirtualFileSystemCallback.h"

//#include "FuseRequestHandler.h"
#ifdef VIRTUAL_FILE_SYSTEM_LOG_TAG
#undef VIRTUAL_FILE_SYSTEM_LOG_TAG
#endif
#define VIRTUAL_FILE_SYSTEM_LOG_TAG "VirtualFileSystem"
#include "VirtualFileSystemLog.h"

#define MOUNTROOT "/data/vfs"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

// nsVirtualFileSystemOpenedFileInfo

NS_IMPL_ISUPPORTS(nsVirtualFileSystemOpenedFileInfo, nsIVirtualFileSystemOpenedFileInfo)

nsVirtualFileSystemOpenedFileInfo::nsVirtualFileSystemOpenedFileInfo(
                              const uint32_t aOpenRequestId,
                              const nsAString& aFilePath,
                              const uint16_t aMode)
  : mOpenRequestId(aOpenRequestId),
    mFilePath(aFilePath),
    mMode(aMode)
{
}

NS_IMETHODIMP
nsVirtualFileSystemOpenedFileInfo::GetOpenRequestId(uint32_t* aOpenRequestId)
{
  *aOpenRequestId = mOpenRequestId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemOpenedFileInfo::SetOpenRequestId(const uint32_t aOpenRequestId)
{
  mOpenRequestId = aOpenRequestId;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemOpenedFileInfo::GetFilePath(nsAString& aFilePath)
{
  aFilePath = mFilePath;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemOpenedFileInfo::SetFilePath(const nsAString& aFilePath)
{
  mFilePath = aFilePath;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemOpenedFileInfo::GetMode(uint16_t* aMode)
{
  *aMode = mMode;
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemOpenedFileInfo::SetMode(const uint16_t aMode)
{
  mMode = aMode;
  return NS_OK;
}

// nsVirtualFileSystemInfo

NS_IMPL_ISUPPORTS(nsVirtualFileSystemInfo, nsIVirtualFileSystemInfo)

nsVirtualFileSystemInfo::nsVirtualFileSystemInfo(nsIVirtualFileSystemMountOptions* aOption)
  : mOption(nullptr),
    mOpenedFiles()
{
  mOption = aOption;
}

void
nsVirtualFileSystemInfo::AppendOpenedFile(
                          already_AddRefed<nsIVirtualFileSystemOpenedFileInfo> aInfo)
{
  mOpenedFiles.AppendElement(aInfo);
}

void
nsVirtualFileSystemInfo::RemoveOpenedFile(const uint32_t aOpenedRequestId)
{
  nsTArray<RefPtr<nsIVirtualFileSystemOpenedFileInfo>>::size_type numInfos
                                                        = mOpenedFiles.Length();
  nsTArray<RefPtr<nsIVirtualFileSystemOpenedFileInfo>>::index_type idx;
  RefPtr<nsIVirtualFileSystemOpenedFileInfo> info;
  for (idx = 0; idx < numInfos; ++idx) {
    uint32_t requestId;
    mOpenedFiles[idx]->GetOpenRequestId(&requestId);
    if (requestId == aOpenedRequestId) {
      info = mOpenedFiles[idx];
      break;
    }
  }
  if (info != nullptr) {
    mOpenedFiles.RemoveElement(info);
  }
}

// nsIVirtualFileSystemInfo interface implementation

NS_IMETHODIMP
nsVirtualFileSystemInfo::SetFileSystemId(const nsAString& aFileSystemId)
{
  return mOption->SetFileSystemId(aFileSystemId);
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::GetFileSystemId(nsAString& aFileSystemId)
{
  return mOption->GetFileSystemId(aFileSystemId);
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::SetDisplayName(const nsAString& aDisplayName)
{
  return mOption->SetDisplayName(aDisplayName);
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::GetDisplayName(nsAString& aDisplayName)
{
  return mOption->GetDisplayName(aDisplayName);
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::SetWritable(const bool aWritable)
{
  return mOption->SetWritable(aWritable);
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::GetWritable(bool* aWritable)
{
  return mOption->GetWritable(aWritable);
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::SetOpenedFilesLimit(const uint32_t aLimit)
{
  return mOption->SetOpenedFilesLimit(aLimit);
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::GetOpenedFilesLimit(uint32_t* aLimit)
{
  return mOption->GetOpenedFilesLimit(aLimit);
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::GetOpenedFiles(nsIArray** aOpenedFiles)
{
  NS_ENSURE_ARG_POINTER(aOpenedFiles);
  *aOpenedFiles = nullptr;
  nsresult rv;
  nsCOMPtr<nsIMutableArray> openedFiles =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<RefPtr<nsIVirtualFileSystemOpenedFileInfo>>::size_type numOpened
                                                        = mOpenedFiles.Length();
  nsTArray<RefPtr<nsIVirtualFileSystemOpenedFileInfo>>::index_type index;
  for (index = 0; index < numOpened; index++) {
    RefPtr<nsIVirtualFileSystemOpenedFileInfo> info = mOpenedFiles[index];
    rv = openedFiles->AppendElement(info, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  openedFiles.forget(aOpenedFiles);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemInfo::SetOpenedFiles(nsIArray* aOpenFiles)
{
  return NS_OK;
}

/**************************************************************/
NS_IMPL_ISUPPORTS(nsVirtualFileSystem, nsIVirtualFileSystem)

nsVirtualFileSystem::nsVirtualFileSystem(nsIVirtualFileSystemMountOptions* aOption)
  : mInfo(new nsVirtualFileSystemInfo(aOption)),
    mRequestManager(nullptr),
    mResponseHandler(nullptr),
    mMountPoint(NS_LITERAL_STRING(""))
{
  MOZ_ASSERT(mInfo);
  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  mMountPoint = nsVirtualFileSystem::CreateMountPoint(fileSystemId);
}

nsVirtualFileSystem::~nsVirtualFileSystem()
{
}

const char*
nsVirtualFileSystem::FileSystemIdStr()
{
  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  return NS_ConvertUTF16toUTF8(fileSystemId).get();
}

const char*
nsVirtualFileSystem::DisplayNameStr()
{
  nsString displayName;
  mInfo->GetDisplayName(displayName);
  return NS_ConvertUTF16toUTF8(displayName).get();
}

const bool
nsVirtualFileSystem::IsWritable()
{
  bool isWritable;
  mInfo->GetWritable(&isWritable);
  return isWritable;
}

const char*
nsVirtualFileSystem::MountPointStr()
{
  return NS_ConvertUTF16toUTF8(mMountPoint).get();
}

const nsString
nsVirtualFileSystem::GetFileSystemId()
{
  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  return fileSystemId;
}

const nsString
nsVirtualFileSystem::GetMountPoint()
{
  return mMountPoint;
}

nsString
nsVirtualFileSystem::CreateMountPoint(const nsAString& aFileSystemId)
{
  nsString mountPoint = NS_LITERAL_STRING(MOUNTROOT);
  mountPoint.Append(NS_LITERAL_STRING("/"));
  mountPoint.Append(aFileSystemId);
  return mountPoint;
}

void
nsVirtualFileSystem::SetResponseHandler(nsIVirtualFileSystemResponseHandler* aHandler)
{
  mResponseHandler = aHandler;
}

void
nsVirtualFileSystem::SetRequestManager(nsIVirtualFileSystemRequestManager* aManager)
{
  mRequestManager = aManager;
}

// nsIVirtualFileSystem interface implmentation
NS_IMETHODIMP
nsVirtualFileSystem::GetInfo(nsIVirtualFileSystemInfo** aInfo)
{
  RefPtr<nsIVirtualFileSystemInfo> info = mInfo;
  info.forget(aInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystem::Abort(const uint32_t aOperationId, uint32_t* aRequestId)
{
  nsCOMPtr<nsIVirtualFileSystemAbortRequestOption> option =
                          do_CreateInstance(VIRTUALFILESYSTEMABORTREQUESTOPTION_CID);

  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  option->SetFileSystemId(fileSystemId);
  option->SetOperationRequestId(aOperationId);

  RefPtr<nsIVirtualFileSystemCallback> callback = new nsVirtualFileSystemCallback(this);

  MOZ_ASSERT(mRequestManager);

  return mRequestManager->CreateRequest(
                                   nsIVirtualFileSystemRequestManager::REQUEST_ABORT,
                                   option,
                                   callback,
                                   aRequestId);
}

NS_IMETHODIMP
nsVirtualFileSystem::OpenFile(const nsAString& aPath,
                         const uint16_t aMode,
                         uint32_t* aRequestId)
{
  nsCOMPtr<nsIVirtualFileSystemOpenFileRequestOption> option =
                       do_CreateInstance(VIRTUALFILESYSTEMOPENFILEREQUESTOPTION_CID);

  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  option->SetFileSystemId(fileSystemId);
  option->SetFilePath(aPath);
  option->SetMode(aMode);


  RefPtr<nsIVirtualFileSystemCallback> callback =
         new nsVirtualFileSystemOpenFileCallback(this,
             new nsVirtualFileSystemOpenedFileInfo(*aRequestId, aPath, aMode));

  MOZ_ASSERT(mRequestManager);

  return mRequestManager->CreateRequest(
                                nsIVirtualFileSystemRequestManager::REQUEST_OPENFILE,
                                option,
                                callback,
                                aRequestId);
}

NS_IMETHODIMP
nsVirtualFileSystem::CloseFile(const uint32_t aOpenFileId,
                          uint32_t* aRequestId)
{
  nsCOMPtr<nsIVirtualFileSystemCloseFileRequestOption> option =
                      do_CreateInstance(VIRTUALFILESYSTEMCLOSEFILEREQUESTOPTION_CID);

  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  option->SetFileSystemId(fileSystemId);
  option->SetOpenRequestId(aOpenFileId);

  RefPtr<nsIVirtualFileSystemCallback> callback =
         new nsVirtualFileSystemCloseFileCallback(this, aOpenFileId);

  MOZ_ASSERT(mRequestManager);

  return mRequestManager->CreateRequest(
                               nsIVirtualFileSystemRequestManager::REQUEST_CLOSEFILE,
                               option,
                               callback,
                               aRequestId);
}

NS_IMETHODIMP
nsVirtualFileSystem::GetMetadata(const nsAString& aEntryPath,
                            uint32_t* aRequestId)
{
  nsCOMPtr<nsIVirtualFileSystemGetMetadataRequestOption> option =
                    do_CreateInstance(VIRTUALFILESYSTEMGETMETADATAREQUESTOPTION_CID);

  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  option->SetFileSystemId(fileSystemId);
  option->SetEntryPath(aEntryPath);

  RefPtr<nsIVirtualFileSystemCallback> callback = new nsVirtualFileSystemCallback(this);

  MOZ_ASSERT(mRequestManager);

  return mRequestManager->CreateRequest(
                             nsIVirtualFileSystemRequestManager::REQUEST_GETMETADATA,
                             option,
                             callback,
                             aRequestId);
}

NS_IMETHODIMP
nsVirtualFileSystem::ReadDirectory(const nsAString& aDirPath,
                              uint32_t* aRequestId)
{
  nsCOMPtr<nsIVirtualFileSystemReadDirectoryRequestOption> option =
                  do_CreateInstance(VIRTUALFILESYSTEMREADDIRECTORYREQUESTOPTION_CID);

  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  option->SetFileSystemId(fileSystemId);
  option->SetDirPath(aDirPath);

  RefPtr<nsIVirtualFileSystemCallback> callback = new nsVirtualFileSystemCallback(this);

  MOZ_ASSERT(mRequestManager);

  return mRequestManager->CreateRequest(
                           nsIVirtualFileSystemRequestManager::REQUEST_READDIRECTORY,
                           option,
                           callback,
                           aRequestId);
}

NS_IMETHODIMP
nsVirtualFileSystem::ReadFile(const uint32_t aOpenFileId,
                         const uint64_t aOffset,
                         const uint64_t aLength,
                         uint32_t* aRequestId)
{
  nsCOMPtr<nsIVirtualFileSystemReadFileRequestOption> option =
                  do_CreateInstance(VIRTUALFILESYSTEMREADFILEREQUESTOPTION_CID);

  nsString fileSystemId;
  mInfo->GetFileSystemId(fileSystemId);
  option->SetFileSystemId(fileSystemId);
  option->SetOpenRequestId(aOpenFileId);
  option->SetOffset(aOffset);
  option->SetLength(aLength);

  RefPtr<nsIVirtualFileSystemCallback> callback = new nsVirtualFileSystemCallback(this);

  MOZ_ASSERT(mRequestManager);

  return mRequestManager->CreateRequest(
                                nsIVirtualFileSystemRequestManager::REQUEST_READFILE,
                                option,
                                callback,
                                aRequestId);
}

NS_IMETHODIMP
nsVirtualFileSystem::Unmount(uint32_t* aRequestId)
{
  RefPtr<nsIVirtualFileSystemCallback> callback = new nsVirtualFileSystemCallback(this);

  MOZ_ASSERT(mRequestManager);

  return mRequestManager->CreateRequest(
                                nsIVirtualFileSystemRequestManager::REQUEST_UNMOUNT,
                                nullptr,
                                callback,
                                aRequestId);
}

NS_IMETHODIMP
nsVirtualFileSystem::OnRequestSuccess(const uint32_t aRequestId,
                                 nsIVirtualFileSystemRequestValue* aValue)
{
  MOZ_ASSERT(mResponseHandler);
  mResponseHandler->OnSuccess(aRequestId, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystem::OnOpenFileSuccess(const uint32_t aRequestId,
                                  nsIVirtualFileSystemRequestValue* aValue,
                                  nsIVirtualFileSystemOpenedFileInfo* aFileInfo)
{
  RefPtr<nsIVirtualFileSystemOpenedFileInfo> fileInfo = aFileInfo;
  aFileInfo->SetOpenRequestId(aRequestId);
  MOZ_ASSERT(mInfo);
  mInfo->AppendOpenedFile(fileInfo.forget());
  MOZ_ASSERT(mResponseHandler);
  mResponseHandler->OnSuccess(aRequestId, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystem::OnCloseFileSuccess(const uint32_t aRequestId,
                                   nsIVirtualFileSystemRequestValue* aValue,
                                   const uint32_t aOpenedFileId)
{
  MOZ_ASSERT(mInfo);
  mInfo->RemoveOpenedFile(aOpenedFileId);
  MOZ_ASSERT(mResponseHandler);
  mResponseHandler->OnSuccess(aRequestId, aValue);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystem::OnRequestError(const uint32_t aRequestId,
                               const uint32_t aError)
{
  MOZ_ASSERT(mResponseHandler);
  mResponseHandler->OnError(aRequestId, aError);
  return NS_OK;
}


} // end namespace virtualfilesystem
} // end namespace dom
} // end namespace mozilla
