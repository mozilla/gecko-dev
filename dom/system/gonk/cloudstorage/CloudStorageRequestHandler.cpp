/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorageRequestHandler.h"
#include "CloudStorageLog.h"
#include "CloudStorage.h"

//#include <pthread.h>
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

#include "nsICloudStorageInterface.h"
#include "mozilla/Services.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace system {
namespace cloudstorage {

class CloudStorageRequestRunnable : public nsRunnable
{
public:
  CloudStorageRequestRunnable(CloudStorage* aCloudStorage)
    : mCloudStorage(aCloudStorage)
  {
  }

  nsresult Run()
  {     
    nsresult rv;
//    if (!mInterface) {
      nsCOMPtr<nsICloudStorageInterface > mInterface = do_CreateInstance("@mozilla.org/cloudstorageinterface;1", &rv);
      if (NS_FAILED(rv)) {
        LOG("fail to get cloudstorageinterface [%x]", rv);
        if (mCloudStorage->IsWaitForRequest()) {
          mCloudStorage->SetWaitForRequest(false);
        }
        return NS_OK;
      }
//    }
    switch (mCloudStorage->RequestData().RequestType) {
      case FUSE_GETATTR: {
        rv = mInterface->GetFileMeta(mCloudStorage->Name(), mCloudStorage->RequestData().Path);
        if (NS_FAILED(rv)) {
          LOG("fail to call cloudstorageinterface->GetFileMeta(%s) [%x]", mCloudStorage->RequestData().Path.get(), rv);
        }
        break;
      }
      case FUSE_READDIR: {
        rv = mInterface->GetFileList(mCloudStorage->Name(), mCloudStorage->RequestData().Path);
        if (NS_FAILED(rv)) {
          LOG("fail to call cloudstorageinterface->GetFileList(%s) [%x]", mCloudStorage->RequestData().Path.get(), rv);
        }
        break;
      }
      case FUSE_READ: {
        rv = mInterface->GetData(mCloudStorage->Name(),
	                         mCloudStorage->RequestData().Path,
                                 mCloudStorage->RequestData().Size,
                                 mCloudStorage->RequestData().Offset);
	if (NS_FAILED(rv)) {
	  LOG("fail to call cloudstroageinterface->GetData(%s, %u, %llu) [%x]", mCloudStorage->RequestData().Path.get(), mCloudStorage->RequestData().Size, mCloudStorage->RequestData().Offset, rv);
	}
        break;
      }
      default: {
        LOG("Unknown request type [%u]", mCloudStorage->RequestData().RequestType);
        if (mCloudStorage->IsWaitForRequest()) {
          mCloudStorage->SetWaitForRequest(false);
        }
      }
    }
    return NS_OK;
  }
private:
  CloudStorage* mCloudStorage;
//  static nsCOMPtr<nsICloudStorageInterface> mInterface;
};

// nsCOMPtr<nsICloudStorageInterface> CloudStorageRequestRunnable::mInterface = NULL;

CloudStorageRequestHandler::CloudStorageRequestHandler(CloudStorage* aCloudStorage)
  : mCloudStorage(aCloudStorage),
    mFuse(NULL),
    mFuseHandler(NULL)
{
  Init();
}

CloudStorageRequestHandler::~CloudStorageRequestHandler()
{
  Close();
}

void
CloudStorageRequestHandler::Init()
{
  int fd;
  char opts[256];
  int res;

  umount2(mCloudStorage->MountPoint().get(), 2);

  fd = open("/dev/fuse", O_RDWR);
  if (fd < 0){
    LOG("cannot open fuse device: %s", strerror(errno));
    return;
  }

  // option for fuse filesystem
  snprintf(opts, sizeof(opts),
    "fd=%i,rootmode=40000,default_permissions,allow_other,user_id=0,group_id=1015",fd);

  res = mount("/dev/fuse", mCloudStorage->MountPoint().get(), "fuse", MS_NOSUID | MS_NODEV, opts);
  if (res < 0) {
    LOG("cannot mount fuse filesystem: %s", strerror(errno));
    close(fd);
    return;
  }

  mFuse = new Fuse();
  mFuse->fd = fd;
  mFuse->rootnid = FUSE_ROOT_ID;
  mFuse->next_generation = 0;
  mFuseHandler = new FuseHandler();
  mFuseHandler->fuse = mFuse;
  mFuseHandler->token = 0;
  
  umask(0);
}

void
CloudStorageRequestHandler::Close()
{
  umount2(mCloudStorage->MountPoint().get(), 2);
  if (mFuse) {
    close(mFuse->fd);
    delete mFuse;
    mFuse = NULL;
  }
  if (mFuseHandler) {
    delete mFuseHandler;
    mFuseHandler = NULL;
  }
  if (mCloudStorage) {
    // needn't delete cloudstorage, manager will maintain its life time
    mCloudStorage = NULL;
  }
}

void
CloudStorageRequestHandler::HandleRequests()
{
  while (mCloudStorage->State() == CloudStorage::STATE_RUNNING) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(mFuse->fd, &fds);
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100000000;

    int res = pselect(mFuse->fd+1, &fds, NULL, NULL, &timeout, NULL);
    if (res == -1 && errno != EINTR) {
      LOG("pselect error %d.", errno);
      continue;
    } else if (res == 0) { //timeout
      continue;
    } else if (FD_ISSET(mFuse->fd, &fds)) {
      ssize_t len = read(mFuse->fd, mFuseHandler->request_buffer, sizeof(mFuseHandler->request_buffer));
      if (len < 0) {
        if (errno != EINTR) {
          LOG("[%d] handle_fuse_requests: errno=%d", mFuseHandler->token, errno);
        }
        continue;
      }

      if ((size_t)len < sizeof(FuseInHeader)) {
        LOG("[%d] request too short: len=%zu", mFuseHandler->token, (size_t)len);
        continue;
      }

      const FuseInHeader *hdr = (const FuseInHeader*)((void*)mFuseHandler->request_buffer);
      if (hdr->len != (size_t)len) {
        LOG("[%d] malformed header: len=%zu, hdr->len=%u", mFuseHandler->token, (size_t)len, hdr->len);
          continue;
      }

      const void *data = mFuseHandler->request_buffer + sizeof(FuseInHeader);
      size_t data_len = len - sizeof(FuseInHeader);
      uint64_t unique = hdr->unique;

      int res = HandleRequest(hdr, data, data_len);

      if (res != CLOUD_STORAGE_NO_STATUS) {
        if (res) {
          LOG("[%d] LOG %d", mFuseHandler->token, res);
        }
        FuseOutHeader outhdr;
        outhdr.len = sizeof(outhdr);
        outhdr.error = res;
        outhdr.unique = unique;
        write(mFuse->fd, &outhdr, sizeof(outhdr));
      }
    } else {
      LOG("fds is ready, but not mFuse->fd, should not be here.");
    }
  }
}

