/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_nsvirtualfilesystemservice_h__
#define mozilla_dom_nsvirtualfilesystemservice_h__

#include "mozilla/StaticPtr.h"
#include "mozilla/Monitor.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIVirtualFileSystemService.h"
#include "nsIVirtualFileSystem.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

/**
 *  nsVirtualFileSystemService
 *  The implementation of nsIVirtualFileSystemService. It is used to manage
 *  virtual file system in the Gecko.
 */

class nsVirtualFileSystemService final : public nsIVirtualFileSystemService
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIVIRTUALFILESYSTEMSERVICE

  typedef nsTArray<RefPtr<nsIVirtualFileSystem>> VirtualFileSystemArray;

public:
  static already_AddRefed<nsIVirtualFileSystemService> GetSingleton();

  nsVirtualFileSystemService();

  already_AddRefed<nsIVirtualFileSystem>
         FindVirtualFileSystemById(const nsAString& aName);
private:
  ~nsVirtualFileSystemService();

private:
  Monitor mArrayMonitor;
  VirtualFileSystemArray mVirtualFileSystemArray;
  static StaticRefPtr<nsIVirtualFileSystemService> sService;
};

} // end namespace virtualfilesystem
} // end namespace dom
} // end namespace mozilla
#endif
