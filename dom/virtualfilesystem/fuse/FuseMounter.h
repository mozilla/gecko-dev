/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_virtualfilesystem_fusemounter_h__
#define mozilla_dom_virtualfilesystem_fusemounter_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsIVirtualFileSystemCallback.h"
#include "FuseHandler.h"

namespace mozilla {
namespace dom {
namespace virtualfilesystem {

class VirtualFileSystemVolumeRequest final : public nsRunnable
{
public:
  NS_INLINE_DECL_REFCOUNTING(VirtualFileSystemVolumeRequest)

  enum eRequestType {
    CREATEFAKEVOLUME = 0,
    REMOVEFAKEVOLUME = 1
  };
  VirtualFileSystemVolumeRequest(const eRequestType aType, const nsAString& aName,
                            const nsAString& aMountPoint);
  nsresult Run();
private:
  ~VirtualFileSystemVolumeRequest() = default;

  eRequestType mType;
  nsString mVolumeName;
  nsString mMountPoint;
};

class FuseMounter final
{
public:
  NS_INLINE_DECL_REFCOUNTING(FuseMounter)

  FuseMounter(FuseHandler* aFuseHanlder);
  void Mount(nsIVirtualFileSystemCallback* aCallback,
             const uint32_t aRequestId);
  void Unmount(nsIVirtualFileSystemCallback* aCallback,
               const uint32_t aRequestId);
private:
  class FuseMountRunnable final : public nsRunnable
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(FuseInitRunnable)

    FuseMountRunnable(FuseHandler* aFuseHandler,
                     nsIVirtualFileSystemCallback* aCallback,
                     const uint32_t aRequestId);

    nsresult Run();

  private:
    ~FuseMountRunnable() = default;

    bool CheckMountPoint();

    RefPtr<FuseHandler> mHandler;
    RefPtr<nsIVirtualFileSystemCallback> mCallback;
    uint32_t mRequestId;
  };

  class FuseUnmountRunnable final : public nsRunnable
  {
  public:
    NS_INLINE_DECL_REFCOUNTING(FuseCloseRunnable)

    FuseUnmountRunnable(FuseHandler* aFuseHandler,
                      nsIVirtualFileSystemCallback* aCallback,
                      const uint32_t aRequestId);

    nsresult Run();

  private:
    ~FuseUnmountRunnable() = default;

    RefPtr<FuseHandler> mHandler;
    RefPtr<nsIVirtualFileSystemCallback> mCallback;
    uint32_t mRequestId;
  };

  ~FuseMounter() = default;

  RefPtr<FuseHandler> mHandler;
};


} // end namespace virtualfilesystem
} // end namespace dom
} // end namespace mozilla
#endif
