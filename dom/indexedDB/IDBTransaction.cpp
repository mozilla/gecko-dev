/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBTransaction.h"

#include "BackgroundChildImpl.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsIAppShell.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsTHashtable.h"
#include "nsWidgetsCID.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"
#include "WorkerFeature.h"
#include "WorkerPrivate.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::workers;
using namespace mozilla::ipc;

namespace {

NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

bool
RunBeforeNextEvent(IDBTransaction* aTransaction)
{
  MOZ_ASSERT(aTransaction);

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
    MOZ_ASSERT(appShell);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(appShell->RunBeforeNextEvent(aTransaction)));

    return true;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  if (NS_WARN_IF(!workerPrivate->RunBeforeNextEvent(aTransaction))) {
    return false;
  }

  return true;
}

} // anonymous namespace

class IDBTransaction::WorkerFeature final
  : public mozilla::dom::workers::WorkerFeature
{
  WorkerPrivate* mWorkerPrivate;

  // The IDBTransaction owns this object so we only need a weak reference back
  // to it.
  IDBTransaction* mTransaction;

public:
  WorkerFeature(WorkerPrivate* aWorkerPrivate, IDBTransaction* aTransaction)
    : mWorkerPrivate(aWorkerPrivate)
    , mTransaction(aTransaction)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aTransaction);
    aWorkerPrivate->AssertIsOnWorkerThread();
    aTransaction->AssertIsOnOwningThread();

    MOZ_COUNT_CTOR(IDBTransaction::WorkerFeature);
  }

  ~WorkerFeature()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    MOZ_COUNT_DTOR(IDBTransaction::WorkerFeature);

    mWorkerPrivate->RemoveFeature(mWorkerPrivate->GetJSContext(), this);
  }

private:
  virtual bool
  Notify(JSContext* aCx, Status aStatus) override;
};

IDBTransaction::IDBTransaction(IDBDatabase* aDatabase,
                               const nsTArray<nsString>& aObjectStoreNames,
                               Mode aMode)
  : IDBWrapperCache(aDatabase)
  , mDatabase(aDatabase)
  , mObjectStoreNames(aObjectStoreNames)
  , mLoggingSerialNumber(0)
  , mNextObjectStoreId(0)
  , mNextIndexId(0)
  , mAbortCode(NS_OK)
  , mPendingRequestCount(0)
  , mLineNo(0)
  , mReadyState(IDBTransaction::INITIAL)
  , mMode(aMode)
  , mCreating(false)
  , mRegistered(false)
  , mAbortedByScript(false)
#ifdef DEBUG
  , mSentCommitOrAbort(false)
  , mFiredCompleteOrAbort(false)
#endif
{
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();

  mBackgroundActor.mNormalBackgroundActor = nullptr;

  BackgroundChildImpl::ThreadLocal* threadLocal =
    BackgroundChildImpl::GetThreadLocalForCurrentThread();
  MOZ_ASSERT(threadLocal);

  ThreadLocal* idbThreadLocal = threadLocal->mIndexedDBThreadLocal;
  MOZ_ASSERT(idbThreadLocal);

  const_cast<int64_t&>(mLoggingSerialNumber) =
    idbThreadLocal->NextTransactionSN(aMode);

#ifdef DEBUG
  if (!aObjectStoreNames.IsEmpty()) {
    nsTArray<nsString> sortedNames(aObjectStoreNames);
    sortedNames.Sort();

    const uint32_t count = sortedNames.Length();
    MOZ_ASSERT(count == aObjectStoreNames.Length());

    // Make sure the array is properly sorted.
    for (uint32_t index = 0; index < count; index++) {
      MOZ_ASSERT(aObjectStoreNames[index] == sortedNames[index]);
    }

    // Make sure there are no duplicates in our objectStore names.
    for (uint32_t index = 0; index < count - 1; index++) {
      MOZ_ASSERT(sortedNames[index] != sortedNames[index + 1]);
    }
  }
#endif
}

