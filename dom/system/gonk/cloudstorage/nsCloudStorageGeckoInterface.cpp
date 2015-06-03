/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCloudStorageGeckoInterface.h"
#include "CloudStorageLog.h"
#include "CloudStorageManager.h"
#include "CloudStorage.h"
#include "fuse.h"
#include <time.h>

using namespace mozilla::system::cloudstorage;
using namespace mozilla;

NS_IMPL_ISUPPORTS(nsCloudStorageGeckoInterface, nsICloudStorageGeckoInterface)

nsCloudStorageGeckoInterface::nsCloudStorageGeckoInterface()
{
}

nsCloudStorageGeckoInterface::~nsCloudStorageGeckoInterface()
{
}

nsresult
nsCloudStorageGeckoInterface::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsCloudStorageGeckoInterface::SetFileMeta(const nsACString_internal& aCloudName,
                                          const nsACString_internal& aPath,
                                          bool aIsDir,
                                          uint64_t aSize,
                                          uint64_t aMTime,
                                          uint64_t aCTime)
{
  nsCString cloudName(aCloudName);
  nsCString path(aPath);
  RefPtr<CloudStorage> cloudStorage = CloudStorageManager::FindCloudStorageByName(cloudName);
  LOG("in CloudStorageGeckoInterface::SetFileMeta");
  LOG("file type: %s, size: %llu, modified time: %llu, created time: %llu", aIsDir?"Directory":"File", aSize, aMTime, aCTime);

  FuseAttr attr;
  if (aIsDir) {
    attr.size = 4096;
    attr.blocks = 8;
    attr.blksize = 512;
    attr.mode = 0x41ff;
  } else {
    attr.size = aSize;
    attr.blocks = aSize/512;
    attr.blksize = 512;
    attr.mode = 0x81fd;
  }
  if (aMTime != 0 && aCTime != 0) {
    attr.atime = aMTime/1000;
    attr.mtime = aMTime/1000;
    attr.ctime = aCTime/1000;
  } else {
    attr.atime = time(NULL);
    attr.mtime = attr.atime;
    attr.ctime = attr.ctime;
  }
  attr.uid = 0;
  attr.gid = 1015;

  cloudStorage->SetAttrByPath(path, attr);
  cloudStorage->SetWaitForRequest(false);
  return NS_OK;
}

