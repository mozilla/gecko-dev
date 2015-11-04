/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuseRequestMonitor.h"
#include "nsIVirtualFileSystem.h"
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

#ifdef VIRTUAL_FILE_SYSTEM_LOG_TAG
#undef VIRTUAL_FILE_SYSTEM_LOG_TAG
#endif
#define VIRTUAL_FILE_SYSTEM_LOG_TAG "FuseRequestMonitor"
#include "VirtualFileSystemLog.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

// FuseRequestMonitor

FuseRequestMonitor::FuseRequestMonitor(FuseHandler* aFuseHandler)
  : mHandler(aFuseHandler)
{
}

void
FuseRequestMonitor::Monitor(nsIVirtualFileSystem* aVirtualFileSystem)
{
  MOZ_ASSERT(mHandler);
  RefPtr<FuseMonitorRunnable> runnable =
                              new FuseMonitorRunnable(mHandler, aVirtualFileSystem);
  nsresult rv = mHandler->DispatchRunnable(runnable);
  if (NS_FAILED(rv)) {
    ERR("Dispatching request monitor job on FUSE device failed.");
  }
}

void
FuseRequestMonitor::Stop()
{
  MOZ_ASSERT(mHandler);
  RefPtr<FuseStopRunnable> runnable = new FuseStopRunnable(mHandler);
  nsresult rv = mHandler->DispatchRunnable(runnable);
  if (NS_FAILED(rv)) {
    ERR("Dispatching stop monitor job on FUSE device failed.");
  }
}

// FuseMonitorRunnable

FuseRequestMonitor::FuseMonitorRunnable::FuseMonitorRunnable(
                                         FuseHandler* aFuseHandler,
                                         nsIVirtualFileSystem* aVirtualFileSystem)
  : mHandler(aFuseHandler),
    mVirtualFileSystem(aVirtualFileSystem)
{
}

nsresult
FuseRequestMonitor::FuseMonitorRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());

  MozFuse& fuse = mHandler->GetFuse();

  if (fuse.fuseFd == -1) {
    ERR("FUSE device file descriptor should not be -1");
    return NS_ERROR_FAILURE;
  }

  while (true) {
    if (fuse.waitForResponse) {
      NS_ProcessNextEvent();
      continue;
    }
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fuse.fuseFd, &fds);
    FD_SET(fuse.stopFds[0], &fds);
    struct timespec timeout;
    timeout.tv_sec = 10;
    timeout.tv_nsec = 0;

    int res = pselect(fuse.fuseFd+1, &fds, NULL, NULL, &timeout, NULL);
    if (res == -1 && errno != EINTR) {
      ERR("pselect error %d.", errno);
      continue;
    } else if (res == 0) { //timeout
      continue;
    } else if (FD_ISSET(fuse.fuseFd, &fds)) {
      // Handle request from FUSE device
      if (!HandleRequest()) {
        continue;
      }
    } else if (FD_ISSET(fuse.stopFds[0], &fds)) {
      LOG("the monitor job for fuse device is going to finish.");
      break;
    } else {
      ERR("should not be here.");
    }
  }
  return NS_OK;
}


bool
FuseRequestMonitor::FuseMonitorRunnable::HandleRequest()
{
  MOZ_ASSERT(!NS_IsMainThread());
  // Read one request from FUSE device
  MozFuse& fuse = mHandler->GetFuse();

  ssize_t len = read(fuse.fuseFd, fuse.requestBuffer,
                                    sizeof(fuse.requestBuffer));
  if (len < 0) {
    if (errno != EINTR) {
      ERR("[%d] handle_fuse_requests: errno=%d", fuse.token, errno);
    }
    return false;
  }
  if ((size_t)len < sizeof(struct fuse_in_header)) {
    ERR("[%d] request too short: len=%zu", fuse.token, (size_t)len);
    return false;
  }

  const struct fuse_in_header *hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);
  if (hdr->len != (size_t)len) {
    ERR("[%d] malformed header: len=%zu, hdr->len=%u", fuse.token,
       (size_t)len, hdr->len);
    return false;
  }

  // TODO: implement handling for different FUSE request.
  switch (hdr->opcode) {
    case FUSE_LOOKUP: { HandleLookup(); break; }
    case FUSE_GETATTR: { HandleGetAttr(); break; }
    case FUSE_OPEN: { HandleOpen(); break; }
    case FUSE_READ: { HandleRead(); break; }
    case FUSE_OPENDIR: { HandleOpenDir(); break; }
    case FUSE_READDIR: { HandleReadDir(); break; }
    case FUSE_RELEASEDIR: { HandleReleaseDir(); break; }
    case FUSE_RELEASE: { HandleRelease(); break; }
    case FUSE_INIT: { HandleInit(); break; }
    case FUSE_FORGET:
    case FUSE_SETATTR:
    case FUSE_MKNOD:
    case FUSE_MKDIR:
    case FUSE_UNLINK:
    case FUSE_RMDIR:
    case FUSE_RENAME:
    case FUSE_WRITE:
    case FUSE_STATFS:
    case FUSE_FSYNC:
    case FUSE_FLUSH:
    case FUSE_FSYNCDIR: {
      // currently we do nothing with these operations.
      break;
    }
    default: {
      LOG("[%d] NOTIMPL op=%d uniq=%llx nid=%llx",
           fuse.token, hdr->opcode, hdr->unique, hdr->nodeid);
      ResponseError(-ENOSYS);
      break;
    }
  }
  return true;
}