IDBTransaction::~IDBTransaction()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mPendingRequestCount);
  MOZ_ASSERT(!mCreating);
  MOZ_ASSERT(mSentCommitOrAbort);
  MOZ_ASSERT_IF(mMode == VERSION_CHANGE &&
                  mBackgroundActor.mVersionChangeBackgroundActor,
                mFiredCompleteOrAbort);
  MOZ_ASSERT_IF(mMode != VERSION_CHANGE &&
                  mBackgroundActor.mNormalBackgroundActor,
                mFiredCompleteOrAbort);

  if (mRegistered) {
    mDatabase->UnregisterTransaction(this);
#ifdef DEBUG
    mRegistered = false;
#endif
  }

  if (mMode == VERSION_CHANGE) {
    if (auto* actor = mBackgroundActor.mVersionChangeBackgroundActor) {
      actor->SendDeleteMeInternal(/* aFailedConstructor */ false);

      MOZ_ASSERT(!mBackgroundActor.mVersionChangeBackgroundActor,
                 "SendDeleteMeInternal should have cleared!");
    }
  } else if (auto* actor = mBackgroundActor.mNormalBackgroundActor) {
    actor->SendDeleteMeInternal();

    MOZ_ASSERT(!mBackgroundActor.mNormalBackgroundActor,
               "SendDeleteMeInternal should have cleared!");
  }
}

// static
already_AddRefed<IDBTransaction>
IDBTransaction::CreateVersionChange(
                                IDBDatabase* aDatabase,
                                BackgroundVersionChangeTransactionChild* aActor,
                                IDBOpenDBRequest* aOpenRequest,
                                int64_t aNextObjectStoreId,
                                int64_t aNextIndexId)
{
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aOpenRequest);
  MOZ_ASSERT(aNextObjectStoreId > 0);
  MOZ_ASSERT(aNextIndexId > 0);

  nsTArray<nsString> emptyObjectStoreNames;

  nsRefPtr<IDBTransaction> transaction =
    new IDBTransaction(aDatabase,
                       emptyObjectStoreNames,
                       VERSION_CHANGE);
  aOpenRequest->GetCallerLocation(transaction->mFilename,
                                  &transaction->mLineNo);

  transaction->SetScriptOwner(aDatabase->GetScriptOwner());

  if (NS_WARN_IF(!RunBeforeNextEvent(transaction))) {
    MOZ_ASSERT(!NS_IsMainThread());
#ifdef DEBUG
    // Silence assertions.
    transaction->mSentCommitOrAbort = true;
#endif
    aActor->SendDeleteMeInternal(/* aFailedConstructor */ true);
    return nullptr;
  }

  transaction->mBackgroundActor.mVersionChangeBackgroundActor = aActor;
  transaction->mNextObjectStoreId = aNextObjectStoreId;
  transaction->mNextIndexId = aNextIndexId;
  transaction->mCreating = true;

  aDatabase->RegisterTransaction(transaction);
  transaction->mRegistered = true;

  return transaction.forget();
}

// static
already_AddRefed<IDBTransaction>
IDBTransaction::Create(IDBDatabase* aDatabase,
                       const nsTArray<nsString>& aObjectStoreNames,
                       Mode aMode)
{
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();
  MOZ_ASSERT(!aObjectStoreNames.IsEmpty());
  MOZ_ASSERT(aMode == READ_ONLY ||
             aMode == READ_WRITE ||
             aMode == READ_WRITE_FLUSH);

  nsRefPtr<IDBTransaction> transaction =
    new IDBTransaction(aDatabase, aObjectStoreNames, aMode);
  IDBRequest::CaptureCaller(transaction->mFilename, &transaction->mLineNo);

  transaction->SetScriptOwner(aDatabase->GetScriptOwner());

  if (NS_WARN_IF(!RunBeforeNextEvent(transaction))) {
    MOZ_ASSERT(!NS_IsMainThread());
    return nullptr;
  }

  transaction->mCreating = true;

  aDatabase->RegisterTransaction(transaction);
  transaction->mRegistered = true;

  if (!NS_IsMainThread()) {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);

    workerPrivate->AssertIsOnWorkerThread();

    JSContext* cx = workerPrivate->GetJSContext();
    MOZ_ASSERT(cx);

    transaction->mWorkerFeature = new WorkerFeature(workerPrivate, transaction);
    MOZ_ALWAYS_TRUE(workerPrivate->AddFeature(cx, transaction->mWorkerFeature));
  }

  return transaction.forget();
}

