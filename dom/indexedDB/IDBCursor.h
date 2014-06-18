/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbcursor_h__
#define mozilla_dom_indexeddb_idbcursor_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBCursorBinding.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#include "mozilla/dom/indexedDB/IDBObjectStore.h"
#include "mozilla/dom/indexedDB/Key.h"

class nsIRunnable;
class nsIScriptContext;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {
class OwningIDBObjectStoreOrIDBIndex;
}
}

BEGIN_INDEXEDDB_NAMESPACE

class ContinueHelper;
class ContinueObjectStoreHelper;
class ContinueIndexHelper;
class ContinueIndexObjectHelper;
class IDBIndex;
class IDBRequest;
class IDBTransaction;
class IndexedDBCursorChild;
class IndexedDBCursorParent;

class IDBCursor MOZ_FINAL : public nsISupports,
                            public nsWrapperCache
{
  friend class ContinueHelper;
  friend class ContinueObjectStoreHelper;
  friend class ContinueIndexHelper;
  friend class ContinueIndexObjectHelper;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBCursor)

  enum Type
  {
    OBJECTSTORE = 0,
    OBJECTSTOREKEY,
    INDEXKEY,
    INDEXOBJECT
  };

  enum Direction
  {
    NEXT = 0,
    NEXT_UNIQUE,
    PREV,
    PREV_UNIQUE,

    // Only needed for IPC serialization helper, should never be used in code.
    DIRECTION_INVALID
  };

  // For OBJECTSTORE cursors.
  static
  already_AddRefed<IDBCursor>
  Create(IDBRequest* aRequest,
         IDBTransaction* aTransaction,
         IDBObjectStore* aObjectStore,
         Direction aDirection,
         const Key& aRangeKey,
         const nsACString& aContinueQuery,
         const nsACString& aContinueToQuery,
         const Key& aKey,
         StructuredCloneReadInfo&& aCloneReadInfo);

  // For OBJECTSTOREKEY cursors.
  static
  already_AddRefed<IDBCursor>
  Create(IDBRequest* aRequest,
         IDBTransaction* aTransaction,
         IDBObjectStore* aObjectStore,
         Direction aDirection,
         const Key& aRangeKey,
         const nsACString& aContinueQuery,
         const nsACString& aContinueToQuery,
         const Key& aKey);

  // For INDEXKEY cursors.
  static
  already_AddRefed<IDBCursor>
  Create(IDBRequest* aRequest,
         IDBTransaction* aTransaction,
         IDBIndex* aIndex,
         Direction aDirection,
         const Key& aRangeKey,
         const nsACString& aContinueQuery,
         const nsACString& aContinueToQuery,
         const Key& aKey,
         const Key& aObjectKey);

  // For INDEXOBJECT cursors.
  static
  already_AddRefed<IDBCursor>
  Create(IDBRequest* aRequest,
         IDBTransaction* aTransaction,
         IDBIndex* aIndex,
         Direction aDirection,
         const Key& aRangeKey,
         const nsACString& aContinueQuery,
         const nsACString& aContinueToQuery,
         const Key& aKey,
         const Key& aObjectKey,
         StructuredCloneReadInfo&& aCloneReadInfo);

  IDBTransaction* Transaction() const
  {
    return mTransaction;
  }

  IDBRequest* Request() const
  {
    return mRequest;
  }

  static Direction
  ConvertDirection(IDBCursorDirection aDirection);

  void
  SetActor(IndexedDBCursorChild* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  SetActor(IndexedDBCursorParent* aActorParent)
  {
    NS_ASSERTION(!aActorParent || !mActorParent,
                 "Shouldn't have more than one!");
    mActorParent = aActorParent;
  }

  IndexedDBCursorChild*
  GetActorChild() const
  {
    return mActorChild;
  }

  IndexedDBCursorParent*
  GetActorParent() const
  {
    return mActorParent;
  }

  void
  ContinueInternal(const Key& aKey, int32_t aCount,
                   ErrorResult& aRv);

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  IDBTransaction*
  GetParentObject() const
  {
    return mTransaction;
  }

  void
  GetSource(OwningIDBObjectStoreOrIDBIndex& aSource) const;

  IDBCursorDirection
  GetDirection() const;

  void
  GetKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
         ErrorResult& aRv);

  void
  GetPrimaryKey(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Update(JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv);

  void
  Advance(uint32_t aCount, ErrorResult& aRv);

  void
  Continue(JSContext* aCx, JS::Handle<JS::Value> aKey, ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  Delete(JSContext* aCx, ErrorResult& aRv);

  void
  GetValue(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
           ErrorResult& aRv);

protected:
  IDBCursor();
  ~IDBCursor();

  void DropJSObjects();

  static
  already_AddRefed<IDBCursor>
  CreateCommon(IDBRequest* aRequest,
               IDBTransaction* aTransaction,
               IDBObjectStore* aObjectStore,
               Direction aDirection,
               const Key& aRangeKey,
               const nsACString& aContinueQuery,
               const nsACString& aContinueToQuery);

  nsRefPtr<IDBRequest> mRequest;
  nsRefPtr<IDBTransaction> mTransaction;
  nsRefPtr<IDBObjectStore> mObjectStore;
  nsRefPtr<IDBIndex> mIndex;

  JS::Heap<JSObject*> mScriptOwner;

  Type mType;
  Direction mDirection;
  nsCString mContinueQuery;
  nsCString mContinueToQuery;

  // These are cycle-collected!
  JS::Heap<JS::Value> mCachedKey;
  JS::Heap<JS::Value> mCachedPrimaryKey;
  JS::Heap<JS::Value> mCachedValue;

  Key mRangeKey;

  Key mKey;
  Key mObjectKey;
  StructuredCloneReadInfo mCloneReadInfo;
  Key mContinueToKey;

  IndexedDBCursorChild* mActorChild;
  IndexedDBCursorParent* mActorParent;

  bool mHaveCachedKey;
  bool mHaveCachedPrimaryKey;
  bool mHaveCachedValue;
  bool mRooted;
  bool mContinueCalled;
  bool mHaveValue;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbcursor_h__
