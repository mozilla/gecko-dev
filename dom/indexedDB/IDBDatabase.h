/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbdatabase_h__
#define mozilla_dom_indexeddb_idbdatabase_h__

#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBTransactionBinding.h"
#include "mozilla/dom/StorageTypeBinding.h"
#include "mozilla/dom/indexedDB/IDBWrapperCache.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "nsAutoPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsString.h"
#include "nsTHashtable.h"

class nsIDocument;
class nsPIDOMWindow;

namespace mozilla {

class ErrorResult;
class EventChainPostVisitor;

namespace dom {

class Blob;
class DOMStringList;
struct IDBObjectStoreParameters;
template <class> class Optional;
class StringOrStringSequence;

namespace indexedDB {

class BackgroundDatabaseChild;
class DatabaseSpec;
class FileManager;
class IDBFactory;
class IDBMutableFile;
class IDBObjectStore;
class IDBRequest;
class IDBTransaction;
class PBackgroundIDBDatabaseFileChild;

class IDBDatabase final
  : public IDBWrapperCache
{
  typedef mozilla::dom::StorageType StorageType;
  typedef mozilla::dom::quota::PersistenceType PersistenceType;

  class LogWarningRunnable;
  friend class LogWarningRunnable;

  class Observer;
  friend class Observer;

  // The factory must be kept alive when IndexedDB is used in multiple
  // processes. If it dies then the entire actor tree will be destroyed with it
  // and the world will explode.
  nsRefPtr<IDBFactory> mFactory;

  nsAutoPtr<DatabaseSpec> mSpec;

  // Normally null except during a versionchange transaction.
  nsAutoPtr<DatabaseSpec> mPreviousSpec;

  nsRefPtr<FileManager> mFileManager;

  BackgroundDatabaseChild* mBackgroundActor;

  nsTHashtable<nsPtrHashKey<IDBTransaction>> mTransactions;

  nsDataHashtable<nsISupportsHashKey, PBackgroundIDBDatabaseFileChild*>
    mFileActors;

  nsTHashtable<nsISupportsHashKey> mReceivedBlobs;

  nsRefPtr<Observer> mObserver;

  // Weak refs, IDBMutableFile strongly owns this IDBDatabase object.
  nsTArray<IDBMutableFile*> mLiveMutableFiles;

  bool mClosed;
  bool mInvalidated;

public:
  static already_AddRefed<IDBDatabase>
  Create(IDBWrapperCache* aOwnerCache,
         IDBFactory* aFactory,
         BackgroundDatabaseChild* aActor,
         DatabaseSpec* aSpec);

  void
  AssertIsOnOwningThread() const
#ifdef DEBUG
  ;
#else
  { }
#endif

  const nsString&
  Name() const;

  void
  GetName(nsAString& aName) const
  {
    AssertIsOnOwningThread();

    aName = Name();
  }

  uint64_t
  Version() const;

  already_AddRefed<nsIDocument>
  GetOwnerDocument() const;

  void
  Close()
  {
    AssertIsOnOwningThread();

    CloseInternal();
  }

  bool
  IsClosed() const
  {
    AssertIsOnOwningThread();

    return mClosed;
  }

  void
  Invalidate();

  // Whether or not the database has been invalidated. If it has then no further
  // transactions for this database will be allowed to run.
  bool
  IsInvalidated() const
  {
    AssertIsOnOwningThread();

    return mInvalidated;
  }

  void
  EnterSetVersionTransaction(uint64_t aNewVersion);

  void
  ExitSetVersionTransaction();

  // Called when a versionchange transaction is aborted to reset the
  // DatabaseInfo.
  void
  RevertToPreviousState();

  IDBFactory*
  Factory() const
  {
    AssertIsOnOwningThread();

    return mFactory;
  }

  void
  RegisterTransaction(IDBTransaction* aTransaction);

  void
  UnregisterTransaction(IDBTransaction* aTransaction);

  void
  AbortTransactions(bool aShouldWarn);

  PBackgroundIDBDatabaseFileChild*
  GetOrCreateFileActorForBlob(Blob* aBlob);

  void
  NoteFinishedFileActor(PBackgroundIDBDatabaseFileChild* aFileActor);

  void
  NoteReceivedBlob(Blob* aBlob);

  void
  DelayedMaybeExpireFileActors();

  // XXX This doesn't really belong here... It's only needed for IDBMutableFile
  //     serialization and should be removed someday.
  nsresult
  GetQuotaInfo(nsACString& aOrigin, PersistenceType* aPersistenceType);

  void
  NoteLiveMutableFile(IDBMutableFile* aMutableFile);

  void
  NoteFinishedMutableFile(IDBMutableFile* aMutableFile);

  void
  OnNewFileHandle();

  void
  OnFileHandleFinished();

  nsPIDOMWindow*
  GetParentObject() const;

  already_AddRefed<DOMStringList>
  ObjectStoreNames() const;

  already_AddRefed<IDBObjectStore>
  CreateObjectStore(const nsAString& aName,
                    const IDBObjectStoreParameters& aOptionalParameters,
                    ErrorResult& aRv);

  void
  DeleteObjectStore(const nsAString& name, ErrorResult& aRv);

  // This will be called from the DOM.
  already_AddRefed<IDBTransaction>
  Transaction(const StringOrStringSequence& aStoreNames,
              IDBTransactionMode aMode,
              ErrorResult& aRv);

  // This can be called from C++ to avoid JS exception.
  nsresult
  Transaction(const StringOrStringSequence& aStoreNames,
              IDBTransactionMode aMode,
              IDBTransaction** aTransaction);

  StorageType
  Storage() const;

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(versionchange)

  already_AddRefed<IDBRequest>
  CreateMutableFile(const nsAString& aName,
                    const Optional<nsAString>& aType,
                    ErrorResult& aRv);

  already_AddRefed<IDBRequest>
  MozCreateFileHandle(const nsAString& aName,
                      const Optional<nsAString>& aType,
                      ErrorResult& aRv)
  {
    return CreateMutableFile(aName, aType, aRv);
  }

  void
  ClearBackgroundActor()
  {
    AssertIsOnOwningThread();

    mBackgroundActor = nullptr;
  }

  const DatabaseSpec*
  Spec() const
  {
    return mSpec;
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBDatabase, IDBWrapperCache)

  // nsIDOMEventTarget
  virtual void
  LastRelease() override;

  virtual nsresult
  PostHandleEvent(EventChainPostVisitor& aVisitor) override;

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

private:
  IDBDatabase(IDBWrapperCache* aOwnerCache,
              IDBFactory* aFactory,
              BackgroundDatabaseChild* aActor,
              DatabaseSpec* aSpec);

  ~IDBDatabase();

  void
  CloseInternal();

  void
  InvalidateInternal();

  bool
  RunningVersionChangeTransaction() const
  {
    AssertIsOnOwningThread();

    return !!mPreviousSpec;
  }

  void
  RefreshSpec(bool aMayDelete);

  void
  ExpireFileActors(bool aExpireAll);

  void
  InvalidateMutableFiles();

  void
  LogWarning(const char* aMessageName,
             const nsAString& aFilename,
             uint32_t aLineNumber);
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbdatabase_h__