// static
IDBTransaction*
IDBTransaction::GetCurrent()
{
  using namespace mozilla::ipc;

  MOZ_ASSERT(BackgroundChild::GetForCurrentThread());

  BackgroundChildImpl::ThreadLocal* threadLocal =
    BackgroundChildImpl::GetThreadLocalForCurrentThread();
  MOZ_ASSERT(threadLocal);

  ThreadLocal* idbThreadLocal = threadLocal->mIndexedDBThreadLocal;
  MOZ_ASSERT(idbThreadLocal);

  return idbThreadLocal->GetCurrentTransaction();
}

#ifdef DEBUG

void
IDBTransaction::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mDatabase);
  mDatabase->AssertIsOnOwningThread();
}

#endif // DEBUG

void
IDBTransaction::SetBackgroundActor(BackgroundTransactionChild* aBackgroundActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(!mBackgroundActor.mNormalBackgroundActor);
  MOZ_ASSERT(mMode != VERSION_CHANGE);

  mBackgroundActor.mNormalBackgroundActor = aBackgroundActor;
}

BackgroundRequestChild*
IDBTransaction::StartRequest(IDBRequest* aRequest, const RequestParams& aParams)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aParams.type() != RequestParams::T__None);

  BackgroundRequestChild* actor = new BackgroundRequestChild(aRequest);

  if (mMode == VERSION_CHANGE) {
    MOZ_ASSERT(mBackgroundActor.mVersionChangeBackgroundActor);

    mBackgroundActor.mVersionChangeBackgroundActor->
      SendPBackgroundIDBRequestConstructor(actor, aParams);
  } else {
    MOZ_ASSERT(mBackgroundActor.mNormalBackgroundActor);

    mBackgroundActor.mNormalBackgroundActor->
      SendPBackgroundIDBRequestConstructor(actor, aParams);
  }

  // Balanced in BackgroundRequestChild::Recv__delete__().
  OnNewRequest();

  return actor;
}

void
IDBTransaction::OpenCursor(BackgroundCursorChild* aBackgroundActor,
                           const OpenCursorParams& aParams)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBackgroundActor);
  MOZ_ASSERT(aParams.type() != OpenCursorParams::T__None);

  if (mMode == VERSION_CHANGE) {
    MOZ_ASSERT(mBackgroundActor.mVersionChangeBackgroundActor);

    mBackgroundActor.mVersionChangeBackgroundActor->
      SendPBackgroundIDBCursorConstructor(aBackgroundActor, aParams);
  } else {
    MOZ_ASSERT(mBackgroundActor.mNormalBackgroundActor);

    mBackgroundActor.mNormalBackgroundActor->
      SendPBackgroundIDBCursorConstructor(aBackgroundActor, aParams);
  }

  // Balanced in BackgroundCursorChild::RecvResponse().
  OnNewRequest();
}

void
IDBTransaction::RefreshSpec(bool aMayDelete)
{
  AssertIsOnOwningThread();

  for (uint32_t count = mObjectStores.Length(), index = 0;
       index < count;
       index++) {
    mObjectStores[index]->RefreshSpec(aMayDelete);
  }

  for (uint32_t count = mDeletedObjectStores.Length(), index = 0;
       index < count;
       index++) {
    mDeletedObjectStores[index]->RefreshSpec(false);
  }
}

void
IDBTransaction::OnNewRequest()
{
  AssertIsOnOwningThread();

  if (!mPendingRequestCount) {
    MOZ_ASSERT(INITIAL == mReadyState);
    mReadyState = LOADING;
  }

  ++mPendingRequestCount;
}

void
IDBTransaction::OnRequestFinished(bool aActorDestroyedNormally)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mPendingRequestCount);

  --mPendingRequestCount;

  if (!mPendingRequestCount && !mDatabase->IsInvalidated()) {
    mReadyState = COMMITTING;

    if (aActorDestroyedNormally) {
      if (NS_SUCCEEDED(mAbortCode)) {
        SendCommit();
      } else {
        SendAbort(mAbortCode);
      }
    } else {
      // Don't try to send any more messages to the parent if the request actor
      // was killed.
#ifdef DEBUG
      MOZ_ASSERT(!mSentCommitOrAbort);
      mSentCommitOrAbort = true;
#endif
      IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld]: "
                     "Request actor was killed, transaction will be aborted",
                   "IndexedDB %s: C T[%lld]: IDBTransaction abort",
                   IDB_LOG_ID_STRING(),
                   LoggingSerialNumber());
    }
  }
}

