/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCloudStorageGeckoInterface.h"
#include "CloudStorageLog.h"
#include "CloudStorageManager.h"
#include "CloudStorage.h"

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
  CloudStorageResponseData resData;
  resData.IsDir = aIsDir;
  resData.FileSize = aSize;
  resData.MTime = aMTime;
  resData.CTime = aCTime;
  cloudStorage->SetResponseData(resData);
  cloudStorage->SetWaitForRequest(false);
  return NS_OK;
}

