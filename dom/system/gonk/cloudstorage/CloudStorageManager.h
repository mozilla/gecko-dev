/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_cloudstoragemanager_h__
#define mozilla_system_cloudstoragemanager_h__

#include "nsString.h"
#include "nsTArray.h"
#include "CloudStorage.h"

namespace mozilla {
namespace system {
namespace cloudstorage {

class CloudStorageManager
{
public:
  NS_INLINE_DECL_REFCOUNTING(CloudStorageManager)

  CloudStorageManager();
  ~CloudStorageManager();

  typedef nsTArray<RefPtr<CloudStorage>> CloudStorageArray;

  static CloudStorageArray::size_type NumCloudStorages();
  static TemporaryRef<CloudStorage> GetCloudStorage(CloudStorageArray::index_type aIndex);
  static TemporaryRef<CloudStorage> FindCloudStorageByName(const nsCSubstring& aName);
  static TemporaryRef<CloudStorage> FindAddCloudStorageByName(const nsCSubstring& aName);

  static bool StartCloudStorage(const nsCSubstring& aName);
  static bool StopCloudStorage(const nsCSubstring& aName);

private:
 
  CloudStorageArray mCloudStorageArray;
};

void InitCloudStorageManager();

} // end namespace cloudstorage
} // end namespace system
} // end namespace mozilla
#endif 
