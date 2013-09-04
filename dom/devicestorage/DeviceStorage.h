/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DeviceStorage_h
#define DeviceStorage_h

#include "nsIDOMDeviceStorage.h"
#include "nsIFile.h"
#include "nsIPrincipal.h"
#include "nsIObserver.h"
#include "nsDOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "DOMRequest.h"

class nsIInputStream;

#define DEVICESTORAGE_PICTURES   "pictures"
#define DEVICESTORAGE_VIDEOS     "videos"
#define DEVICESTORAGE_MUSIC      "music"
#define DEVICESTORAGE_APPS       "apps"
#define DEVICESTORAGE_SDCARD     "sdcard"

class DeviceStorageFile MOZ_FINAL
  : public nsISupports {
public:
  nsCOMPtr<nsIFile> mFile;
  nsString mStorageType;
  nsString mStorageName;
  nsString mRootDir;
  nsString mPath;
  bool mEditable;
  nsString mMimeType;
  uint64_t mLength;
  uint64_t mLastModifiedDate;

  // Used when the path will be set later via SetPath.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName);
  // Used for non-enumeration purposes.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName,
                    const nsAString& aPath);
  // Used for enumerations. When you call Enumerate, you can pass in a directory to enumerate
  // and the results that are returned are relative to that directory, files related to an
  // enumeration need to know the "root of the enumeration" directory.
  DeviceStorageFile(const nsAString& aStorageType,
                    const nsAString& aStorageName,
                    const nsAString& aRootDir,
                    const nsAString& aPath);

  void SetPath(const nsAString& aPath);
  void SetEditable(bool aEditable);

  static already_AddRefed<DeviceStorageFile> CreateUnique(nsAString& aFileName,
                                                          uint32_t aFileType,
                                                          uint32_t aFileAttributes);
  NS_DECL_ISUPPORTS

  bool IsAvailable();
  void GetFullPath(nsAString& aCompositePath);

  // we want to make sure that the names of file can't reach
  // outside of the type of storage the user asked for.
  bool IsSafePath();
  bool IsSafePath(const nsAString& aPath);

  void Dump(const char* label);

  nsresult Remove();
  nsresult Write(nsIInputStream* aInputStream);
  nsresult Write(InfallibleTArray<uint8_t>& bits);
  void CollectFiles(nsTArray<nsRefPtr<DeviceStorageFile> >& aFiles,
                    PRTime aSince = 0);
  void collectFilesInternal(nsTArray<nsRefPtr<DeviceStorageFile> >& aFiles,
                            PRTime aSince, nsAString& aRootPath);

  void AccumDiskUsage(uint64_t* aPicturesSoFar, uint64_t* aVideosSoFar,
                      uint64_t* aMusicSoFar, uint64_t* aTotalSoFar);

  void GetDiskFreeSpace(int64_t* aSoFar);
  void GetStatus(nsAString& aStatus);
  static void GetRootDirectoryForType(const nsAString& aStorageType,
                                      const nsAString& aStorageName,
                                      nsIFile** aFile);

  nsresult CalculateSizeAndModifiedDate();
  nsresult CalculateMimeType();

private:
  void Init();
  void NormalizeFilePath();
  void AppendRelativePath(const nsAString& aPath);
  void AccumDirectoryUsage(nsIFile* aFile,
                           uint64_t* aPicturesSoFar,
                           uint64_t* aVideosSoFar,
                           uint64_t* aMusicSoFar,
                           uint64_t* aTotalSoFar);
};

class FileUpdateDispatcher MOZ_FINAL
  : public nsIObserver
{
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static FileUpdateDispatcher* GetSingleton();
 private:
  static mozilla::StaticRefPtr<FileUpdateDispatcher> sSingleton;
};

class nsDOMDeviceStorage MOZ_FINAL
  : public nsIDOMDeviceStorage
  , public nsDOMEventTargetHelper
  , public nsIObserver
{
public:
  typedef nsTArray<nsString> VolumeNameArray;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDEVICESTORAGE

  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTTARGET
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMDeviceStorage, nsDOMEventTargetHelper)

  nsDOMDeviceStorage();

  nsresult Init(nsPIDOMWindow* aWindow, const nsAString& aType,
                const nsAString& aVolName);

  bool IsAvailable();
  bool IsFullPath(const nsAString& aPath) { return aPath.Length() > 0 && aPath.CharAt(0) == '/'; }

  void SetRootDirectoryForType(const nsAString& aType, const nsAString& aVolName);

  static void CreateDeviceStorageFor(nsPIDOMWindow* aWin,
                                     const nsAString& aType,
                                     nsDOMDeviceStorage** aStore);

  static void CreateDeviceStoragesFor(nsPIDOMWindow* aWin,
                                      const nsAString& aType,
                                      nsTArray<nsRefPtr<nsDOMDeviceStorage> >& aStores);
  void Shutdown();

  static void GetOrderedVolumeNames(nsTArray<nsString>& aVolumeNames);

  static void GetDefaultStorageName(const nsAString& aStorageType,
                                    nsAString &aStorageName);

  static bool ParseFullPath(const nsAString& aFullPath,
                            nsAString& aOutStorageName,
                            nsAString& aOutStoragePath);
private:
  ~nsDOMDeviceStorage();

  nsresult GetInternal(const JS::Value & aName,
                       JSContext* aCx,
                       nsIDOMDOMRequest** aRetval,
                       bool aEditable);

  nsresult GetInternal(nsPIDOMWindow* aWin,
                       const nsAString& aPath,
                       mozilla::dom::DOMRequest* aRequest,
                       bool aEditable);

  nsresult DeleteInternal(nsPIDOMWindow* aWin,
                          const nsAString& aPath,
                          mozilla::dom::DOMRequest* aRequest);

  nsresult EnumerateInternal(const JS::Value& aName,
                             const JS::Value& aOptions,
                             JSContext* aCx,
                             uint8_t aArgc,
                             bool aEditable,
                             nsIDOMDeviceStorageCursor** aRetval);

  nsString mStorageType;
  nsCOMPtr<nsIFile> mRootDirectory;
  nsString mStorageName;
  already_AddRefed<nsDOMDeviceStorage> GetStorage(const nsAString& aFullPath,
                                                  nsAString& aOutStoragePath);
  already_AddRefed<nsDOMDeviceStorage> GetStorageByName(const nsAString &aStorageName);

  nsCOMPtr<nsIPrincipal> mPrincipal;

  bool mIsWatchingFile;
  bool mAllowedToWatchFile;

  nsresult Notify(const char* aReason, class DeviceStorageFile* aFile);

  friend class WatchFileEvent;
  friend class DeviceStorageRequest;

  class VolumeNameCache : public mozilla::RefCounted<VolumeNameCache>
  {
  public:
    nsTArray<nsString>  mVolumeNames;
  };
  static mozilla::StaticRefPtr<VolumeNameCache> sVolumeNameCache;

#ifdef MOZ_WIDGET_GONK
  nsString mLastStatus;
  void DispatchMountChangeEvent(nsAString& aVolumeStatus);
#endif

  // nsIDOMDeviceStorage.type
  enum {
      DEVICE_STORAGE_TYPE_DEFAULT = 0,
      DEVICE_STORAGE_TYPE_SHARED,
      DEVICE_STORAGE_TYPE_EXTERNAL
  };
};

#endif