void
IDBTransaction::SendCommit()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_SUCCEEDED(mAbortCode));
  MOZ_ASSERT(IsCommittingOrDone());
  MOZ_ASSERT(!mSentCommitOrAbort);
  MOZ_ASSERT(!mPendingRequestCount);

  // Don't do this in the macro because we always need to increment the serial
  // number to keep in sync with the parent.
  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();

  IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                 "All requests complete, committing transaction",
               "IndexedDB %s: C T[%lld] R[%llu]: IDBTransaction commit",
               IDB_LOG_ID_STRING(),
               LoggingSerialNumber(),
               requestSerialNumber);

  if (mMode == VERSION_CHANGE) {
    MOZ_ASSERT(mBackgroundActor.mVersionChangeBackgroundActor);
    mBackgroundActor.mVersionChangeBackgroundActor->SendCommit();
  } else {
    MOZ_ASSERT(mBackgroundActor.mNormalBackgroundActor);
    mBackgroundActor.mNormalBackgroundActor->SendCommit();
  }

#ifdef DEBUG
  mSentCommitOrAbort = true;
#endif
}

void
IDBTransaction::SendAbort(nsresult aResultCode)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aResultCode));
  MOZ_ASSERT(IsCommittingOrDone());
  MOZ_ASSERT(!mSentCommitOrAbort);

  // Don't do this in the macro because we always need to increment the serial
  // number to keep in sync with the parent.
  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();

  IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                 "Aborting transaction with result 0x%x",
               "IndexedDB %s: C T[%lld] R[%llu]: IDBTransaction abort (0x%x)",
               IDB_LOG_ID_STRING(),
               LoggingSerialNumber(),
               requestSerialNumber,
               aResultCode);

  if (mMode == VERSION_CHANGE) {
    MOZ_ASSERT(mBackgroundActor.mVersionChangeBackgroundActor);
    mBackgroundActor.mVersionChangeBackgroundActor->SendAbort(aResultCode);
  } else {
    MOZ_ASSERT(mBackgroundActor.mNormalBackgroundActor);
    mBackgroundActor.mNormalBackgroundActor->SendAbort(aResultCode);
  }

#ifdef DEBUG
  mSentCommitOrAbort = true;
#endif
}

bool
IDBTransaction::IsOpen() const
{
  AssertIsOnOwningThread();

  // If we haven't started anything then we're open.
  if (mReadyState == IDBTransaction::INITIAL) {
    return true;
  }

  // If we've already started then we need to check to see if we still have the
  // mCreating flag set. If we do (i.e. we haven't returned to the event loop
  // from the time we were created) then we are open. Otherwise check the
  // currently running transaction to see if it's the same. We only allow other
  // requests to be made if this transaction is currently running.
  if (mReadyState == IDBTransaction::LOADING &&
      (mCreating || GetCurrent() == this)) {
    return true;
  }

  return false;
}

void
IDBTransaction::GetCallerLocation(nsAString& aFilename, uint32_t* aLineNo) const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aLineNo);

  aFilename = mFilename;
  *aLineNo = mLineNo;
}

already_AddRefed<IDBObjectStore>
IDBTransaction::CreateObjectStore(const ObjectStoreSpec& aSpec)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aSpec.metadata().id());
  MOZ_ASSERT(VERSION_CHANGE == mMode);
  MOZ_ASSERT(mBackgroundActor.mVersionChangeBackgroundActor);
  MOZ_ASSERT(IsOpen());

#ifdef DEBUG
  {
    const nsString& name = aSpec.metadata().name();

    for (uint32_t count = mObjectStores.Length(), index = 0;
         index < count;
         index++) {
      MOZ_ASSERT(mObjectStores[index]->Name() != name);
    }
  }
#endif

  MOZ_ALWAYS_TRUE(mBackgroundActor.mVersionChangeBackgroundActor->
                    SendCreateObjectStore(aSpec.metadata()));

  nsRefPtr<IDBObjectStore> objectStore = IDBObjectStore::Create(this, aSpec);
  MOZ_ASSERT(objectStore);

  mObjectStores.AppendElement(objectStore);

  return objectStore.forget();
}

