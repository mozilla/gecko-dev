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

class CloudStorage;

#define CLOUD_STORAGE_NO_STATUS 1

class CloudStorageRequestHandler
{
public:
  NS_INLINE_DECL_REFCOUNTING(CloudStorageRequestHandler)
  
  CloudStorageRequestHandler(CloudStorage* aCloudStorage);
  virtual ~CloudStorageRequestHandler();

  void HandleRequests();

protected:
  void Init();
  void Close();

  void SendRequestToMainThread();
  uint64_t AcquireOrCreateChildNId(const nsCString& childpath);

//  void InitFuse(int aFd);
//  void InitFuseHandler();

  int HandleRequest(const FuseInHeader *hdr, const void *data, size_t data_len);
  virtual int HandleLookup(const FuseInHeader *hdr, const char* name);
  virtual int HandleForget(const FuseInHeader *hdr, const FuseForgetIn *req); 
  virtual int HandleGetAttr(const FuseInHeader *hdr, const FuseGetAttrIn *req);
  virtual int HandleSetAttr(const FuseInHeader *hdr, const FuseSetAttrIn *req);
  virtual int HandleMkNod(const FuseInHeader* hdr, const FuseMkNodIn* req, const char* name);
  virtual int HandleMkDir(const FuseInHeader* hdr, const FuseMkDirIn* req, const char* name);
  virtual int HandleUnlink(const FuseInHeader* hdr, const char* name);
  virtual int HandleRmDir(const FuseInHeader* hdr, const char* name);
  virtual int HandleRename(const FuseInHeader* hdr, const FuseRenameIn* req, const char* old_name, const char* new_name); 
  virtual int HandleOpen(const FuseInHeader* hdr, const FuseOpenIn* req);
  virtual int HandleRead(const FuseInHeader* hdr, const FuseReadIn* req);
  virtual int HandleWrite(const FuseInHeader* hdr, const FuseWriteIn* req, const void* buffer);
  virtual int HandleStatfs(const FuseInHeader* hdr);
  virtual int HandleRelease(const FuseInHeader* hdr, const FuseReleaseIn* req);
  virtual int HandleFsync(const FuseInHeader* hdr, const FuseFsyncIn* req);
  virtual int HandleFlush(const FuseInHeader* hdr);
  virtual int HandleOpenDir(const FuseInHeader* hdr, const FuseOpenIn* req);
  virtual int HandleReadDir(const FuseInHeader* hdr, const FuseReadIn* req);
  virtual int HandleReleaseDir(const FuseInHeader* hdr, const FuseReleaseIn* req);
  virtual int HandleInit(const FuseInHeader* hdr, const FuseInitIn* req);
 
  CloudStorage* mCloudStorage;
  Fuse* mFuse;
  FuseHandler* mFuseHandler;
};

} // end namespace cloudstorage
} // end namespace system
} // end namespace mozilla

#endif // mozilla_system_cloudstoragerequesthandler_h__
