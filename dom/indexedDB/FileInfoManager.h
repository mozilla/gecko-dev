/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INDEXEDDB_FILEINFOMANAGER_H_
#define DOM_INDEXEDDB_FILEINFOMANAGER_H_

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticMutex.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"
#include "nsISupportsImpl.h"
#include "FileInfo.h"
#include "FlippedOnce.h"

namespace mozilla::dom::indexedDB {

class FileInfoManagerBase {
 public:
  bool Invalidated() const { return mInvalidated; }

 protected:
  bool AssertValid() const {
    if (NS_WARN_IF(Invalidated())) {
      MOZ_ASSERT(false);
      return false;
    }

    return true;
  }

  void Invalidate() { mInvalidated.Flip(); }

 private:
  FlippedOnce<false> mInvalidated;
};

template <typename FileManager>
class FileInfoManager : public FileInfoManagerBase {
 public:
  using FileInfoType = FileInfo<FileManager>;
  using MutexType = StaticMutex;
  using AutoLockType = mozilla::detail::BaseAutoLock<MutexType&>;

  [[nodiscard]] SafeRefPtr<FileInfoType> GetFileInfo(int64_t aId) const {
    return AcquireFileInfo([this, aId]() MOZ_REQUIRES(Mutex()) {
      return mFileInfos.MaybeGet(aId);
    });
  }

  bool ContainsFileInfo(int64_t aFileId) {
    AutoLockType lock(Mutex());
    return mFileInfos.Contains(aFileId);
  }

  [[nodiscard]] SafeRefPtr<FileInfoType> CreateFileInfo(
      const Maybe<int64_t>& aMaybeId = Nothing(),
      const nsrefcnt aDBRefCnt = 0) {
    return AcquireFileInfo([this, &aMaybeId,
                            &aDBRefCnt]() MOZ_REQUIRES(Mutex()) {
      const int64_t id = aMaybeId.isSome() ? *aMaybeId : ++mLastFileId;

      auto fileInfo =
          MakeNotNull<FileInfoType*>(FileInfoManagerGuard{},
                                     SafeRefPtr{static_cast<FileManager*>(this),
                                                AcquireStrongRefFromRawPtr{}},
                                     id, aDBRefCnt);

      mFileInfos.InsertOrUpdate(id, fileInfo);

      if (aMaybeId.isSome()) {
        mLastFileId = std::max(id, mLastFileId);
      }

      return Some(fileInfo);
    });
  }

  void RemoveFileInfo(const int64_t aId, const AutoLockType& aFileMutexLock)
      MOZ_REQUIRES(Mutex()) {
#ifdef DEBUG
    aFileMutexLock.AssertOwns(Mutex());
#endif
    mFileInfos.Remove(aId);
  }

  // After calling this method, callers should not call any more methods on this
  // class.
  virtual nsresult Invalidate() {
    AutoLockType lock(Mutex());

    FileInfoManagerBase::Invalidate();

    mFileInfos.RemoveIf([](const auto& iter) {
      FileInfoType* info = iter.Data();
      MOZ_ASSERT(info);

      return !info->LockedClearDBRefs(FileInfoManagerGuard{});
    });

    return NS_OK;
  }

  struct FileInfoManagerGuard {
    FileInfoManagerGuard() = default;
  };

  static MutexType& Mutex() { return FileManager::MutexInstance(); }

 private:
  // Runs the given aFileInfoTableOp operation, which must return a FileInfo*,
  // under the FileManager lock, acquires a strong reference to the returned
  // object under the lock, and returns the strong reference.
  template <typename FileInfoTableOp>
  [[nodiscard]] SafeRefPtr<FileInfoType> AcquireFileInfo(
      const FileInfoTableOp& aFileInfoTableOp) const {
    if (!AssertValid()) {
      // In release, the assertions are disabled.
      return nullptr;
    }

    // We cannot simply change this to SafeRefPtr<FileInfo>, because
    // FileInfo::AddRef also acquires the FileManager::Mutex.
    auto fileInfo = [&aFileInfoTableOp]() -> RefPtr<FileInfoType> {
      AutoLockType lock(Mutex());

      const auto maybeFileInfo = aFileInfoTableOp();
      if (maybeFileInfo) {
        const auto& fileInfo = maybeFileInfo.ref();
        fileInfo->LockedAddRef();
        return dont_AddRef(fileInfo.get());
      }

      return {};
    }();

    return SafeRefPtr{std::move(fileInfo)};
  }

  // Access to the following private fields must be protected by
  // FileManager::Mutex() which is now enforced by MOZ_GUARDED_BY annotations.
  nsTHashMap<nsUint64HashKey, NotNull<FileInfoType*>> mFileInfos
      MOZ_GUARDED_BY(Mutex());

  int64_t mLastFileId MOZ_GUARDED_BY(Mutex()) = 0;

 protected:
#ifdef DEBUG
  ~FileInfoManager() { MOZ_ASSERT(mFileInfos.IsEmpty()); }
#else
  ~FileInfoManager() = default;
#endif
};

}  // namespace mozilla::dom::indexedDB

#endif  // DOM_INDEXEDDB_FILEINFOMANAGER_H_
