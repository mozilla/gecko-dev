/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IndexedDatabaseInlines_h
#define IndexedDatabaseInlines_h

#ifndef mozilla_dom_indexeddatabase_h__
#  error Must include IndexedDatabase.h first
#endif

#include "DatabaseFileInfo.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/File.h"

namespace mozilla::dom::indexedDB {

#ifdef NS_BUILD_REFCNT_LOGGING
inline StructuredCloneFileChild::StructuredCloneFileChild(
    StructuredCloneFileChild&& aOther)
    : StructuredCloneFileBase{std::move(aOther)},
      mContents{std::move(aOther.mContents)} {
  MOZ_COUNT_CTOR(StructuredCloneFileChild);
}
#endif

inline StructuredCloneFileChild::~StructuredCloneFileChild() {
  MOZ_COUNT_DTOR(StructuredCloneFileChild);
}

inline StructuredCloneFileChild::StructuredCloneFileChild(FileType aType)
    : StructuredCloneFileBase{aType}, mContents{Nothing()} {
  MOZ_COUNT_CTOR(StructuredCloneFileChild);
}

inline StructuredCloneFileChild::StructuredCloneFileChild(
    FileType aType, RefPtr<dom::Blob> aBlob)
    : StructuredCloneFileBase{aType}, mContents{std::move(aBlob)} {
  MOZ_ASSERT(eBlob == aType || eStructuredClone == aType);
  MOZ_ASSERT(mContents->as<RefPtr<dom::Blob>>());
  MOZ_COUNT_CTOR(StructuredCloneFileChild);
}

inline StructuredCloneFileParent::StructuredCloneFileParent(
    FileType aType, SafeRefPtr<DatabaseFileInfo> aFileInfo)
    : StructuredCloneFileBase{aType}, mContents{Some(std::move(aFileInfo))} {
  MOZ_ASSERT(**mContents);
  MOZ_COUNT_CTOR(StructuredCloneFileParent);
}

#ifdef NS_BUILD_REFCNT_LOGGING
inline StructuredCloneFileParent::StructuredCloneFileParent(
    StructuredCloneFileParent&& aOther)
    : StructuredCloneFileBase{std::move(aOther)},
      mContents{std::move(aOther.mContents)} {
  MOZ_COUNT_CTOR(StructuredCloneFileParent);
}
#endif

inline StructuredCloneFileParent::~StructuredCloneFileParent() {
  MOZ_COUNT_DTOR(StructuredCloneFileParent);
}

inline SafeRefPtr<DatabaseFileInfo> StructuredCloneFileParent::FileInfoPtr()
    const {
  return (*mContents)->clonePtr();
}

inline RefPtr<dom::Blob> StructuredCloneFileChild::BlobPtr() const {
  return mContents->as<RefPtr<dom::Blob>>();
}

template <typename StructuredCloneFile>
inline StructuredCloneReadInfo<StructuredCloneFile>::StructuredCloneReadInfo(
    JS::StructuredCloneScope aScope)
    : StructuredCloneReadInfoBase(JSStructuredCloneData{aScope}) {
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);
}

template <typename StructuredCloneFile>
inline StructuredCloneReadInfo<StructuredCloneFile>::StructuredCloneReadInfo()
    : StructuredCloneReadInfo(
          JS::StructuredCloneScope::DifferentProcessForIndexedDB) {}

template <typename StructuredCloneFile>
inline StructuredCloneReadInfo<StructuredCloneFile>::StructuredCloneReadInfo(
    JSStructuredCloneData&& aData, nsTArray<StructuredCloneFile> aFiles)
    : StructuredCloneReadInfoBase{std::move(aData)}, mFiles{std::move(aFiles)} {
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);
}

