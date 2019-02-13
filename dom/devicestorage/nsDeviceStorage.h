/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceStorage_h
#define nsDeviceStorage_h

class nsPIDOMWindow;
#include "mozilla/Attributes.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestChild.h"

#include "DOMRequest.h"
#include "DOMCursor.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMClassInfoID.h"
#include "nsIClassInfo.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsWeakPtr.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsIStringBundle.h"
#include "mozilla/Mutex.h"
#include "prtime.h"
#include "DeviceStorage.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class Blob;
}
} // namespace mozilla

#define POST_ERROR_EVENT_FILE_EXISTS                 "NoModificationAllowedError"
#define POST_ERROR_EVENT_FILE_DOES_NOT_EXIST         "NotFoundError"
#define POST_ERROR_EVENT_FILE_NOT_ENUMERABLE         "TypeMismatchError"
#define POST_ERROR_EVENT_PERMISSION_DENIED           "SecurityError"
#define POST_ERROR_EVENT_ILLEGAL_TYPE                "TypeMismatchError"
#define POST_ERROR_EVENT_UNKNOWN                     "Unknown"

enum DeviceStorageRequestType {
    DEVICE_STORAGE_REQUEST_READ,
    DEVICE_STORAGE_REQUEST_WRITE,
    DEVICE_STORAGE_REQUEST_APPEND,
    DEVICE_STORAGE_REQUEST_CREATE,
    DEVICE_STORAGE_REQUEST_DELETE,
    DEVICE_STORAGE_REQUEST_WATCH,
    DEVICE_STORAGE_REQUEST_FREE_SPACE,
    DEVICE_STORAGE_REQUEST_USED_SPACE,
    DEVICE_STORAGE_REQUEST_AVAILABLE,
    DEVICE_STORAGE_REQUEST_STATUS,
    DEVICE_STORAGE_REQUEST_FORMAT,
    DEVICE_STORAGE_REQUEST_MOUNT,
    DEVICE_STORAGE_REQUEST_UNMOUNT,
    DEVICE_STORAGE_REQUEST_CREATEFD
};

class DeviceStorageUsedSpaceCache final
{
public:
  static DeviceStorageUsedSpaceCache* CreateOrGet();

  DeviceStorageUsedSpaceCache();
  ~DeviceStorageUsedSpaceCache();


  class InvalidateRunnable final : public nsRunnable
  {
    public:
      InvalidateRunnable(DeviceStorageUsedSpaceCache* aCache, 
                         const nsAString& aStorageName)
        : mCache(aCache)
        , mStorageName(aStorageName) {}

      ~InvalidateRunnable() {}

      NS_IMETHOD Run() override
      {
        nsRefPtr<DeviceStorageUsedSpaceCache::CacheEntry> cacheEntry;
        cacheEntry = mCache->GetCacheEntry(mStorageName);
        if (cacheEntry) {
          cacheEntry->mDirty = true;
        }
        return NS_OK;
      }
    private:
      DeviceStorageUsedSpaceCache* mCache;
      nsString mStorageName;
  };

  void Invalidate(const nsAString& aStorageName)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mIOThread);

    nsRefPtr<InvalidateRunnable> r = new InvalidateRunnable(this, aStorageName);
    mIOThread->Dispatch(r, NS_DISPATCH_NORMAL);
  }

  void Dispatch(nsIRunnable* aRunnable)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mIOThread);

    mIOThread->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
  }

  nsresult AccumUsedSizes(const nsAString& aStorageName,
                          uint64_t* aPictureSize, uint64_t* aVideosSize,
                          uint64_t* aMusicSize, uint64_t* aTotalSize);

  void SetUsedSizes(const nsAString& aStorageName,
                    uint64_t aPictureSize, uint64_t aVideosSize,
                    uint64_t aMusicSize, uint64_t aTotalSize);