int
CloudStorageRequestHandler::HandleRequest(const FuseInHeader *hdr, const void *data, size_t data_len)
{
  switch (hdr->opcode) {
    case FUSE_LOOKUP: { // bytez[] -> entry_out
      const char* name = (const char*)(data);
      return HandleLookup(hdr, name);
    }

    case FUSE_FORGET: {
      const FuseForgetIn *req = (const FuseForgetIn*)(data);
      return HandleForget(hdr, req);
    }

    case FUSE_GETATTR: { // getattr_in -> attr_out
      const FuseGetAttrIn *req = (const FuseGetAttrIn*)(data);
      return HandleGetAttr(hdr, req);
    }

    case FUSE_SETATTR: { // setattr_in -> attr_out
      const FuseSetAttrIn *req = (const FuseSetAttrIn*)(data);
      return HandleSetAttr(hdr, req);
    }

//    case FUSE_READLINK:
//    case FUSE_SYMLINK:
    case FUSE_MKNOD: { // mknod_in, bytez[] -> entry_out
      const FuseMkNodIn *req = (const FuseMkNodIn*)(data);
      const char *name = ((const char*) data) + sizeof(*req);
      return HandleMkNod(hdr, req, name);
    }

    case FUSE_MKDIR: { // mkdir_in, bytez[] -> entry_out
      const FuseMkDirIn *req = (const FuseMkDirIn*)(data);
      const char *name = ((const char*) data) + sizeof(*req);
      return HandleMkDir(hdr, req, name);
    }

    case FUSE_UNLINK: { // bytez[] ->
      const char* name = (const char*)(data);
      return HandleUnlink(hdr, name);
    }

    case FUSE_RMDIR: { // bytez[] ->
      const char* name = (const char*)(data);
      return HandleRmDir(hdr, name);
    }

    case FUSE_RENAME: { // rename_in, oldname, newname ->
      const FuseRenameIn *req = (const FuseRenameIn*)(data);
      const char *old_name = ((const char*) data) + sizeof(*req);
      const char *new_name = old_name + strlen(old_name) + 1;
      return HandleRename(hdr, req, old_name, new_name);
    }

//    case FUSE_LINK:
    case FUSE_OPEN: { // open_in -> open_out
      const FuseOpenIn *req = (const FuseOpenIn*)(data);
      return HandleOpen(hdr, req);
    }

    case FUSE_READ: { // read_in -> byte[]
      const FuseReadIn *req = (const FuseReadIn*)(data);
      return HandleRead(hdr, req);
    }

    case FUSE_WRITE: { // write_in, byte[write_in.size] -> write_out
      const FuseWriteIn *req = (const FuseWriteIn*)(data);
      const void* buffer = (const __u8*)data + sizeof(*req);
      return HandleWrite(hdr, req, buffer);
    }

    case FUSE_STATFS: { // getattr_in -> attr_out
      return HandleStatfs(hdr);
    }

    case FUSE_RELEASE: { // release_in ->
      const FuseReleaseIn *req = (const FuseReleaseIn*)(data);
      return HandleRelease(hdr, req);
    }

    case FUSE_FSYNC: {
      const FuseFsyncIn *req = (const FuseFsyncIn*)(data);
      return HandleFsync(hdr, req);
    }

    case FUSE_FLUSH: {
      return HandleFlush(hdr);
    }

    case FUSE_OPENDIR: { // open_in -> open_out
      const FuseOpenIn *req = (const FuseOpenIn*)(data);
      return HandleOpenDir(hdr, req);
    }

    case FUSE_READDIR: {
      const FuseReadIn *req = (const FuseReadIn*)(data);
      return HandleReadDir(hdr, req);
    }

    case FUSE_RELEASEDIR: { // release_in ->
      const FuseReleaseIn *req = (const FuseReleaseIn*)(data);
      return HandleReleaseDir(hdr, req);
    }

//    case FUSE_FSYNCDIR:
    case FUSE_INIT: { // init_in -> init_out
      const FuseInitIn *req = (const FuseInitIn*)(data);
      return HandleInit(hdr, req);
    }

    default: {
      LOG("[%d] NOTIMPL op=%d uniq=%llx nid=%llx",
           mFuseHandler->token, hdr->opcode, hdr->unique, hdr->nodeid);
      return -ENOSYS;
    }
  }
}