void
FuseRequestMonitor::FuseMonitorRunnable::Response(void* aData, size_t aSize)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  struct fuse_out_header outhdr;
  struct iovec vec[2];
  outhdr.len = aSize + sizeof(outhdr);
  outhdr.error = 0;
  outhdr.unique = hdr->unique;
  vec[0].iov_base = &outhdr;
  vec[0].iov_len = sizeof(outhdr);
  vec[1].iov_base = aData;
  vec[1].iov_len = aSize;
  int res = writev(fuse.fuseFd, vec, 2);
  if (res < 0) {
    ERR("Response to FUSE device failed. [%d]\n", errno);
  }
}

void
FuseRequestMonitor::FuseMonitorRunnable::ResponseError(int32_t aError)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header *hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);
  struct fuse_out_header outhdr;
  outhdr.len = sizeof(outhdr);
  outhdr.error = aError;
  outhdr.unique = hdr->unique;
  int res = write(fuse.fuseFd, &outhdr, outhdr.len);
  if (res < 0) {
    ERR("reply error to FUSE device failed. [%d]\n", errno);
  }
}

void
FuseRequestMonitor::FuseMonitorRunnable::HandleInit()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();

  const struct fuse_init_in *req = (const struct fuse_init_in*)
               (fuse.requestBuffer + sizeof(struct fuse_in_header));

  LOG("[%d] INIT ver=%d.%d maxread=%d flags=%x\n",
       fuse.token, req->major, req->minor, req->max_readahead, req->flags);

  // FUSE_KERNEL_VERSION and FUSE_KERNEL_MINOR_VERSION is from
  // system/core/sdcard/fuse.h, which is used in AOSP's emulated sdcard
  struct fuse_init_out out;
  out.major = FUSE_KERNEL_VERSION;
  out.minor = FUSE_KERNEL_MINOR_VERSION;
  out.max_readahead = req->max_readahead;
  out.flags = FUSE_ATOMIC_O_TRUNC | FUSE_BIG_WRITES;
  out.max_background = 32;
  out.congestion_threshold = 32;
  out.max_write = VIRTUAL_FILE_SYSTEM_MAX_WRITE;

  Response(((void*)&out), sizeof(out));
}

void
FuseRequestMonitor::FuseMonitorRunnable::HandleLookup()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  const char* name = (const char*)(fuse.requestBuffer+sizeof(fuse_in_header));
  nsString path = mHandler->GetPathByNodeId(hdr->nodeid);

  if (path.IsEmpty() || path.Equals(NS_LITERAL_STRING(""))) {
    LOG("Getting path by node id [%llu] failed.", hdr->nodeid);
    ResponseError(-ENOENT);
    return;
  }

  nsString childpath = path;
  if (!childpath.Equals(NS_LITERAL_STRING("/"))) {
    childpath.AppendLiteral("/");
  }
  childpath.AppendASCII(name);
  mHandler->GetNodeIdByPath(childpath);

  MOZ_ASSERT(mVirtualFileSystem);
  if (mVirtualFileSystem == nullptr) {
    LOG("Getting virtual file system interface failed.");
    ResponseError(-ENOSYS);
    return;
  }

  uint32_t requestId;
  mVirtualFileSystem->GetMetadata(childpath, &requestId);
  mHandler->SetOperationByRequestId(requestId, FUSE_LOOKUP);
  fuse.waitForResponse = true;
}

void
FuseRequestMonitor::FuseMonitorRunnable::HandleGetAttr()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  nsString path = mHandler->GetPathByNodeId(hdr->nodeid);

  if (path.IsEmpty() || path.Equals(NS_LITERAL_STRING(""))) {
    LOG("Getting path by node id [%llu] failed.", hdr->nodeid);
    ResponseError(-ENOENT);
    return;
  }

  MOZ_ASSERT(mVirtualFileSystem);
  if (mVirtualFileSystem == nullptr) {
    LOG("Getting virtual file system interface failed.");
    ResponseError(-ENOSYS);
    return;
  }

  uint32_t requestId;
  mVirtualFileSystem->GetMetadata(path, &requestId);
  mHandler->SetOperationByRequestId(requestId, FUSE_GETATTR);
  fuse.waitForResponse = true;
}

