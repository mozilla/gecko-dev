/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorageRequestHandler.h"
#include "CloudStorageLog.h"

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
#include <math.h>
#include <ctype.h>

namespace mozilla {
namespace system {
namespace cloudstorage {

CloudStorageRequestHandler::CloudStorageRequestHandler(const nsCString& aMountPoint)
  : mMountPoint(aMountPoint),
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

  umount2(mMountPoint.get(), 2);

  fd = open("/dev/fuse", O_RDWR);
  if (fd < 0){
    LOG("cannot open fuse device: %s", strerror(errno));
    return;
  }

  // option for fuse filesystem
  snprintf(opts, sizeof(opts),
    "fd=%i,rootmode=40000,default_permissions,allow_other,user_id=0,group_id=1015",fd);

  res = mount("/dev/fuse", mMountPoint.get(), "fuse", MS_NOSUID | MS_NODEV, opts);
  if (res < 0) {
    LOG("cannot mount fuse filesystem: %s", strerror(errno));
    close(fd);
    return;
  }

  InitFuse(fd);
  InitFuseHandler();
  
  umask(0);
}

void
CloudStorageRequestHandler::InitFuse(int aFd)
{
  mFuse = new Fuse();
  //pthread_mutex_init(&mFuse->lock, NULL);

  mFuse->fd = aFd;
  mFuse->next_generation = 0;
}

void
CloudStorageRequestHandler::InitFuseHandler()
{
  mFuseHandler = new FuseHandler();
  mFuseHandler->fuse = mFuse;
  mFuseHandler->token = 0;
}

void
CloudStorageRequestHandler::Close()
{
  umount2(mMountPoint.get(), 2);
  if (mFuse) {
    close(mFuse->fd);
    delete mFuse;
    mFuse = NULL;
  }
  if (mFuseHandler) {
    delete mFuseHandler;
    mFuseHandler = NULL;
  }
}

void
CloudStorageRequestHandler::HandleOneRequest()
{
  ssize_t len = read(mFuse->fd, mFuseHandler->request_buffer, sizeof(mFuseHandler->request_buffer));
  if (len < 0) {
    if (errno != EINTR) {
      LOG("[%d] handle_fuse_requests: errno=%d", mFuseHandler->token, errno);
    }
    return;
  }

  if ((size_t)len < sizeof(FuseInHeader)) {
    LOG("[%d] request too short: len=%zu", mFuseHandler->token, (size_t)len);
    return;
  }

  const FuseInHeader *hdr = (const FuseInHeader*)((void*)mFuseHandler->request_buffer);
  if (hdr->len != (size_t)len) {
    LOG("[%d] malformed header: len=%zu, hdr->len=%u", mFuseHandler->token, (size_t)len, hdr->len);
      return;
  }

  const void *data = mFuseHandler->request_buffer + sizeof(FuseInHeader);
  size_t data_len = len - sizeof(FuseInHeader);
  __u64 unique = hdr->unique;
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

/*
int
DropboxRequestHandler::HandleLookup(Fuse* fuse, FuseHandler* handler, const FuseInHeader *hdr, const char* name)
{
  return FuseReplyEntry(fuse, hdr->unique, parent_node, name, actual_name, child_path);
}

int
DropboxRequestHandler::HandleGetAttr(Fuse* fuse, FuseHandler* handler, const FuseInHeader *hdr, const FuseGetAttrIn *req)
{
  // input: file/directory path
  // output: file/directory attributes
  //         size
  //         blocks
  //         blksize
  //         atime
  //         mtime
  //         ctime
  //         atimensec
  //         mtimensec
  //         ctimensec
  return CLOUD_STORAGE_NO_STATUS;
}


int
DropboxRequestHandler::HandleOpen(Fuse* fuse, FuseHandler* handler, const FuseInHeader* hdr, const FuseOpenIn* req)
{
  // input: file path
  //        request flags
  // output: file handler
  //         open flags
  //         padding
  return CLOUD_STORAGE_NO_STATUS;
}

int
DropboxRequestHandler::HandleRead(Fuse* fuse, FuseHandler* handler,const FuseInHeader* hdr, const FuseReadIn* req)
{
  // input: file path
  //        file handle
  //        read size
  //        read offset
  // output: data bytearray
  return CLOUD_STORAGE_NO_STATUS;
}

int
DropboxRequestHandler::HandleOpenDir(Fuse* fuse, FuseHandler* handler, const FuseInHeader* hdr, const FuseOpenIn* req)
{
  // input: directory path
  //        request flags
  // output: directory handle
  //         open flags
  //         padding
  return CLOUD_STORAGE_NO_STATUS;
}

int
DropboxRequestHandler::HandleReadDir(Fuse* fuse, FuseHandler* handler, const FuseInHeader* hdr, const FuseReadIn* req)
{
  // input: directory path
  //        directory handle
  //        request offset
  // output: entry path
  //         entry type
  //         entry name length
  //         entry offset
  return CLOUD_STORAGE_NO_STATUS;
}

int
DropboxRequestHandler::HandleReleaseDir(Fuse* fuse, FuseHandler* handler, const FuseInHeader* hdr, const FuseReleaseIn* req)
{
  return 0;
}

int
DropboxRequestHandler::HandleInit(Fuse* fuse, FuseHandler* handler, const FuseInHeader* hdr, const FuseInitIn* req)
{
  FuseInitOut out;
  out.major = FUSE_KERNEL_VERSION;
  out.minor = FUSE_KERNEL_MINOR_VERSION;
  out.max_readahead = req->max_readahead;
  out.flags = FUSE_ATOMIC_O_TRUNC | FUSE_BIG_WRITES;
  out.max_background = 32;
  out.congestion_threshold = 32;
  out.max_write = CLOUD_STORAGE_MAX_WRITE;
  FuseReply(fuse, hdr->unique, &out, sizeof(out));
  return CLOUD_STORAGE_NO_STATUS;
}
*/

} //end namespace cloudstorage
} //end namespace system
} //end namespace mozilla