void
IDBTransaction::DeleteObjectStore(int64_t aObjectStoreId)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObjectStoreId);
  MOZ_ASSERT(VERSION_CHANGE == mMode);
  MOZ_ASSERT(mBackgroundActor.mVersionChangeBackgroundActor);
  MOZ_ASSERT(IsOpen());

  MOZ_ALWAYS_TRUE(mBackgroundActor.mVersionChangeBackgroundActor->
                    SendDeleteObjectStore(aObjectStoreId));

  for (uint32_t count = mObjectStores.Length(), index = 0;
       index < count;
       index++) {
    nsRefPtr<IDBObjectStore>& objectStore = mObjectStores[index];

    if (objectStore->Id() == aObjectStoreId) {
      objectStore->NoteDeletion();

      nsRefPtr<IDBObjectStore>* deletedObjectStore =
        mDeletedObjectStores.AppendElement();
      deletedObjectStore->swap(mObjectStores[index]);

      mObjectStores.RemoveElementAt(index);
      break;
    }
  }
}

void
IDBTransaction::CreateIndex(IDBObjectStore* aObjectStore,
                            const IndexMetadata& aMetadata)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObjectStore);
  MOZ_ASSERT(aMetadata.id());
  MOZ_ASSERT(VERSION_CHANGE == mMode);
  MOZ_ASSERT(mBackgroundActor.mVersionChangeBackgroundActor);
  MOZ_ASSERT(IsOpen());

  MOZ_ALWAYS_TRUE(mBackgroundActor.mVersionChangeBackgroundActor->
                    SendCreateIndex(aObjectStore->Id(), aMetadata));
}

void
IDBTransaction::DeleteIndex(IDBObjectStore* aObjectStore,
                            int64_t aIndexId)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObjectStore);
  MOZ_ASSERT(aIndexId);
  MOZ_ASSERT(VERSION_CHANGE == mMode);
  MOZ_ASSERT(mBackgroundActor.mVersionChangeBackgroundActor);
  MOZ_ASSERT(IsOpen());

  MOZ_ALWAYS_TRUE(mBackgroundActor.mVersionChangeBackgroundActor->
                    SendDeleteIndex(aObjectStore->Id(), aIndexId));
}

void
IDBTransaction::AbortInternal(nsresult aAbortCode,
                              already_AddRefed<DOMError> aError)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(NS_FAILED(aAbortCode));
  MOZ_ASSERT(!IsCommittingOrDone());

  nsRefPtr<DOMError> error = aError;

  const bool isVersionChange = mMode == VERSION_CHANGE;
  const bool isInvalidated = mDatabase->IsInvalidated();
  bool needToSendAbort = mReadyState == INITIAL && !isInvalidated;

  if (isInvalidated) {
#ifdef DEBUG
    mSentCommitOrAbort = true;
#endif
    // Increment the serial number counter here to account for the aborted
    // transaction and keep the parent in sync.
    IDBRequest::NextSerialNumber();
  }

  mAbortCode = aAbortCode;
  mReadyState = DONE;
  mError = error.forget();

  if (isVersionChange) {
    // If a version change transaction is aborted, we must revert the world
    // back to its previous state unless we're being invalidated after the
    // transaction already completed.
    if (!isInvalidated) {
      mDatabase->RevertToPreviousState();
    }

    const nsTArray<ObjectStoreSpec>& specArray =
      mDatabase->Spec()->objectStores();

    if (specArray.IsEmpty()) {
      mObjectStores.Clear();
      mDeletedObjectStores.Clear();
    } else {
      nsTHashtable<nsUint64HashKey> validIds(specArray.Length());

      for (uint32_t specCount = specArray.Length(), specIndex = 0;
           specIndex < specCount;
           specIndex++) {
        const int64_t objectStoreId = specArray[specIndex].metadata().id();
        MOZ_ASSERT(objectStoreId);

        validIds.PutEntry(uint64_t(objectStoreId));
      }

      for (uint32_t objCount = mObjectStores.Length(), objIndex = 0;
            objIndex < objCount;
            /* incremented conditionally */) {
        const int64_t objectStoreId = mObjectStores[objIndex]->Id();
        MOZ_ASSERT(objectStoreId);

        if (validIds.Contains(uint64_t(objectStoreId))) {
          objIndex++;
        } else {
          mObjectStores.RemoveElementAt(objIndex);
          objCount--;
        }
      }

      if (!mDeletedObjectStores.IsEmpty()) {
        for (uint32_t objCount = mDeletedObjectStores.Length(), objIndex = 0;
              objIndex < objCount;
              objIndex++) {
          const int64_t objectStoreId = mDeletedObjectStores[objIndex]->Id();
          MOZ_ASSERT(objectStoreId);

          if (validIds.Contains(uint64_t(objectStoreId))) {
            nsRefPtr<IDBObjectStore>* objectStore =
              mObjectStores.AppendElement();
            objectStore->swap(mDeletedObjectStores[objIndex]);
          }
        }
        mDeletedObjectStores.Clear();
      }
    }
  }

  // Fire the abort event if there are no outstanding requests. Otherwise the
  // abort event will be fired when all outstanding requests finish.
  if (needToSendAbort) {
    SendAbort(aAbortCode);
  }

  if (isVersionChange) {
    mDatabase->Close();
  }
}