void
CloudStorageRequestHandler::SendRequestToMainThread()
{
  mCloudStorage->SetWaitForRequest(true);
  nsresult rv = NS_DispatchToMainThread(new CloudStorageRequestRunnable(mCloudStorage));
  if (NS_FAILED(rv)) {
    LOG("fail to dispatch to main thread [%x]", rv);
  }
  while (mCloudStorage->IsWaitForRequest() && mCloudStorage->State() == CloudStorage::STATE_RUNNING) {
    usleep(10);
  }
}

uint64_t
CloudStorageRequestHandler::AcquireOrCreateChildNId(const nsCString& childpath)
{
  uint64_t nid = mCloudStorage->GetNIdByPath(childpath);
  if (nid != 0) {
    return nid;
  }
  uint64_t* nidptr = (uint64_t*)malloc(sizeof(uint64_t));
  nid = (uint64_t) (uintptr_t)nidptr;
  mCloudStorage->PutPathByNId(nid, childpath);
  mCloudStorage->PutNIdByPath(childpath, nid);
  return nid;
}

int
CloudStorageRequestHandler::HandleLookup(const FuseInHeader *hdr, const char* name)
{
  LOG("Lookup");
  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s, passed name %s", path.get(), name);
  nsCString childpath = path;
  if (hdr->nodeid != FUSE_ROOT_ID) {
    childpath.AppendLiteral("/");
  }
  childpath.AppendASCII(name);
  LOG("childpath: %s", childpath.get());
  uint64_t childnid = AcquireOrCreateChildNId(childpath);
  if (childnid == 0) {
    return -ENOMEM;
  }
    
  FuseEntryOut out;
  out.attr.ino = hdr->nodeid;
  out.attr = mCloudStorage->GetAttrByPath(childpath);
  if (out.attr.size == 0) {
    CloudStorageRequestData reqData;
    reqData.RequestType = (uint32_t) FUSE_GETATTR;
    reqData.Path = childpath;
    mCloudStorage->SetRequestData(reqData);
    SendRequestToMainThread();
    out.attr = mCloudStorage->GetAttrByPath(childpath);
    if (out.attr.size == 0) {
      return -ENOENT;
    }
  }

  out.attr_valid = 10;
  out.entry_valid = 10;
  out.nodeid = childnid;
  out.generation = mFuse->next_generation++;

  if (mCloudStorage->State() == CloudStorage::STATE_RUNNING) {
    FuseOutHeader outhdr;
    struct iovec vec[2];
    int res;
    outhdr.len = sizeof(out) + sizeof(outhdr);
    outhdr.error = 0;
    outhdr.unique = hdr->unique;
    vec[0].iov_base = &outhdr;
    vec[0].iov_len = sizeof(outhdr);
    vec[1].iov_base = (void*) &out;
    vec[1].iov_len = sizeof(out);
    res = writev(mFuse->fd, vec, 2);
    if (res < 0) {
      LOG("*** REPLY FAILED ***");
      switch (errno) {
        case EAGAIN: LOG("EAGAIN"); break;
        case EBADF: LOG("EBADF"); break;
        case EFAULT: LOG("EFAULT"); break;
        case EFBIG: LOG("EFBIG"); break;
        case EINVAL: LOG("EINVAL"); break;
        case EINTR: LOG("EINTR"); break;
        case EIO: LOG("EIO"); break;
        case ENOSPC: LOG("ENOSPC"); break;
        case EPIPE: LOG("EPIPE"); break;
        default : LOG("Unknown error no %d", errno);
      }
    }
  }
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleForget(const FuseInHeader *hdr, const FuseForgetIn *req) 
{
  LOG("Forget");
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleGetAttr(const FuseInHeader *hdr, const FuseGetAttrIn* req) 
{
  LOG("GetAttr");
  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s", path.get());

  FuseAttrOut attrOut;
  attrOut.attr.ino = hdr->nodeid;
  attrOut.attr = mCloudStorage->GetAttrByPath(path);
  if (attrOut.attr.size == 0) {
    CloudStorageRequestData reqData;
    reqData.RequestType = (uint32_t) FUSE_GETATTR;
    reqData.Path = path;
    mCloudStorage->SetRequestData(reqData);
    SendRequestToMainThread();
    attrOut.attr = mCloudStorage->GetAttrByPath(path);
    if (attrOut.attr.size == 0) {
      return -ENOENT;
    }
  }
  attrOut.attr_valid = 10;

  if (mCloudStorage->State() == CloudStorage::STATE_RUNNING) {
    FuseOutHeader outhdr;
    struct iovec vec[2];
    int res;
    outhdr.len = sizeof(attrOut) + sizeof(outhdr);
    outhdr.error = 0;
    outhdr.unique = hdr->unique;
    vec[0].iov_base = &outhdr;
    vec[0].iov_len = sizeof(outhdr);
    vec[1].iov_base = (void*) &attrOut;
    vec[1].iov_len = sizeof(attrOut);
    res = writev(mFuse->fd, vec, 2);
    if (res < 0) {
      LOG("*** reply failed *** %d", errno);
    }
  }
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleSetAttr(const FuseInHeader *hdr, const FuseSetAttrIn *req)
{
  LOG("SetAttr");
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleMkNod(const FuseInHeader* hdr, const FuseMkNodIn* req, const char* name)
{
  LOG("MkNod");
  return CLOUD_STORAGE_NO_STATUS;
}

int CloudStorageRequestHandler::HandleMkDir(const FuseInHeader* hdr, const FuseMkDirIn* req, const char* name)
{
  LOG("MkDir");
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleUnlink(const FuseInHeader* hdr, const char* name)
{
  LOG("Unlink");
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleRmDir(const FuseInHeader* hdr, const char* name)
{
  LOG("RmDir");
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleRename(const FuseInHeader* hdr, const FuseRenameIn* req, const char* old_name, const char* new_name)
{
  LOG("Rename");
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleOpen(const FuseInHeader* hdr, const FuseOpenIn* req)
{
  LOG("Open");
  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s", path.get());

  FuseOpenOut out;
  uint64_t* handle = (uint64_t*) malloc(sizeof(uint64_t));
  out.fh = (uint64_t) (uintptr_t) handle;
  out.open_flags = 0;
  out.padding = 0;

  if (mCloudStorage->State() == CloudStorage::STATE_RUNNING) {
    FuseOutHeader outhdr;
    struct iovec vec[2];
    int res;
    outhdr.len = sizeof(out) + sizeof(outhdr);
    outhdr.error = 0;
    outhdr.unique = hdr->unique;
    vec[0].iov_base = &outhdr;
    vec[0].iov_len = sizeof(outhdr);
    vec[1].iov_base = (void*) &out;
    vec[1].iov_len = sizeof(out);
    res = writev(mFuse->fd, vec, 2);
    if (res < 0) {
      LOG("*** REPLY FAILED ***");
      switch (errno) {
        case EAGAIN: LOG("EAGAIN"); break;
        case EBADF: LOG("EBADF"); break;
        case EFAULT: LOG("EFAULT"); break;
        case EFBIG: LOG("EFBIG"); break;
        case EINVAL: LOG("EINVAL"); break;
        case EINTR: LOG("EINTR"); break;
        case EIO: LOG("EIO"); break;
        case ENOSPC: LOG("ENOSPC"); break;
        case EPIPE: LOG("EPIPE"); break;
        default : LOG("Unknown error no %d", errno);
      }
    }
  }
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleRead(const FuseInHeader* hdr, const FuseReadIn* req)
{
  LOG("Read");
  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s, nodeid: %llu, size: %d, offset: %d", path.get(), hdr->nodeid, req->size, (int)req->offset);

  CloudStorageRequestData reqData;
  reqData.RequestType = (uint32_t) FUSE_READ;
  reqData.Path = path;
  reqData.Size = req->size;
  reqData.Offset = req->offset;
  mCloudStorage->SetRequestData(reqData);
  SendRequestToMainThread();

  if (mCloudStorage->State() == CloudStorage::STATE_RUNNING) {
    if (mCloudStorage->DataBufferSize() < 0) {
      return mCloudStorage->DataBufferSize();
    }
    FuseOutHeader outhdr;
    struct iovec vec[2];
    int res;
    outhdr.len = mCloudStorage->DataBufferSize() + sizeof(outhdr);
    outhdr.error = 0;
    outhdr.unique = hdr->unique;
    vec[0].iov_base = &outhdr;
    vec[0].iov_len = sizeof(outhdr);
    vec[1].iov_base = (void*) mCloudStorage->DataBuffer();
    vec[1].iov_len = mCloudStorage->DataBufferSize();
    res = writev(mFuse->fd, vec, 2);
    if (res < 0) {
      LOG("*** REPLY FAILED ***");
      switch (errno) {
        case EAGAIN: LOG("EAGAIN"); break;
        case EBADF: LOG("EBADF"); break;
        case EFAULT: LOG("EFAULT"); break;
        case EFBIG: LOG("EFBIG"); break;
        case EINVAL: LOG("EINVAL"); break;
        case EINTR: LOG("EINTR"); break;
        case EIO: LOG("EIO"); break;
        case ENOSPC: LOG("ENOSPC"); break;
        case EPIPE: LOG("EPIPE"); break;
        default : LOG("Unknown error no %d", errno);
      }
    }
  }
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleWrite(const FuseInHeader* hdr, const FuseWriteIn* req, const void* buffer)
{
  LOG("Write");
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleStatfs(const FuseInHeader* hdr)
{
  LOG("Statfs");
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleRelease(const FuseInHeader* hdr, const FuseReleaseIn* req)
{
  LOG("Release");
  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s", path.get());
  uint64_t* handle = (uint64_t*)(uintptr_t) req->fh;
  free(handle);
  return 0;
}

int
CloudStorageRequestHandler::HandleFsync(const FuseInHeader* hdr, const FuseFsyncIn* req)
{
  LOG("Fsync");
  return 0;
}

int
CloudStorageRequestHandler::HandleFlush(const FuseInHeader* hdr)
{
  LOG("Flush");

  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s", path.get());

  return 0;
}

int
CloudStorageRequestHandler::HandleOpenDir(const FuseInHeader* hdr, const FuseOpenIn* req)
{

  LOG("OpenDir");

  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s", path.get());
  FuseOpenOut out;
  // when releaseDir is called, call free for dirHandle
  uint64_t* dirHandle = (uint64_t*) malloc(sizeof(uint64_t));
  if (!dirHandle) {
    return -ENOMEM;
  }
  out.fh = (uint64_t) (uintptr_t) dirHandle;
  out.open_flags = 0;
  out.padding = 0;
  FuseOutHeader outhdr;
  struct iovec vec[2];
  int res;
  outhdr.len = sizeof(out) + sizeof(outhdr);
  outhdr.error = 0;
  outhdr.unique = hdr->unique;
  vec[0].iov_base = &outhdr;
  vec[0].iov_len = sizeof(outhdr);
  vec[1].iov_base = (void*) &out;
  vec[1].iov_len = sizeof(out);
  res = writev(mFuse->fd, vec, 2);
  if (res < 0) {
    LOG("*** REPLY FAILED ***");
    switch (errno) {
      case EAGAIN: LOG("EAGAIN"); break;
      case EBADF: LOG("EBADF"); break;
      case EFAULT: LOG("EFAULT"); break;
      case EFBIG: LOG("EFBIG"); break;
      case EINVAL: LOG("EINVAL"); break;
      case EINTR: LOG("EINTR"); break;
      case EIO: LOG("EIO"); break;
      case ENOSPC: LOG("ENOSPC"); break;
      case EPIPE: LOG("EPIPE"); break;
      default : LOG("Unknown error no %d", errno);
    }
  }
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleReadDir(const FuseInHeader* hdr, const FuseReadIn* req)
{
  LOG("ReadDir");
  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s, offset: %llu", path.get(), req->offset);
  char buffer[8192];
  FuseDirent *fde = (FuseDirent*) buffer;
  fde->ino = FUSE_UNKNOWN_INO;
  fde->off = req->offset + 1;
  nsCString entryName;
 
  entryName = mCloudStorage->GetEntryByPathAndOffset(path, req->offset);
  if (entryName.Equals(NS_LITERAL_CSTRING(""))) {
    CloudStorageRequestData reqData;
    reqData.RequestType = (uint32_t) FUSE_READDIR;
    reqData.Path = path;
    mCloudStorage->SetRequestData(reqData);
    SendRequestToMainThread();
    entryName = mCloudStorage->GetEntryByPathAndOffset(path, req->offset);
  }

  if (mCloudStorage->State() == CloudStorage::STATE_RUNNING) {
    if (entryName.Equals("")) {
      LOG("No entry");
      return 0;
    }
    nsCString childPath = path;
    if (!path.Equals(NS_LITERAL_CSTRING("/"))) {
      childPath.Append(NS_LITERAL_CSTRING("/"));
    }
    childPath.Append(entryName);

    if (S_ISDIR(mCloudStorage->GetAttrByPath(childPath).mode)) {
      fde->type = DT_DIR;
    } else {
      fde->type = DT_REG;
    }
    fde->namelen = entryName.Length();
    memcpy(fde->name, entryName.get(), fde->namelen + 1);

    LOG("entry: %s, type: %s", entryName.get(), fde->type == DT_DIR ? "directory" : "file");

    FuseOutHeader outhdr;
    struct iovec vec[2];
    int res;
    outhdr.len = FUSE_DIRENT_ALIGN(sizeof(FuseDirent) + fde->namelen) + sizeof(outhdr);
    outhdr.error = 0;
    outhdr.unique = hdr->unique;
    vec[0].iov_base = &outhdr;
    vec[0].iov_len = sizeof(outhdr);
    vec[1].iov_base = (void*) fde;
    vec[1].iov_len = FUSE_DIRENT_ALIGN(sizeof(FuseDirent) + fde->namelen);
    res = writev(mFuse->fd, vec, 2);
    if (res < 0) {
      LOG("*** REPLY FAILED ***");
      switch (errno) {
        case EAGAIN: LOG("EAGAIN"); break;
        case EBADF: LOG("EBADF"); break;
        case EFAULT: LOG("EFAULT"); break;
        case EFBIG: LOG("EFBIG"); break;
        case EINVAL: LOG("EINVAL"); break;
        case EINTR: LOG("EINTR"); break;
        case EIO: LOG("EIO"); break;
        case ENOSPC: LOG("ENOSPC"); break;
        case EPIPE: LOG("EPIPE"); break;
        default : LOG("Unknown error no %d", errno);
      }
    }
  }
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleReleaseDir(const FuseInHeader* hdr, const FuseReleaseIn* req)
{
  LOG("ReleaseDir");
  nsCString path = mCloudStorage->GetPathByNId(hdr->nodeid);
  if (path.Equals(NS_LITERAL_CSTRING(""))) {
    return -ENOENT; 
  }
  LOG("path: %s", path.get());
  uint64_t* handle = (uint64_t*)(uintptr_t) req->fh;
  free(handle);
  return CLOUD_STORAGE_NO_STATUS;
}

int
CloudStorageRequestHandler::HandleInit(const FuseInHeader* hdr, const FuseInitIn* req)
{
  LOG("Init");
  FuseInitOut out;
  out.major = FUSE_KERNEL_VERSION;
  out.minor = FUSE_KERNEL_MINOR_VERSION;
  out.max_readahead = req->max_readahead;
  out.flags = FUSE_ATOMIC_O_TRUNC | FUSE_BIG_WRITES;
  out.max_background = 32;
  out.congestion_threshold = 32;
  out.max_write = CLOUD_STORAGE_MAX_WRITE;

  FuseOutHeader outhdr;
  struct iovec vec[2];
  int res;
  outhdr.len = sizeof(out) + sizeof(outhdr);
  outhdr.error = 0;
  outhdr.unique = hdr->unique;
  vec[0].iov_base = &outhdr;
  vec[0].iov_len = sizeof(outhdr);
  vec[1].iov_base = (void*) &out;
  vec[1].iov_len = sizeof(out);
  res = writev(mFuse->fd, vec, 2);
  if (res < 0) {
    LOG("*** REPLY FAILED ***");
    switch (errno) {
      case EAGAIN: LOG("EAGAIN"); break;
      case EBADF: LOG("EBADF"); break;
      case EFAULT: LOG("EFAULT"); break;
      case EFBIG: LOG("EFBIG"); break;
      case EINVAL: LOG("EINVAL"); break;
      case EINTR: LOG("EINTR"); break;
      case EIO: LOG("EIO"); break;
      case ENOSPC: LOG("ENOSPC"); break;
      case EPIPE: LOG("EPIPE"); break;
      default : LOG("Unknown error no %d", errno);
    }
  }

  return CLOUD_STORAGE_NO_STATUS;
}

} //end namespace cloudstorage
} //end namespace system
} //end namespace mozilla