void
FuseRequestMonitor::FuseMonitorRunnable::HandleOpen()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  const struct fuse_open_in* req =
        (const struct fuse_open_in*)(fuse.requestBuffer+sizeof(fuse_in_header));

  nsString path = mHandler->GetPathByNodeId(hdr->nodeid);

  if (path.IsEmpty() || path.Equals(NS_LITERAL_STRING(""))) {
    LOG("Getting path by node id [%llu] failed.", hdr->nodeid);
    ResponseError(-ENOENT);
    return;
  }

  MOZ_ASSERT(mVirtualFileSystem);
  if (mVirtualFileSystem == nullptr) {
    LOG("Getting virtual file system interface failed.");
    ResponseError(-ENOSYS);
    return;
  }

  uint32_t requestId;
  mVirtualFileSystem->OpenFile(path, req->flags, &requestId);
  mHandler->SetOperationByRequestId(requestId, FUSE_OPEN);
  fuse.waitForResponse = true;
}

void
FuseRequestMonitor::FuseMonitorRunnable::HandleRead()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();

  const struct fuse_read_in* req =
        (const struct fuse_read_in*)(fuse.requestBuffer+sizeof(fuse_in_header));

  MOZ_ASSERT(mVirtualFileSystem);
  if (mVirtualFileSystem == nullptr) {
    LOG("Getting virtual file system interface failed.");
    ResponseError(-ENOSYS);
    return;
  }

  uint32_t requestId;
  mVirtualFileSystem->ReadFile(req->fh, req->offset, req->size, &requestId);
  mHandler->SetOperationByRequestId(requestId, FUSE_READ);
  fuse.waitForResponse = true;
}

void
FuseRequestMonitor::FuseMonitorRunnable::HandleRelease()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();

  const struct fuse_release_in* req =
     (const struct fuse_release_in*)(fuse.requestBuffer+sizeof(fuse_in_header));

  MOZ_ASSERT(mVirtualFileSystem);
  if (mVirtualFileSystem == nullptr) {
    LOG("Getting virtual file system interface failed.");
    ResponseError(-ENOSYS);
    return;
  }

  uint32_t requestId;
  mVirtualFileSystem->CloseFile(req->fh, &requestId);
  mHandler->SetOperationByRequestId(requestId, FUSE_RELEASE);
  fuse.waitForResponse = true;
}

void
FuseRequestMonitor::FuseMonitorRunnable::HandleOpenDir()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  nsString path = mHandler->GetPathByNodeId(hdr->nodeid);

  if (path.IsEmpty() || path.Equals(NS_LITERAL_STRING(""))) {
    LOG("Getting path by node id [%llu] failed.", hdr->nodeid);
    ResponseError(-ENOENT);
    return;
  }

  struct fuse_open_out out;
  out.fh = (uint64_t)time(NULL);
  out.open_flags = 0;
  out.padding = 0;
  Response(((void*)&out), sizeof(out));
}

void
FuseRequestMonitor::FuseMonitorRunnable::HandleReadDir()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mHandler);
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  nsString path = mHandler->GetPathByNodeId(hdr->nodeid);

  if (path.IsEmpty() || path.Equals(NS_LITERAL_STRING(""))) {
    LOG("Getting path by node id [%llu] failed.", hdr->nodeid);
    ResponseError(-ENOENT);
    return;
  }

  MOZ_ASSERT(mVirtualFileSystem);
  if (mVirtualFileSystem == nullptr) {
    LOG("Getting virtual file system interface failed.");
    ResponseError(-ENOSYS);
    return;
  }

  uint32_t requestId;
  mVirtualFileSystem->ReadDirectory(path, &requestId);
  mHandler->SetOperationByRequestId(requestId, FUSE_READDIR);
  fuse.waitForResponse = true;
}

// FuseStopRunnable

FuseRequestMonitor::FuseStopRunnable::FuseStopRunnable(FuseHandler* aFuseHandler)
  : mHandler(aFuseHandler)
{
}

nsresult
FuseRequestMonitor::FuseStopRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();
  char message[16] = "monitor byebye!";
  int res = write(fuse.stopFds[1], message, sizeof(message));
  if (res < 0) {
    ERR("Send stop monitor message failed.");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

} //end namespace virtualfilesystem
} //end namespace dom
} //end namespace mozilla
