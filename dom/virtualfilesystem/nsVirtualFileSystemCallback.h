/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_virtualfilesystem_nsVirtualFileSystemCallback_h
#define mozilla_dom_virtualfilesystem_nsVirtualFileSystemCallback_h

#include "nsIVirtualFileSystemCallback.h"
#include "nsIVirtualFileSystem.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

/**
 *  The general callback for virtual file system request.
 *  Callback providers OnSuccess and OnError methods for request handler to
 *  notify the request is finished or failed.
 */
class nsVirtualFileSystemCallback final : public nsIVirtualFileSystemCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMCALLBACK

  nsVirtualFileSystemCallback() = default;
  nsVirtualFileSystemCallback(nsIVirtualFileSystem* aVirtualFileSystem);
private:
  ~nsVirtualFileSystemCallback() = default;

  RefPtr<nsIVirtualFileSystem> mVirtualFileSystem;
};

class nsVirtualFileSystemOpenFileCallback final : public nsIVirtualFileSystemCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMCALLBACK

  nsVirtualFileSystemOpenFileCallback() = default;
  nsVirtualFileSystemOpenFileCallback(nsIVirtualFileSystem* aVirtualFileSystem,
                                 nsIVirtualFileSystemOpenedFileInfo* aFileInfo);

private:
  ~nsVirtualFileSystemOpenFileCallback() = default;

  RefPtr<nsIVirtualFileSystem> mVirtualFileSystem;
  RefPtr<nsIVirtualFileSystemOpenedFileInfo> mFileInfo;
};

class nsVirtualFileSystemCloseFileCallback final : public nsIVirtualFileSystemCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMCALLBACK

  nsVirtualFileSystemCloseFileCallback() = default;
  nsVirtualFileSystemCloseFileCallback(nsIVirtualFileSystem* aVirtualFileSystem,
                                  const uint32_t aOpenedFileId);

private:
  ~nsVirtualFileSystemCloseFileCallback() = default;

  RefPtr<nsIVirtualFileSystem> mVirtualFileSystem;
  uint32_t mOpenedFileId;
};

} // end of namespace virtualfilesystem
} // end of namespace dom
} // end of namespace mozilla

#endif
