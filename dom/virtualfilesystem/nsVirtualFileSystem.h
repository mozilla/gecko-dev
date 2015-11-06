/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsvirtualfilesystem_h_
#define mozilla_dom_nsvirtualfilesystem_h_

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsIVirtualFileSystemService.h"
#include "nsIVirtualFileSystem.h"
#include "nsIVirtualFileSystemCallback.h"
#include "nsIVirtualFileSystemRequestManager.h"
#include "nsIVirtualFileSystemResponseHandler.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

class nsVirtualFileSystemOpenedFileInfo final : public nsIVirtualFileSystemOpenedFileInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMOPENEDFILEINFO

  nsVirtualFileSystemOpenedFileInfo() = default;
  nsVirtualFileSystemOpenedFileInfo(const uint32_t aOpenedRequestId,
                               const nsAString& aPath,
                               const uint16_t aOpenMode);

private:
  ~nsVirtualFileSystemOpenedFileInfo() = default;

  uint32_t mOpenRequestId;
  nsString mFilePath;
  uint16_t mMode;
};

class nsVirtualFileSystemInfo final : public nsIVirtualFileSystemInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMINFO

  nsVirtualFileSystemInfo() = default;
  nsVirtualFileSystemInfo(nsIVirtualFileSystemMountOptions* aOption);

  void AppendOpenedFile(already_AddRefed<nsIVirtualFileSystemOpenedFileInfo> aInfo);
  void RemoveOpenedFile(const uint32_t aOpenRequestId);
private:
  ~nsVirtualFileSystemInfo() = default;

  RefPtr<nsIVirtualFileSystemMountOptions> mOption;
  nsTArray<RefPtr<nsIVirtualFileSystemOpenedFileInfo>> mOpenedFiles;
};

class nsVirtualFileSystem final : public nsIVirtualFileSystem
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEM

  nsVirtualFileSystem(nsIVirtualFileSystemMountOptions* aOption);

  const char* FileSystemIdStr();
  const char* DisplayNameStr();
  const char* MountPointStr();
  const bool IsWritable();
  const nsString GetFileSystemId();
  const nsString GetMountPoint();

  static nsString CreateMountPoint(const nsAString& aFileSystemId);


  void Mount(nsIVirtualFileSystemCallback* aCallback);
  void SetResponseHandler(nsIVirtualFileSystemResponseHandler* aResponseHandler);
  void SetRequestManager(nsIVirtualFileSystemRequestManager* aRequestManager);

private:
  ~nsVirtualFileSystem();

  RefPtr<nsVirtualFileSystemInfo> mInfo;
  RefPtr<nsIVirtualFileSystemRequestManager> mRequestManager;
  RefPtr<nsIVirtualFileSystemResponseHandler> mResponseHandler;
  nsString mMountPoint;
};

} // end namespace virtualfilesystem
} // end namespace dom
} // end namespace mozilla
#endif
