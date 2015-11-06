/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FuseResponseHandler.h"
#include "nsArrayUtils.h"
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
#define VIRTUAL_FILE_SYSTEM_LOG_TAG "FuseResponseHandler"
#include "VirtualFileSystemLog.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

// FuseResponseHandler

FuseResponseHandler::FuseResponseHandler(FuseHandler* aFuseHandler)
 : mHandler(aFuseHandler)
{
}

NS_IMETHODIMP
FuseResponseHandler::OnSuccess(const uint32_t aRequestId,
                               nsIVirtualFileSystemRequestValue* aValue)
{
  MOZ_ASSERT(mHandler);
  RefPtr<FuseSuccessRunnable> runnable =
                          new FuseSuccessRunnable(mHandler, aRequestId, aValue);
  nsresult rv = mHandler->DispatchRunnable(runnable);
  if (NS_FAILED(rv)) {
    ERR("Dispatching success response to fuse device failed.");
  }
  return rv;
}

NS_IMETHODIMP
FuseResponseHandler::OnError(const uint32_t aRequestId,
                             const uint32_t aError)
{
  MOZ_ASSERT(mHandler);
  RefPtr<FuseErrorRunnable> runnable =
                          new FuseErrorRunnable(mHandler, aRequestId, aError);
  nsresult rv = mHandler->DispatchRunnable(runnable);
  if (NS_FAILED(rv)) {
    ERR("Dispatching success response to fuse device failed.");
  }
  return rv;
}

// FuseSuccuessRunnable
FuseResponseHandler::FuseSuccessRunnable::FuseSuccessRunnable(
                                          FuseHandler* aFuseHandler,
                                          const uint32_t aRequestId,
                                          nsIVirtualFileSystemRequestValue* aValue)
 : mRequestId(aRequestId),
   mHandler(aFuseHandler),
   mValue(aValue)
{
}

nsresult
FuseResponseHandler::FuseSuccessRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mHandler);
  uint32_t operation = mHandler->GetOperationByRequestId(mRequestId);
  switch (operation) {
    case FUSE_LOOKUP:  { HandleLookup(); break; }
    case FUSE_GETATTR: { HandleGetAttr(); break; }
    case FUSE_OPEN: { HandleOpen(); break; }
    case FUSE_READ: { HandleRead(); break; }
    case FUSE_READDIR: { HandleReadDir(); break; }
    case FUSE_RELEASE:   // needn't do anything
    case FUSE_OPENDIR:   // already handled in monitor
    case FUSE_RELEASEDIR:// already handled in monitor
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
    case FUSE_FSYNCDIR:
    case FUSE_INIT:
    default: {
      break;
    }
  }
  mHandler->RemoveOperationByRequestId(mRequestId);
  return NS_OK;
}

void
FuseResponseHandler::FuseSuccessRunnable::HandleLookup()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIVirtualFileSystemGetMetadataRequestValue> value =
                                                 do_QueryInterface(mValue);
  MOZ_ASSERT(value);
  nsCOMPtr<nsIEntryMetadata> data;
  value->GetMetadata(getter_AddRefs(data));

  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  nsString path = mHandler->GetPathByNodeId(hdr->nodeid);

  if (path.IsEmpty() || path.Equals(NS_LITERAL_STRING(""))) {
    ERR("Getting path by node id [%llu] failed.", hdr->nodeid);
    return;
  }

  nsString name;
  data->GetName(name);
  nsString childpath = path;
  if (!childpath.Equals(NS_LITERAL_STRING("/"))) {
    childpath.AppendLiteral("/");
  }
  childpath.Append(name);
  uint64_t childnodeid = mHandler->GetNodeIdByPath(childpath);

  struct fuse_entry_out out;
  out.attr = CreateAttrByMetadata(data);
  out.attr.ino = hdr->nodeid;
  out.nodeid = childnodeid;
  out.attr_valid = 10;
  out.entry_valid = 10;
  out.generation = fuse.nextGeneration++;
  Response(((void*)&out), sizeof(out));
}

void
FuseResponseHandler::FuseSuccessRunnable::HandleGetAttr()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIVirtualFileSystemGetMetadataRequestValue> value =
                                                 do_QueryInterface(mValue);
  MOZ_ASSERT(value);
  nsCOMPtr<nsIEntryMetadata> data;
  value->GetMetadata(getter_AddRefs(data));

  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  struct fuse_attr_out out;
  out.attr = CreateAttrByMetadata(data);
  out.attr.ino = hdr->nodeid;
  out.attr_valid = 10;
  Response(((void*)&out), sizeof(out));
}

void
FuseResponseHandler::FuseSuccessRunnable::HandleOpen()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mHandler);

  struct fuse_open_out out;
  out.fh = mRequestId;
  out.open_flags = 0;
  out.padding = 0;
  Response(((void*)&out), sizeof(out));
}