void
IDBTransaction::Abort(IDBRequest* aRequest)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aRequest);

  if (IsCommittingOrDone()) {
    // Already started (and maybe finished) the commit or abort so there is
    // nothing to do here.
    return;
  }

  ErrorResult rv;
  nsRefPtr<DOMError> error = aRequest->GetError(rv);

  AbortInternal(aRequest->GetErrorCode(), error.forget());
}

void
IDBTransaction::Abort(nsresult aErrorCode)
{
  AssertIsOnOwningThread();

  if (IsCommittingOrDone()) {
    // Already started (and maybe finished) the commit or abort so there is
    // nothing to do here.
    return;
  }

  nsRefPtr<DOMError> error = new DOMError(GetOwner(), aErrorCode);
  AbortInternal(aErrorCode, error.forget());
}

void
IDBTransaction::Abort(ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (IsCommittingOrDone()) {
    aRv = NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
    return;
  }

  AbortInternal(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR, nullptr);

  MOZ_ASSERT(!mAbortedByScript);
  mAbortedByScript = true;
}

void
IDBTransaction::FireCompleteOrAbortEvents(nsresult aResult)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mFiredCompleteOrAbort);

  mReadyState = DONE;

#ifdef DEBUG
  mFiredCompleteOrAbort = true;
#endif

  // Make sure we drop the WorkerFeature when this function completes.
  nsAutoPtr<WorkerFeature> workerFeature = Move(mWorkerFeature);

  nsCOMPtr<nsIDOMEvent> event;
  if (NS_SUCCEEDED(aResult)) {
    event = CreateGenericEvent(this,
                               nsDependentString(kCompleteEventType),
                               eDoesNotBubble,
                               eNotCancelable);
    MOZ_ASSERT(event);
  } else {
    if (!mError && !mAbortedByScript) {
      mError = new DOMError(GetOwner(), aResult);
    }

    event = CreateGenericEvent(this,
                               nsDependentString(kAbortEventType),
                               eDoesBubble,
                               eNotCancelable);
    MOZ_ASSERT(event);
  }

  if (NS_SUCCEEDED(mAbortCode)) {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld]: "
                   "Firing 'complete' event",
                 "IndexedDB %s: C T[%lld]: IDBTransaction 'complete' event",
                 IDB_LOG_ID_STRING(),
                 mLoggingSerialNumber);
  } else {
    IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld]: "
                   "Firing 'abort' event with error 0x%x",
                 "IndexedDB %s: C T[%lld]: IDBTransaction 'abort' event (0x%x)",
                 IDB_LOG_ID_STRING(),
                 mLoggingSerialNumber,
                 mAbortCode);
  }

  bool dummy;
  if (NS_FAILED(DispatchEvent(event, &dummy))) {
    NS_WARNING("DispatchEvent failed!");
  }

  mDatabase->DelayedMaybeExpireFileActors();
}

int64_t
IDBTransaction::NextObjectStoreId()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(VERSION_CHANGE == mMode);

  return mNextObjectStoreId++;
}

int64_t
IDBTransaction::NextIndexId()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(VERSION_CHANGE == mMode);

  return mNextIndexId++;
}

