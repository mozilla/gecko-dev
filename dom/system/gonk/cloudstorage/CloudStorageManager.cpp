/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CloudStorageManager.h"
#include "CloudStorageLog.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace system {
namespace cloudstorage {

static StaticRefPtr<CloudStorageManager> sCloudStorageManager;

CloudStorageManager::CloudStorageManager()
{
  LOG("CloudStorageManager constructor called");
}

CloudStorageManager::~CloudStorageManager()
{
}

//static
size_t
CloudStorageManager::NumCloudStorages()
{
  if (!sCloudStorageManager) {
    return 0;
  }
  return sCloudStorageManager->mCloudStorageArray.Length();
}

//static
TemporaryRef<CloudStorage>
CloudStorageManager::GetCloudStorage(size_t aIndex)
{
  MOZ_ASSERT(aIndex < NumCloudStorages());
  return sCloudStorageManager->mCloudStorageArray[aIndex];
}

//static
TemporaryRef<CloudStorage>
CloudStorageManager::FindCloudStorageByName(const nsCSubstring& aName)
{
  if (!sCloudStorageManager) {
    return nullptr;
  }
  CloudStorageArray::size_type  numCloudStorages = NumCloudStorages();
  CloudStorageArray::index_type csIndex;
  for (csIndex = 0; csIndex < numCloudStorages; csIndex++) {
    RefPtr<CloudStorage> cs = GetCloudStorage(csIndex);
    if (cs->Name().Equals(aName)) {
      return cs;
    }
  }
  return nullptr;
}

//static
TemporaryRef<CloudStorage>
CloudStorageManager::FindAddCloudStorageByName(const nsCSubstring& aName)
{
  RefPtr<CloudStorage> cs = FindCloudStorageByName(aName);
  if (cs) {
    return cs;
  }
  // No cloudstorage found, create and add a new one.
  cs = new CloudStorage(nsCString(aName));
  sCloudStorageManager->mCloudStorageArray.AppendElement(cs);
  return cs;
}

//static
bool
CloudStorageManager::StartCloudStorage(const nsCSubstring& aName)
{
  RefPtr<CloudStorage> cs = FindCloudStorageByName(aName);
  if (!cs) {
    LOG("Specified cloud storage '%s' does not exist.", nsCString(aName).get());
    return false;
  }
  if (cs->State() != CloudStorage::STATE_READY) {
    LOG("Specified cloud storage already executed.");
    return false;
  }
  cs->StartStorage();
  return true;
}

//static
bool
CloudStorageManager::StopCloudStorage(const nsCSubstring& aName)
{
  RefPtr<CloudStorage> cs = FindCloudStorageByName(aName);
  if (!cs) {
    LOG("Specified cloud storage '%s' does not exist.", nsCString(aName).get());
    return false;
  }
  if (cs->State() != CloudStorage::STATE_RUNNING) {
    LOG("Specified cloud storage already stopped.");
    return false;
  }
  cs->StopStorage();
  return true;
}

void
InitCloudStorageManager()
{
  MOZ_ASSERT(!sCloudStorageManager);
  sCloudStorageManager = new CloudStorageManager();
}

} // end namespace cloudstorage
} // end namespace system
} // end namespace mozilla