void
FuseResponseHandler::FuseSuccessRunnable::HandleRead()
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsCOMPtr<nsIVirtualFileSystemReadFileRequestValue> value =
                                                 do_QueryInterface(mValue);

  MOZ_ASSERT(value);
  nsCString data;
  value->GetData(data);
  Response(((void*)(data.get())), sizeof(data.get()));
}

void
FuseResponseHandler::FuseSuccessRunnable::HandleReadDir()
{
  MOZ_ASSERT(!NS_IsMainThread());
  nsCOMPtr<nsIVirtualFileSystemReadDirectoryRequestValue> value =
                                                     do_QueryInterface(mValue);
  MOZ_ASSERT(value);

  nsCOMPtr<nsIArray> entries;
  value->GetEntries(getter_AddRefs(entries));

  MozFuse& fuse = mHandler->GetFuse();

  const struct fuse_read_in* req =
        (const struct fuse_read_in*)(fuse.requestBuffer+sizeof(fuse_in_header));

  uint32_t length;
  entries->GetLength(&length);

  if (req->offset > length) {
    ResponseError(0);
    return;
  }

  nsCOMPtr<nsIEntryMetadata> entry = do_QueryElementAt(entries, req->offset);

  MOZ_ASSERT(entry);

  char buffer[8192];
  struct fuse_dirent* fde = (struct fuse_dirent*)buffer;
  fde->ino = 0xffffffff;
  fde->off = req->offset + 1;
  bool isDir;
  entry->GetIsDirectory(&isDir);
  if (isDir) {
    fde->type = DT_DIR;
  } else {
    fde->type = DT_REG;
  }
  nsString name;
  entry->GetName(name);
  fde->namelen = name.Length();
  memcpy(fde->name, NS_ConvertUTF16toUTF8(name).get(), fde->namelen+1);
  size_t fdesize = FUSE_DIRENT_ALIGN(sizeof(struct fuse_dirent) + fde->namelen);
  Response(((void*)fde),fdesize);
}

void
FuseResponseHandler::FuseSuccessRunnable::Response(void* aData, size_t aSize)
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
  fuse.waitForResponse = false;
}

void
FuseResponseHandler::FuseSuccessRunnable::ResponseError(uint32_t aError)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header* hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);

  struct fuse_out_header outhdr;
  outhdr.len = sizeof(outhdr);
  outhdr.error = aError;
  outhdr.unique = hdr->unique;
  int res = write(fuse.fuseFd, &outhdr, outhdr.len);
  if (res < 0) {
    ERR("reply error to FUSE device failed. [%d]\n", errno);
  }
  fuse.waitForResponse = false;
}

struct fuse_attr
FuseResponseHandler::FuseSuccessRunnable::CreateAttrByMetadata(
                                          nsIEntryMetadata* aMeta)
{
  MOZ_ASSERT(aMeta);

  struct fuse_attr attr;
  bool isdir;
  uint64_t size;
  uint64_t mtime;
  aMeta->GetIsDirectory(&isdir);
  aMeta->GetSize(&size);
  aMeta->GetModificationTime(&mtime);

  if (isdir) {
    attr.size = 4096;
    attr.blocks = 8;
    attr.blksize = 512;
    attr.mode = 0x41ff;
  } else {
    attr.size = size;
    attr.blocks = size/512;
    attr.blksize = 512;
    attr.mode = 0x81fd;
  }
  if (mtime) {
    attr.atime = mtime/1000;
    attr.mtime = mtime/1000;
    attr.ctime = mtime/1000;
  } else {
    attr.atime = time(NULL);
    attr.mtime = attr.atime;
    attr.ctime = attr.atime;
  }
  attr.uid = 0;
  attr.gid = 1015;

  return attr;
}

// FuseErrorRunnable
FuseResponseHandler::FuseErrorRunnable::FuseErrorRunnable(
                                          FuseHandler* aFuseHandler,
                                          const uint32_t aRequestId,
                                          const uint32_t aError)
 : mRequestId(aRequestId),
   mError(aError),
   mHandler(aFuseHandler)
{
}

nsresult
FuseResponseHandler::FuseErrorRunnable::Run()
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(mHandler);
  MozFuse& fuse = mHandler->GetFuse();
  const struct fuse_in_header *hdr =
        (const struct fuse_in_header*)((void*)fuse.requestBuffer);
  struct fuse_out_header outhdr;
  outhdr.len = sizeof(outhdr);
  outhdr.error = mError;
  outhdr.unique = hdr->unique;
  int res = write(fuse.fuseFd, &outhdr, outhdr.len);
  if (res < 0) {
    ERR("reply error to FUSE device failed. [%d]\n", errno);
  }
  fuse.waitForResponse = false;
  return NS_OK;
}


} //end namespace virtualfilesystem
} //end namespace dom
} //end namespace mozilla