nsPIDOMWindow*
IDBTransaction::GetParentObject() const
{
  AssertIsOnOwningThread();

  return mDatabase->GetParentObject();
}

IDBTransactionMode
IDBTransaction::GetMode(ErrorResult& aRv) const
{
  AssertIsOnOwningThread();

  switch (mMode) {
    case READ_ONLY:
      return IDBTransactionMode::Readonly;

    case READ_WRITE:
      return IDBTransactionMode::Readwrite;

    case READ_WRITE_FLUSH:
      return IDBTransactionMode::Readwriteflush;

    case VERSION_CHANGE:
      return IDBTransactionMode::Versionchange;

    case MODE_INVALID:
    default:
      MOZ_CRASH("Bad mode!");
  }
}

DOMError*
IDBTransaction::GetError() const
{
  AssertIsOnOwningThread();

  return mError;
}

already_AddRefed<DOMStringList>
IDBTransaction::ObjectStoreNames()
{
  AssertIsOnOwningThread();

  if (mMode == IDBTransaction::VERSION_CHANGE) {
    return mDatabase->ObjectStoreNames();
  }

  nsRefPtr<DOMStringList> list = new DOMStringList();
  list->StringArray() = mObjectStoreNames;
  return list.forget();
}

already_AddRefed<IDBObjectStore>
IDBTransaction::ObjectStore(const nsAString& aName, ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (IsCommittingOrDone()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  const ObjectStoreSpec* spec = nullptr;

  if (IDBTransaction::VERSION_CHANGE == mMode ||
      mObjectStoreNames.Contains(aName)) {
    const nsTArray<ObjectStoreSpec>& objectStores =
      mDatabase->Spec()->objectStores();

    for (uint32_t count = objectStores.Length(), index = 0;
         index < count;
         index++) {
      const ObjectStoreSpec& objectStore = objectStores[index];
      if (objectStore.metadata().name() == aName) {
        spec = &objectStore;
        break;
      }
    }
  }

  if (!spec) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
    return nullptr;
  }

  const int64_t desiredId = spec->metadata().id();

  nsRefPtr<IDBObjectStore> objectStore;

  for (uint32_t count = mObjectStores.Length(), index = 0;
       index < count;
       index++) {
    nsRefPtr<IDBObjectStore>& existingObjectStore = mObjectStores[index];

    if (existingObjectStore->Id() == desiredId) {
      objectStore = existingObjectStore;
      break;
    }
  }

  if (!objectStore) {
    objectStore = IDBObjectStore::Create(this, *spec);
    MOZ_ASSERT(objectStore);

    mObjectStores.AppendElement(objectStore);
  }

  return objectStore.forget();
}

NS_IMPL_ADDREF_INHERITED(IDBTransaction, IDBWrapperCache)
NS_IMPL_RELEASE_INHERITED(IDBTransaction, IDBWrapperCache)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBTransaction)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_END_INHERITING(IDBWrapperCache)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBTransaction)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBTransaction,
                                                  IDBWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDatabase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mError)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mObjectStores)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeletedObjectStores)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBTransaction, IDBWrapperCache)
  // Don't unlink mDatabase!
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mError)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mObjectStores)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDeletedObjectStores)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

JSObject*
IDBTransaction::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  AssertIsOnOwningThread();

  return IDBTransactionBinding::Wrap(aCx, this, aGivenProto);
}

nsresult
IDBTransaction::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  AssertIsOnOwningThread();

  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = mDatabase;
  return NS_OK;
}

NS_IMETHODIMP
IDBTransaction::Run()
{
  AssertIsOnOwningThread();

  // We're back at the event loop, no longer newborn.
  mCreating = false;

  // Maybe commit if there were no requests generated.
  if (mReadyState == IDBTransaction::INITIAL) {
    mReadyState = DONE;

    SendCommit();
  }

  return NS_OK;
}

bool
IDBTransaction::
WorkerFeature::Notify(JSContext* aCx, Status aStatus)
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(aStatus > Running);

  if (mTransaction && aStatus > Terminating) {
    mTransaction->AssertIsOnOwningThread();

    nsRefPtr<IDBTransaction> transaction = Move(mTransaction);

    if (!transaction->IsCommittingOrDone()) {
      IDB_REPORT_INTERNAL_ERR();
      transaction->AbortInternal(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR, nullptr);
    }
  }

  return true;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
