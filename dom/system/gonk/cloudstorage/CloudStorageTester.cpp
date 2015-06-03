#include "CloudStorageTester.h"
#include "CloudStorageLog.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

namespace mozilla {
namespace system {
namespace cloudstorage {

nsDataHashtable<nsUint64HashKey, int> CloudStorageTester::sFileHash;

CloudStorageTester::CloudStorageTester()
{
}

CloudStorageTester::~CloudStorageTester()
{

}

FuseAttr
CloudStorageTester::GetAttrByPath(const nsCString& aPath, const uint64_t aNodeId)
{
  FuseAttr attr;
  nsCString realPath = NS_LITERAL_CSTRING("/data/local/tmp/cloudstorage");
  if (!aPath.Equals(NS_LITERAL_CSTRING("/"))) {
    realPath.Append(aPath);
  }
  
  LOG("path: %s, real path: %s", aPath.get(), realPath.get());
  struct stat s;
  if (lstat(realPath.get(), &s) < 0) {
    LOG("lstat fail");
    switch (errno) {
      case EACCES: LOG("EACCES"); break;
      case EBADF: LOG("EBADF"); break;
      case EFAULT: LOG("EFAULG"); break;
      case ENAMETOOLONG: LOG("ENAMETOOLONG"); break;
      case ENOENT: LOG("ENOENT"); break;
      case ENOMEM: LOG("ENOMEM"); break;
      case ENOTDIR: LOG("ENOTDIR"); break;
      default: LOG("unknown errno %d", errno); break;
    }
  }
  attr.ino = aNodeId;
  attr.size = s.st_size;
  attr.blocks = s.st_blocks;
  attr.atime = s.st_atime;
  attr.mtime = s.st_mtime;
  attr.ctime = s.st_ctime;
  attr.atimensec = s.st_atime_nsec;
  attr.mtimensec = s.st_mtime_nsec;
  attr.ctimensec = s.st_ctime_nsec;
  attr.mode = s.st_mode;
  attr.nlink = s.st_nlink;
  attr.uid = 0;
  attr.gid = 1015;
  return attr;
}

void 
CloudStorageTester::GetEntry(const nsCString& aPath, const uint64_t aOffset, nsCString& aEntryName, uint32_t& aEntryType)
{
  LOG("path: %s, offset: %d", aPath.get(), (unsigned int)aOffset);
  if (aPath.Equals(NS_LITERAL_CSTRING("/"))) {
    if (aOffset == 0) {
      aEntryName = NS_LITERAL_CSTRING("A");
      aEntryType = DT_DIR;
    }
  } else if (aPath.Equals(NS_LITERAL_CSTRING("/A"))) {
    switch (aOffset) {
      case 0: {
        aEntryName = NS_LITERAL_CSTRING("B");
	aEntryType = DT_DIR;
        break;
      }
      case 1: {
        aEntryName = NS_LITERAL_CSTRING("c.jpg");
	aEntryType = DT_REG;
        break;
      }
      default: break;
    }
  } else if (aPath.Equals(NS_LITERAL_CSTRING("/A/B"))) {
    if (aOffset == 0) {
      aEntryName = NS_LITERAL_CSTRING("d.jpg");
      aEntryType = DT_REG;
    }
  }
}

void
CloudStorageTester::GetData(const uint64_t aHandle, const uint32_t aSize, const uint64_t aOffset, char*& aData, int32_t& aActualSize)
{
  int fd = -1;
  LOG("search fd for handle %llx", aHandle);
  LOG("hash table count: %d", sFileHash.Count());
  if (!sFileHash.Get(aHandle, &fd)) {
    LOG("can not find fd(%d) for handle %llx", fd, aHandle);
    return;
  }
  LOG("Size: %d, Offset: %d", aSize, (unsigned int)aOffset);
  aActualSize = pread64(fd, aData, aSize, aOffset);
  LOG("actual size: %d", aActualSize);
}

void
CloudStorageTester::Open(const nsCString& aPath, const uint64_t aHandle)
{
  int fd;
  nsCString realPath = NS_LITERAL_CSTRING("/data/local/tmp/cloudstorage");
  realPath.Append(aPath);
  if (!sFileHash.Get(aHandle, &fd)) {
    if((fd = open(realPath.get(), O_RDWR)) < 0) {
      LOG("fail to open fd for path %s(%llx)", aPath.get(), aHandle);
      return;
    } else {
      LOG("path: %s(%llx), fd: %d", aPath.get(), aHandle, fd);
      sFileHash.Put(aHandle, fd);
      LOG("hash table count: %d", sFileHash.Count());
    }
  }
}

void
CloudStorageTester::Close(const uint64_t aHandle)
{
  int fd;
  LOG("search fd for handle %llx", aHandle);
  if (!sFileHash.Get(aHandle, &fd)) {
    return;
  }
  close(fd);
  sFileHash.Remove(aHandle);
}

}
}
}
