/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsVirtualFileSystemService.h"
#include "VirtualFileSystemLog.h"
#include "nsVirtualFileSystem.h"
#include "nsIMutableArray.h"
#include "nsISupportsPrimitives.h"
#include "nsISupportsUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Services.h"
#include "FuseHandler.h"
#include "FuseMounter.h"
#include "FuseRequestMonitor.h"
#include "FuseResponseHandler.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

// nsVirtualFileSystemService

NS_IMPL_ISUPPORTS(nsVirtualFileSystemService, nsIVirtualFileSystemService)

StaticRefPtr<nsIVirtualFileSystemService> nsVirtualFileSystemService::sService;

nsVirtualFileSystemService::nsVirtualFileSystemService()
  : mArrayMonitor("nsVirtualFileSystemService"),
    mVirtualFileSystemArray()
{
}

nsVirtualFileSystemService::~nsVirtualFileSystemService()
{
}

//static
already_AddRefed<nsIVirtualFileSystemService>
nsVirtualFileSystemService::GetSingleton()
{
  if (!sService) {
    sService = new nsVirtualFileSystemService();
  }
  RefPtr<nsIVirtualFileSystemService> service = sService.get();
  return service.forget();
}

// nsIVirtualFileSystemService interface implementation

NS_IMETHODIMP
nsVirtualFileSystemService::Mount(nsIVirtualFileSystemMountOptions* aOption,
                             nsIVirtualFileSystemRequestManager* aRequestMgr,
                             nsIVirtualFileSystemCallback* aCallback)
{
  nsString fileSystemId;
  nsString displayName;
  uint32_t requestId;
  aOption->GetFileSystemId(fileSystemId);
  aOption->GetDisplayName(displayName);
  aOption->GetRequestId(&requestId);

  if (fileSystemId.IsEmpty()) {
    ERR("Empty file system ID.");
    aCallback->OnError(requestId, nsIVirtualFileSystemCallback::ERROR_FAILED);
    return NS_ERROR_FAILURE;
  }

  if (displayName.IsEmpty()) {
    ERR("Empty display name.");
    aCallback->OnError(requestId, nsIVirtualFileSystemCallback::ERROR_FAILED);
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsIVirtualFileSystem> vfs = FindVirtualFileSystemById(fileSystemId);
  if (vfs) {
    LOG("The virtual file system '%s' had already created.",
         NS_ConvertUTF16toUTF8(fileSystemId).get());
    aCallback->OnError(requestId, nsIVirtualFileSystemCallback::ERROR_EXISTS);
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsVirtualFileSystem> rawstorage = new nsVirtualFileSystem(aOption);
  vfs = rawstorage;

  nsString mountPoint = nsVirtualFileSystem::CreateMountPoint(fileSystemId);

  RefPtr<FuseHandler> fh =
                      new FuseHandler(fileSystemId, mountPoint, displayName);
  sFuseHandlerTable.Put(fileSystemId, fh);

  RefPtr<FuseMounter> mounter = new FuseMounter(fh);
  mounter->Mount(aCallback, requestId);

  RefPtr<nsIVirtualFileSystemResponseHandler> responsehandler =
                                         new FuseResponseHandler(fh);
  rawstorage->SetResponseHandler(responsehandler);

  RefPtr<FuseRequestMonitor> monitor = new FuseRequestMonitor(fh);
  monitor->Monitor(vfs);

  MonitorAutoLock lock(mArrayMonitor);
  mVirtualFileSystemArray.AppendElement(vfs);

  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemService::Unmount(nsIVirtualFileSystemUnmountOptions* aOption,
                               nsIVirtualFileSystemCallback* aCallback)
{
  nsString fileSystemId;
  uint32_t requestId;
  aOption->GetFileSystemId(fileSystemId);
  aOption->GetRequestId(&requestId);

  if (fileSystemId.IsEmpty()) {
    ERR("Empty file system ID.");
    aCallback->OnError(requestId, nsIVirtualFileSystemCallback::ERROR_FAILED);
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsIVirtualFileSystem> vfs = FindVirtualFileSystemById(fileSystemId);
  if (vfs == nullptr) {
    ERR("The cloud storgae '%s' does not exist.",
         NS_ConvertUTF16toUTF8(fileSystemId).get());
    aCallback->OnError(requestId, nsIVirtualFileSystemCallback::ERROR_FAILED);
    return NS_ERROR_FAILURE;
  }

  RefPtr<FuseHandler> fh;
  if(!sFuseHandlerTable.Remove(fileSystemId,getter_AddRefs(fh))) {
    ERR("The corresponding FUSE device '%s' does not exist.",
         NS_ConvertUTF16toUTF8(fileSystemId).get());
    aCallback->OnError(requestId, nsIVirtualFileSystemCallback::ERROR_FAILED);
    return NS_ERROR_FAILURE;
  }

  RefPtr<FuseRequestMonitor> monitor = new FuseRequestMonitor(fh);
  monitor->Stop();

  RefPtr<FuseMounter> mounter = new FuseMounter(fh);
  mounter->Unmount(aCallback, requestId);

  MonitorAutoLock lock(mArrayMonitor);
  mVirtualFileSystemArray.RemoveElement(vfs);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemService::GetVirtualFileSystemById(const nsAString& aFileSystemId,
                                           nsIVirtualFileSystemInfo** aInfo)
{
  MonitorAutoLock lock(mArrayMonitor);
  RefPtr<nsIVirtualFileSystem> vfs = FindVirtualFileSystemById(aFileSystemId);
  if (vfs == nullptr) {
    ERR("The cloud storage '%s' does not exist.",
         NS_ConvertUTF16toUTF8(aFileSystemId).get());
    return NS_ERROR_NOT_AVAILABLE;
  }
  RefPtr<nsIVirtualFileSystemInfo> info;
  vfs->GetInfo(getter_AddRefs(info));
  info.forget(aInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsVirtualFileSystemService::GetAllVirtualFileSystemIds(nsIArray** aCloudNames)
{
  NS_ENSURE_ARG_POINTER(aCloudNames);
  MonitorAutoLock lock(mArrayMonitor);
  *aCloudNames = nullptr;
  nsresult rv;
  nsCOMPtr<nsIMutableArray> cloudNames =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  VirtualFileSystemArray::size_type numClouds = mVirtualFileSystemArray.Length();
  VirtualFileSystemArray::index_type cloudIndex;
  for (cloudIndex = 0; cloudIndex < numClouds; cloudIndex++) {
    RefPtr<nsIVirtualFileSystem> vfs = mVirtualFileSystemArray[cloudIndex];
    nsCOMPtr<nsISupportsString> isupportsString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    RefPtr<nsIVirtualFileSystemInfo> info;
    rv = vfs->GetInfo(getter_AddRefs(info));
    NS_ENSURE_SUCCESS(rv, rv);
    nsString fileSystemId;
    rv = info->GetFileSystemId(fileSystemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = isupportsString->SetData(fileSystemId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = cloudNames->AppendElement(isupportsString, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  cloudNames.forget(aCloudNames);
  return NS_OK;
}

/////////////////////////////////////////////////////////////////
already_AddRefed<nsIVirtualFileSystem>
nsVirtualFileSystemService::FindVirtualFileSystemById(const nsAString& aFileSystemId)
{
  MonitorAutoLock lock(mArrayMonitor);
  VirtualFileSystemArray::size_type  numVirtualFileSystems =
                                mVirtualFileSystemArray.Length();
  VirtualFileSystemArray::index_type vfsIndex;
  for (vfsIndex = 0; vfsIndex < numVirtualFileSystems; vfsIndex++) {
    RefPtr<nsIVirtualFileSystem> vfs = mVirtualFileSystemArray[vfsIndex];
    RefPtr<nsIVirtualFileSystemInfo> info;
    nsresult rv = vfs->GetInfo(getter_AddRefs(info));
    if (NS_FAILED(rv)) {
      ERR("Fail to get the cloud storage info");
      return nullptr;
    }
    nsString fileSystemId;
    rv = info->GetFileSystemId(fileSystemId);
    if (NS_FAILED(rv)) {
      ERR("Fail to get the cloud storage file system id");
      return nullptr;
    }
    if (fileSystemId.Equals(aFileSystemId)) {
      return vfs.forget();
    }
  }
  return nullptr;
}

} // end namespace virtualfilesystem
} // end namespace dom
} // end namespace mozilla
