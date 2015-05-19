/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_cloudstroagerequesthandler_h__
#define mozilla_system_cloudstroagerequesthandler_h__


#include "nsString.h"
#include "fuse.h"

namespace mozilla {
namespace system {
namespace cloudstorage {

#define CLOUD_STORAGE_NO_STATUS 1

class CloudStorageRequestHandler
{
public:
  NS_INLINE_DECL_REFCOUNTING(CloudStorageRequestHandler)
  
  CloudStorageRequestHandler(const nsCString& aMountPoint);
  virtual ~CloudStorageRequestHandler();

  void HandleOneRequest();
  void Close();

protected:
  void Init();
  void InitFuse(int aFd);
  void InitFuseHandler();

  int HandleRequest(const FuseInHeader *hdr, const void *data, size_t data_len);
  virtual int HandleLookup(const FuseInHeader *hdr, const char* name) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleForget(const FuseInHeader *hdr, const FuseForgetIn *req) { return CLOUD_STORAGE_NO_STATUS; } 
  virtual int HandleGetAttr(const FuseInHeader *hdr, const FuseGetAttrIn *req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleSetAttr(const FuseInHeader *hdr, const FuseSetAttrIn *req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleMkNod(const FuseInHeader* hdr, const FuseMkNodIn* req, const char* name) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleMkDir(const FuseInHeader* hdr, const FuseMkDirIn* req, const char* name) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleUnlink(const FuseInHeader* hdr, const char* name) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleRmDir(const FuseInHeader* hdr, const char* name) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleRename(const FuseInHeader* hdr, const FuseRenameIn* req, const char* old_name, const char* new_name) { return CLOUD_STORAGE_NO_STATUS; } 
  virtual int HandleOpen(const FuseInHeader* hdr, const FuseOpenIn* req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleRead(const FuseInHeader* hdr, const FuseReadIn* req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleWrite(const FuseInHeader* hdr, const FuseWriteIn* req, const void* buffer) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleStatfs(const FuseInHeader* hdr) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleRelease(const FuseInHeader* hdr, const FuseReleaseIn* req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleFsync(const FuseInHeader* hdr, const FuseFsyncIn* req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleFlush(const FuseInHeader* hdr) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleOpenDir(const FuseInHeader* hdr, const FuseOpenIn* req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleReadDir(const FuseInHeader* hdr, const FuseReadIn* req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleReleaseDir(const FuseInHeader* hdr, const FuseReleaseIn* req) { return CLOUD_STORAGE_NO_STATUS; }
  virtual int HandleInit(const FuseInHeader* hdr, const FuseInitIn* req) { return CLOUD_STORAGE_NO_STATUS; }

  nsCString mMountPoint;
  Fuse* mFuse;
  FuseHandler* mFuseHandler;
};

} // end namespace cloudstorage
} // end namespace system
} // end namespace mozilla

#endif // mozilla_system_cloudstoragerequesthandler_h__
