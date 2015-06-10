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
nsCloudStorageGeckoInterface::FinishRequest(const nsACString_internal& aCloudName)
{
  nsCString cloudName(aCloudName);
  RefPtr<CloudStorage> cloudStorage = CloudStorageManager::FindCloudStorageByName(cloudName);
  cloudStorage->SetWaitForRequest(false);
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
  cloudStorage->SetAttrByPath(path, aIsDir, aSize, aMTime, aCTime);
  return NS_OK;
}

NS_IMETHODIMP
nsCloudStorageGeckoInterface::SetFileList(const nsACString_internal& aCloudName,
                                          const nsACString_internal& aPath,
                                          const nsACString_internal& aChildPath,
                                          bool aIsDir,
                                          uint64_t aSize,
                                          uint64_t aMTime,
                                          uint64_t aCTime)
{
  nsCString cloudName(aCloudName);
  nsCString path(aPath);
  nsCString childPath(aChildPath);
  RefPtr<CloudStorage> cloudStorage = CloudStorageManager::FindCloudStorageByName(cloudName);
  nsCString entry;
  if (path.Equals(NS_LITERAL_CSTRING("/"))) {
    childPath.Right(entry, childPath.Length()-path.Length());
  } else {
    childPath.Right(entry, childPath.Length()-path.Length()-1);
  }
  cloudStorage->SetAttrByPath(childPath, aIsDir, aSize, aMTime, aCTime);
  cloudStorage->AddEntryByPath(path, entry);
  return NS_OK;
}

NS_IMETHODIMP
nsCloudStorageGeckoInterface::SetData(const nsACString_internal& aCloudName,
                                      uint8_t *aBuffer,
                                      uint32_t aSize)
{
  nsCString cloudName(aCloudName);
  char* buffer = (char*) malloc(sizeof(char) * aSize);
  memset(buffer, 0, aSize);
  memcpy(buffer, aBuffer, aSize);
  RefPtr<CloudStorage> cloudStorage = CloudStorageManager::FindCloudStorageByName(cloudName);
  cloudStorage->SetDataBuffer(buffer, aSize);
  free(buffer);
  return NS_OK;
}