#ifdef NS_BUILD_REFCNT_LOGGING
template <typename StructuredCloneFile>
inline StructuredCloneReadInfo<StructuredCloneFile>::StructuredCloneReadInfo(
    StructuredCloneReadInfo&& aOther) noexcept
    : StructuredCloneReadInfoBase{std::move(aOther)},
      mFiles{std::move(aOther.mFiles)} {
  MOZ_COUNT_CTOR(StructuredCloneReadInfo);
}

template <typename StructuredCloneFile>
inline StructuredCloneReadInfo<
    StructuredCloneFile>::~StructuredCloneReadInfo() {
  MOZ_COUNT_DTOR(StructuredCloneReadInfo);
}

#endif

template <typename StructuredCloneFile>
inline size_t StructuredCloneReadInfo<StructuredCloneFile>::Size() const {
  // We do not check the structured clone size against
  // IndexedDatabaseManager::MaxStructuredCloneSize here.
  // The size has already been validated before being sent from the content
  // process to the parent process. At this stage, the parent process has
  // either stored the data in the database or written it to a separate file,
  // ensuring that size constraints are enforced elsewhere.
  //
  // If necessary, we could add a warning to flag oversized structured clones
  // here. However, failing the request entirely would require additional
  // changes, such as making these methods fallible, which would add complexity.

  // Serialized structured clone size in bytes. For structured clone sizes >
  // IPC::kMessageBufferShmemThreshold, only the size and shared memory handle
  // are included in the IPC message. The value 16 is an estimate.
  size_t size =
      Data().Size() > IPC::kMessageBufferShmemThreshold ? 16 : Data().Size();

  for (uint32_t i = 0, count = mFiles.Length(); i < count; ++i) {
    // We don't want to calculate the size of files and so on, because are
    // mainly file descriptors.
    size += sizeof(uint64_t);
  }

  return size;
}

inline StructuredCloneReadInfoChild::StructuredCloneReadInfoChild(
    JSStructuredCloneData&& aData, nsTArray<StructuredCloneFileChild> aFiles,
    IDBDatabase* aDatabase)
    : StructuredCloneReadInfo{std::move(aData), std::move(aFiles)},
      mDatabase{aDatabase} {}

template <typename E, typename Map>
RefPtr<DOMStringList> CreateSortedDOMStringList(const nsTArray<E>& aArray,
                                                const Map& aMap) {
  auto list = MakeRefPtr<DOMStringList>();

  if (!aArray.IsEmpty()) {
    nsTArray<nsString>& mapped = list->StringArray();
    mapped.SetCapacity(aArray.Length());

    std::transform(aArray.cbegin(), aArray.cend(), MakeBackInserter(mapped),
                   aMap);

    mapped.Sort();
  }

  return list;
}

template <typename StructuredCloneReadInfoType>
JSObject* StructuredCloneReadCallback(
    JSContext* const aCx, JSStructuredCloneReader* const aReader,
    const JS::CloneDataPolicy& aCloneDataPolicy, const uint32_t aTag,
    const uint32_t aData, void* const aClosure) {
  auto* const database = [aClosure]() -> IDBDatabase* {
    if constexpr (std::is_same_v<StructuredCloneReadInfoType,
                                 StructuredCloneReadInfoChild>) {
      return static_cast<StructuredCloneReadInfoChild*>(aClosure)->Database();
    }
    Unused << aClosure;
    return nullptr;
  }();
  return CommonStructuredCloneReadCallback(
      aCx, aReader, aCloneDataPolicy, aTag, aData,
      static_cast<StructuredCloneReadInfoType*>(aClosure), database);
}

template <typename T>
bool WrapAsJSObject(JSContext* const aCx, T& aBaseObject,
                    JS::MutableHandle<JSObject*> aResult) {
  JS::Rooted<JS::Value> wrappedValue(aCx);
  if (!ToJSValue(aCx, aBaseObject, &wrappedValue)) {
    return false;
  }

  aResult.set(&wrappedValue.toObject());
  return true;
}

}  // namespace mozilla::dom::indexedDB

#endif  // IndexedDatabaseInlines_h