private:
  friend class InvalidateRunnable;

  struct CacheEntry
  {
    // Technically, this doesn't need to be threadsafe, but the implementation
    // of the non-thread safe one causes ASSERTS due to the underlying thread
    // associated with a LazyIdleThread changing from time to time.
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheEntry)

    bool mDirty;
    nsString mStorageName;
    int64_t  mFreeBytes;
    uint64_t mPicturesUsedSize;
    uint64_t mVideosUsedSize;
    uint64_t mMusicUsedSize;
    uint64_t mTotalUsedSize;

  private:
    ~CacheEntry() {}
  };
  already_AddRefed<CacheEntry> GetCacheEntry(const nsAString& aStorageName);

  nsTArray<nsRefPtr<CacheEntry>> mCacheEntries;

  nsCOMPtr<nsIThread> mIOThread;

  static mozilla::StaticAutoPtr<DeviceStorageUsedSpaceCache> sDeviceStorageUsedSpaceCache;
};

class DeviceStorageTypeChecker final
{
public:
  static DeviceStorageTypeChecker* CreateOrGet();

  DeviceStorageTypeChecker();
  ~DeviceStorageTypeChecker();

  void InitFromBundle(nsIStringBundle* aBundle);

  bool Check(const nsAString& aType, mozilla::dom::Blob* aBlob);
  bool Check(const nsAString& aType, nsIFile* aFile);
  bool Check(const nsAString& aType, const nsString& aPath);
  void GetTypeFromFile(nsIFile* aFile, nsAString& aType);
  void GetTypeFromFileName(const nsAString& aFileName, nsAString& aType);

  static nsresult GetPermissionForType(const nsAString& aType, nsACString& aPermissionResult);
  static nsresult GetAccessForRequest(const DeviceStorageRequestType aRequestType, nsACString& aAccessResult);
  static bool IsVolumeBased(const nsAString& aType);
  static bool IsSharedMediaRoot(const nsAString& aType);

private:
  nsString mPicturesExtensions;
  nsString mVideosExtensions;
  nsString mMusicExtensions;

  static mozilla::StaticAutoPtr<DeviceStorageTypeChecker> sDeviceStorageTypeChecker;
};

class ContinueCursorEvent final : public nsRunnable
{
public:
  explicit ContinueCursorEvent(already_AddRefed<mozilla::dom::DOMRequest> aRequest);
  explicit ContinueCursorEvent(mozilla::dom::DOMRequest* aRequest);
  ~ContinueCursorEvent();
  void Continue();

  NS_IMETHOD Run() override;
private:
  already_AddRefed<DeviceStorageFile> GetNextFile();
  nsRefPtr<mozilla::dom::DOMRequest> mRequest;
};

class nsDOMDeviceStorageCursor final
  : public mozilla::dom::DOMCursor
  , public nsIContentPermissionRequest
  , public mozilla::dom::devicestorage::DeviceStorageRequestChildCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_FORWARD_NSIDOMDOMCURSOR(mozilla::dom::DOMCursor::)

  // DOMCursor
  virtual void Continue(mozilla::ErrorResult& aRv) override;

  nsDOMDeviceStorageCursor(nsPIDOMWindow* aWindow,
                           nsIPrincipal* aPrincipal,
                           DeviceStorageFile* aFile,
                           PRTime aSince);


  nsTArray<nsRefPtr<DeviceStorageFile> > mFiles;
  bool mOkToCallContinue;
  PRTime mSince;
  size_t mIndex;

  void GetStorageType(nsAString & aType);

  void RequestComplete() override;

private:
  ~nsDOMDeviceStorageCursor();

  nsRefPtr<DeviceStorageFile> mFile;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
};

//helpers
bool
StringToJsval(nsPIDOMWindow* aWindow, nsAString& aString,
              JS::MutableHandle<JS::Value> result);

JS::Value
nsIFileToJsval(nsPIDOMWindow* aWindow, DeviceStorageFile* aFile);

JS::Value
InterfaceToJsval(nsPIDOMWindow* aWindow, nsISupports* aObject, const nsIID* aIID);

#endif
