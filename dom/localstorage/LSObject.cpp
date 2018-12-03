/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSObject.h"

#include "ActorsChild.h"
#include "IPCBlobInputStreamThread.h"
#include "LocalStorageCommon.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "nsContentUtils.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsThread.h"

namespace mozilla {
namespace dom {

namespace {

class RequestHelper;

StaticMutex gRequestHelperMutex;
RequestHelper* gRequestHelper = nullptr;

/**
 * Main-thread helper that implements the blocking logic required by
 * LocalStorage's synchronous semantics.  StartAndReturnResponse pushes an
 * event queue which is a new event target and spins its nested event loop until
 * a result is received or an abort is necessary due to a PContent-managed sync
 * IPC message being received.  Note that because the event queue is its own
 * event target, there is no re-entrancy.  Normal main-thread runnables will not
 * get a chance to run.  See StartAndReturnResponse() for info on this choice.
 *
 * The normal life-cycle of this method looks like:
 * - Main Thread: LSObject::DoRequestSynchronously creates a RequestHelper and
 *   invokes StartAndReturnResponse().  It pushes the event queue and Dispatches
 *   the RequestHelper to the DOM File Thread.
 * - DOM File Thread: RequestHelper::Run is called, invoking Start() which
 *   invokes LSObject::StartRequest, which gets-or-creates the PBackground actor
 *   if necessary (which may dispatch a runnable to the nested event queue on
 *   the main thread), sends LSRequest constructor which is provided with a
 *   callback reference to the RequestHelper. State advances to ResponsePending.
 * - DOM File Thread:: LSRequestChild::Recv__delete__ is received, which invokes
 *   RequestHelepr::OnResponse, advancing the state to Finishing and dispatching
 *   RequestHelper to its own nested event target.
 * - Main Thread: RequestHelper::Run is called, invoking Finish() which advances
 *   the state to Complete and sets mWaiting to false, allowing the nested event
 *   loop being spun by StartAndReturnResponse to cease spinning and return the
 *   received response.
 *
 * See LocalStorageCommon.h for high-level context and method comments for
 * low-level details.
 */
class RequestHelper final : public Runnable, public LSRequestChildCallback {
  enum class State {
    /**
     * The RequestHelper has been created and dispatched to the DOM File Thread.
     */
    Initial,
    /**
     * Start() has been invoked on the DOM File Thread and
     * LSObject::StartRequest has been invoked from there, sending an IPC
     * message to PBackground to service the request.  We stay in this state
     * until a response is received.
     */
    ResponsePending,
    /**
     * A response has been received and RequestHelper has been dispatched back
     * to the nested event loop to call Finish().
     */
    Finishing,
    /**
     * Finish() has been called on the main thread.  The nested event loop will
     * terminate imminently and the received response returned to the caller of
     * StartAndReturnResponse.
     */
    Complete
  };

  // The object we are issuing a request on behalf of.  Present because of the
  // need to invoke LSObject::StartRequest off the main thread.  Dropped on
  // return to the main-thread in Finish().
  RefPtr<LSObject> mObject;
  // The thread the RequestHelper was created on.  This should be the main
  // thread.
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  // The pushed event queue that we use to spin the event loop without
  // processing any of the events dispatched at the mOwningEventTarget (which
  // would result in re-entrancy and violate LocalStorage semantics).
  nsCOMPtr<nsIEventTarget> mNestedEventTarget;
  // The IPC actor handling the request with standard IPC allocation rules.
  // Our reference is nulled in OnResponse which corresponds to the actor's
  // __destroy__ method.
  LSRequestChild* mActor;
  const LSRequestParams mParams;
  LSRequestResponse mResponse;
  nsresult mResultCode;
  State mState;
  // Control flag for the nested event loop; once set to false, the loop ends.
  bool mWaiting;

 public:
  RequestHelper(LSObject* aObject, const LSRequestParams& aParams)
      : Runnable("dom::RequestHelper"),
        mObject(aObject),
        mOwningEventTarget(GetCurrentThreadEventTarget()),
        mActor(nullptr),
        mParams(aParams),
        mResultCode(NS_OK),
        mState(State::Initial),
        mWaiting(true) {
    StaticMutexAutoLock lock(gRequestHelperMutex);
    gRequestHelper = this;
  }

  bool IsOnOwningThread() const {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) &&
           current;
  }

  void AssertIsOnOwningThread() const {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  // Used for requests from the parent process to the parent process; in that
  // case we want ActorsParent to know our event-target and this is better than
  // trying to tunnel the pointer through IPC.
  const nsCOMPtr<nsIEventTarget>& GetSyncLoopEventTarget() const {
    MOZ_ASSERT(XRE_IsParentProcess());

    return mNestedEventTarget;
  }

  nsresult StartAndReturnResponse(LSRequestResponse& aResponse);

  nsresult CancelOnAnyThread();

 private:
  ~RequestHelper() {
    StaticMutexAutoLock lock(gRequestHelperMutex);
    gRequestHelper = nullptr;
  }

  nsresult Start();

  void Finish();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIRUNNABLE

  // LSRequestChildCallback
  void OnResponse(const LSRequestResponse& aResponse) override;
};

}  // namespace

LSObject::LSObject(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal)
    : Storage(aWindow, aPrincipal),
      mPrivateBrowsingId(0),
      mInExplicitSnapshot(false) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NextGenLocalStorageEnabled());
}

LSObject::~LSObject() {
  AssertIsOnOwningThread();

  DropObserver();
}

// static
nsresult LSObject::CreateForWindow(nsPIDOMWindowInner* aWindow,
                                   Storage** aStorage) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aStorage);
  MOZ_ASSERT(NextGenLocalStorageEnabled());
  MOZ_ASSERT(nsContentUtils::StorageAllowedForWindow(aWindow) >
             nsContentUtils::StorageAccess::eDeny);

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  MOZ_ASSERT(sop);

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  if (NS_WARN_IF(!principal)) {
    return NS_ERROR_FAILURE;
  }

  if (nsContentUtils::IsSystemPrincipal(principal)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // localStorage is not available on some pages on purpose, for example
  // about:home. Match the old implementation by using GenerateOriginKey
  // for the check.
  nsCString dummyOriginAttrSuffix;
  nsCString dummyOriginKey;
  nsresult rv =
      GenerateOriginKey(principal, dummyOriginAttrSuffix, dummyOriginKey);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo());
  rv = PrincipalToPrincipalInfo(principal, principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(principalInfo->type() == PrincipalInfo::TContentPrincipalInfo);

  nsCString origin;
  rv = QuotaManager::GetInfoFromPrincipal(principal, nullptr, nullptr, &origin);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t privateBrowsingId;
  rv = principal->GetPrivateBrowsingId(&privateBrowsingId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsString documentURI;
  if (nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc()) {
    rv = doc->GetDocumentURI(documentURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  RefPtr<LSObject> object = new LSObject(aWindow, principal);
  object->mPrincipalInfo = std::move(principalInfo);
  object->mPrivateBrowsingId = privateBrowsingId;
  object->mOrigin = origin;
  object->mDocumentURI = documentURI;

  object.forget(aStorage);
  return NS_OK;
}

// static
nsresult LSObject::CreateForPrincipal(nsPIDOMWindowInner* aWindow,
                                      nsIPrincipal* aPrincipal,
                                      const nsAString& aDocumentURI,
                                      bool aPrivate, LSObject** aObject) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aObject);

  nsCString dummyOriginAttrSuffix;
  nsCString dummyOriginKey;
  nsresult rv =
      GenerateOriginKey(aPrincipal, dummyOriginAttrSuffix, dummyOriginKey);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoPtr<PrincipalInfo> principalInfo(new PrincipalInfo());
  rv = PrincipalToPrincipalInfo(aPrincipal, principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(principalInfo->type() == PrincipalInfo::TContentPrincipalInfo ||
             principalInfo->type() == PrincipalInfo::TSystemPrincipalInfo);

  nsCString origin;

  if (principalInfo->type() == PrincipalInfo::TSystemPrincipalInfo) {
    QuotaManager::GetInfoForChrome(nullptr, nullptr, &origin);
  } else {
    rv = QuotaManager::GetInfoFromPrincipal(aPrincipal, nullptr, nullptr,
                                            &origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  RefPtr<LSObject> object = new LSObject(aWindow, aPrincipal);
  object->mPrincipalInfo = std::move(principalInfo);
  object->mPrivateBrowsingId = aPrivate ? 1 : 0;
  object->mOrigin = origin;
  object->mDocumentURI = aDocumentURI;

  object.forget(aObject);
  return NS_OK;
}

// static
already_AddRefed<nsIEventTarget> LSObject::GetSyncLoopEventTarget() {
  RefPtr<RequestHelper> helper;

  {
    StaticMutexAutoLock lock(gRequestHelperMutex);
    helper = gRequestHelper;
  }

  nsCOMPtr<nsIEventTarget> target;
  if (helper) {
    target = helper->GetSyncLoopEventTarget();
  }

  return target.forget();
}

// static
void LSObject::CancelSyncLoop() {
  RefPtr<RequestHelper> helper;

  {
    StaticMutexAutoLock lock(gRequestHelperMutex);
    helper = gRequestHelper;
  }

  if (helper) {
    Unused << NS_WARN_IF(NS_FAILED(helper->CancelOnAnyThread()));
  }
}

LSRequestChild* LSObject::StartRequest(nsIEventTarget* aMainEventTarget,
                                       const LSRequestParams& aParams,
                                       LSRequestChildCallback* aCallback) {
  AssertIsOnDOMFileThread();

  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread(aMainEventTarget);
  if (NS_WARN_IF(!backgroundActor)) {
    return nullptr;
  }

  LSRequestChild* actor = new LSRequestChild(aCallback);

  backgroundActor->SendPBackgroundLSRequestConstructor(actor, aParams);

  return actor;
}

Storage::StorageType LSObject::Type() const {
  AssertIsOnOwningThread();

  return eLocalStorage;
}

bool LSObject::IsForkOf(const Storage* aStorage) const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aStorage);

  if (aStorage->Type() != eLocalStorage) {
    return false;
  }

  return static_cast<const LSObject*>(aStorage)->mOrigin == mOrigin;
}

int64_t LSObject::GetOriginQuotaUsage() const {
  AssertIsOnOwningThread();

  // It's not necessary to return an actual value here.  This method is
  // implemented only because the SessionStore currently needs it to cap the
  // amount of data it persists to disk (via nsIDOMWindowUtils.getStorageUsage).
  // Any callers that want to know about storage usage should be asking
  // QuotaManager directly.
  //
  // Note: This may change as LocalStorage is repurposed to be the new
  // SessionStorage backend.
  return 0;
}

uint32_t LSObject::GetLength(nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return 0;
  }

  uint32_t result;
  rv = mDatabase->GetLength(this, &result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return 0;
  }

  return result;
}

void LSObject::Key(uint32_t aIndex, nsAString& aResult,
                   nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  nsString result;
  rv = mDatabase->GetKey(this, aIndex, result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  aResult = result;
}

void LSObject::GetItem(const nsAString& aKey, nsAString& aResult,
                       nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  nsString result;
  rv = mDatabase->GetItem(this, aKey, result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  aResult = result;
}

void LSObject::GetSupportedNames(nsTArray<nsString>& aNames) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(*nsContentUtils::SubjectPrincipal())) {
    // Return just an empty array.
    aNames.Clear();
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  rv = mDatabase->GetKeys(this, aNames);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

void LSObject::SetItem(const nsAString& aKey, const nsAString& aValue,
                       nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSNotifyInfo info;
  rv = mDatabase->SetItem(this, aKey, aValue, info);
  if (rv == NS_ERROR_FILE_NO_DEVICE_SPACE) {
    rv = NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(aKey, info.oldValue(), aValue);
  }
}

void LSObject::RemoveItem(const nsAString& aKey,
                          nsIPrincipal& aSubjectPrincipal,
                          ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSNotifyInfo info;
  rv = mDatabase->RemoveItem(this, aKey, info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(aKey, info.oldValue(), VoidString());
  }
}

void LSObject::Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  LSNotifyInfo info;
  rv = mDatabase->Clear(this, info);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  if (info.changed()) {
    OnChange(VoidString(), VoidString(), VoidString());
  }
}

void LSObject::Open(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }
}

void LSObject::Close(nsIPrincipal& aSubjectPrincipal, ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  DropDatabase();
}

void LSObject::BeginExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (mInExplicitSnapshot) {
    aError.Throw(NS_ERROR_ALREADY_INITIALIZED);
    return;
  }

  nsresult rv = EnsureDatabase();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  rv = mDatabase->BeginExplicitSnapshot(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }

  mInExplicitSnapshot = true;
}

void LSObject::EndExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aError) {
  AssertIsOnOwningThread();

  if (!CanUseStorage(aSubjectPrincipal)) {
    aError.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  if (!mInExplicitSnapshot) {
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  nsresult rv = EndExplicitSnapshotInternal();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aError.Throw(rv);
    return;
  }
}

NS_IMPL_ADDREF_INHERITED(LSObject, Storage)
NS_IMPL_RELEASE_INHERITED(LSObject, Storage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LSObject)
NS_INTERFACE_MAP_END_INHERITING(Storage)

NS_IMPL_CYCLE_COLLECTION_CLASS(LSObject)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(LSObject, Storage)
  tmp->AssertIsOnOwningThread();
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(LSObject, Storage)
  tmp->AssertIsOnOwningThread();
  tmp->DropDatabase();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

nsresult LSObject::DoRequestSynchronously(const LSRequestParams& aParams,
                                          LSRequestResponse& aResponse) {
  // We don't need this yet, but once the request successfully finishes, it's
  // too late to initialize PBackground child on the owning thread, because
  // it can fail and parent would keep an extra strong ref to the datastore or
  // observer.
  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<RequestHelper> helper = new RequestHelper(this, aParams);

  // This will start and finish the request on the DOM File thread.
  // The owning thread is synchronously blocked while the request is
  // asynchronously processed on the DOM File thread.
  nsresult rv = helper->StartAndReturnResponse(aResponse);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aResponse.type() == LSRequestResponse::Tnsresult) {
    nsresult errorCode = aResponse.get_nsresult();

    if (errorCode == NS_ERROR_FILE_NO_DEVICE_SPACE) {
      errorCode = NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
    }

    return errorCode;
  }

  return NS_OK;
}

nsresult LSObject::EnsureDatabase() {
  AssertIsOnOwningThread();

  if (mDatabase && !mDatabase->IsAllowedToClose()) {
    return NS_OK;
  }

  mDatabase = LSDatabase::Get(mOrigin);

  if (mDatabase) {
    MOZ_ASSERT(!mDatabase->IsAllowedToClose());
    return NS_OK;
  }

  // We don't need this yet, but once the request successfully finishes, it's
  // too late to initialize PBackground child on the owning thread, because
  // it can fail and parent would keep an extra strong ref to the datastore.
  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  LSRequestPrepareDatastoreParams params;
  params.principalInfo() = *mPrincipalInfo;
  params.createIfNotExists() = true;

  LSRequestResponse response;

  nsresult rv = DoRequestSynchronously(params, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(response.type() ==
             LSRequestResponse::TLSRequestPrepareDatastoreResponse);

  const LSRequestPrepareDatastoreResponse& prepareDatastoreResponse =
      response.get_LSRequestPrepareDatastoreResponse();

  const NullableDatastoreId& datastoreId =
      prepareDatastoreResponse.datastoreId();

  MOZ_ASSERT(datastoreId.type() == NullableDatastoreId::Tuint64_t);

  // The datastore is now ready on the parent side (prepared by the asynchronous
  // request on the DOM File thread).
  // Let's create a direct connection to the datastore (through a database
  // actor) from the owning thread.
  // Note that we now can't error out, otherwise parent will keep an extra
  // strong reference to the datastore.

  RefPtr<LSDatabase> database = new LSDatabase(mOrigin);

  LSDatabaseChild* actor = new LSDatabaseChild(database);

  MOZ_ALWAYS_TRUE(backgroundActor->SendPBackgroundLSDatabaseConstructor(
      actor, *mPrincipalInfo, mPrivateBrowsingId, datastoreId));

  database->SetActor(actor);

  mDatabase = std::move(database);

  return NS_OK;
}

void LSObject::DropDatabase() {
  AssertIsOnOwningThread();

  if (mInExplicitSnapshot) {
    nsresult rv = EndExplicitSnapshotInternal();
    Unused << NS_WARN_IF(NS_FAILED(rv));
  }

  mDatabase = nullptr;
}

nsresult LSObject::EnsureObserver() {
  AssertIsOnOwningThread();

  if (mObserver) {
    return NS_OK;
  }

  mObserver = LSObserver::Get(mOrigin);

  if (mObserver) {
    return NS_OK;
  }

  LSRequestPrepareObserverParams params;
  params.principalInfo() = *mPrincipalInfo;

  LSRequestResponse response;

  nsresult rv = DoRequestSynchronously(params, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(response.type() ==
             LSRequestResponse::TLSRequestPrepareObserverResponse);

  const LSRequestPrepareObserverResponse& prepareObserverResponse =
      response.get_LSRequestPrepareObserverResponse();

  uint64_t observerId = prepareObserverResponse.observerId();

  // The obsserver is now ready on the parent side (prepared by the asynchronous
  // request on the DOM File thread).
  // Let's create a direct connection to the observer (through an observer
  // actor) from the owning thread.
  // Note that we now can't error out, otherwise parent will keep an extra
  // strong reference to the observer.

  PBackgroundChild* backgroundActor = BackgroundChild::GetForCurrentThread();
  MOZ_ASSERT(backgroundActor);

  RefPtr<LSObserver> observer = new LSObserver(mOrigin);

  LSObserverChild* actor = new LSObserverChild(observer);

  MOZ_ALWAYS_TRUE(
      backgroundActor->SendPBackgroundLSObserverConstructor(actor, observerId));

  observer->SetActor(actor);

  mObserver = std::move(observer);

  return NS_OK;
}

void LSObject::DropObserver() {
  AssertIsOnOwningThread();

  if (mObserver) {
    mObserver = nullptr;
  }
}

void LSObject::OnChange(const nsAString& aKey, const nsAString& aOldValue,
                        const nsAString& aNewValue) {
  AssertIsOnOwningThread();

  NotifyChange(/* aStorage */ this, Principal(), aKey, aOldValue, aNewValue,
               /* aStorageType */ kLocalStorageType, mDocumentURI,
               /* aIsPrivate */ !!mPrivateBrowsingId,
               /* aImmediateDispatch */ false);
}

nsresult LSObject::EndExplicitSnapshotInternal() {
  AssertIsOnOwningThread();

  // Can be only called if the mInExplicitSnapshot flag is true.
  // An explicit snapshot must have been created.
  MOZ_ASSERT(mInExplicitSnapshot);

  // If an explicit snapshot have been created then mDatabase must be not null.
  // DropDatabase could be called in the meatime, but that would set
  // mInExplicitSnapshot to false. EnsureDatabase could be called in the
  // meantime too, but that can't set mDatabase to null or to a new value. See
  // the comment below.
  MOZ_ASSERT(mDatabase);

  // Existence of a snapshot prevents the database from allowing to close. See
  // LSDatabase::RequestAllowToClose and LSDatabase::NoteFinishedSnapshot.
  // If the database is not allowed to close then mDatabase could not have been
  // nulled out or set to a new value. See EnsureDatabase.
  MOZ_ASSERT(!mDatabase->IsAllowedToClose());

  nsresult rv = mDatabase->EndExplicitSnapshot(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mInExplicitSnapshot = false;

  return NS_OK;
}

void LSObject::LastRelease() {
  AssertIsOnOwningThread();

  DropDatabase();
}

nsresult RequestHelper::StartAndReturnResponse(LSRequestResponse& aResponse) {
  AssertIsOnOwningThread();

  // Normally, we would use the standard way of blocking the thread using
  // a monitor.
  // The problem is that BackgroundChild::GetOrCreateForCurrentThread()
  // called on the DOM File thread may dispatch a runnable to the main
  // thread to finish initialization of PBackground. A monitor would block
  // the main thread and the runnable would never get executed causing the
  // helper to be stuck in a wait loop.
  // However, BackgroundChild::GetOrCreateForCurrentThread() supports passing
  // a custom main event target, so we can create a nested event target and
  // spin the event loop. Nothing can dispatch to the nested event target
  // except BackgroundChild::GetOrCreateForCurrentThread(), so spinning of the
  // event loop can't fire any other events.
  // This way the thread is synchronously blocked in a safe manner and the
  // runnable gets executed.
  {
    auto thread = static_cast<nsThread*>(NS_GetCurrentThread());

    auto queue =
        static_cast<ThreadEventQueue<EventQueue>*>(thread->EventQueue());

    mNestedEventTarget = queue->PushEventQueue();
    MOZ_ASSERT(mNestedEventTarget);

    auto autoPopEventQueue = mozilla::MakeScopeExit(
        [&] { queue->PopEventQueue(mNestedEventTarget); });

    nsCOMPtr<nsIEventTarget> domFileThread =
        IPCBlobInputStreamThread::GetOrCreate();
    if (NS_WARN_IF(!domFileThread)) {
      return NS_ERROR_FAILURE;
    }

    nsresult rv = domFileThread->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    MOZ_ALWAYS_TRUE(SpinEventLoopUntil([&]() { return !mWaiting; }));
  }

  if (NS_WARN_IF(NS_FAILED(mResultCode))) {
    return mResultCode;
  }

  aResponse = std::move(mResponse);
  return NS_OK;
}

nsresult RequestHelper::CancelOnAnyThread() {
  RefPtr<RequestHelper> self = this;

  RefPtr<Runnable> runnable =
      NS_NewRunnableFunction("RequestHelper::CancelOnAnyThread", [self]() {
        LSRequestChild* actor = self->mActor;
        if (actor && !actor->Finishing()) {
          actor->SendCancel();
        }
      });

  nsCOMPtr<nsIEventTarget> domFileThread =
      IPCBlobInputStreamThread::GetOrCreate();
  if (NS_WARN_IF(!domFileThread)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = domFileThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult RequestHelper::Start() {
  AssertIsOnDOMFileThread();
  MOZ_ASSERT(mState == State::Initial);

  mState = State::ResponsePending;

  LSRequestChild* actor =
      mObject->StartRequest(mNestedEventTarget, mParams, this);
  if (NS_WARN_IF(!actor)) {
    return NS_ERROR_FAILURE;
  }

  mActor = actor;

  return NS_OK;
}

void RequestHelper::Finish() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Finishing);

  mObject = nullptr;

  mWaiting = false;

  mState = State::Complete;
}

NS_IMPL_ISUPPORTS_INHERITED0(RequestHelper, Runnable)

NS_IMETHODIMP
RequestHelper::Run() {
  nsresult rv;

  switch (mState) {
    case State::Initial:
      rv = Start();
      break;

    case State::Finishing:
      Finish();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::Finishing) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    mState = State::Finishing;

    if (IsOnOwningThread()) {
      Finish();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          mNestedEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void RequestHelper::OnResponse(const LSRequestResponse& aResponse) {
  AssertIsOnDOMFileThread();
  MOZ_ASSERT(mState == State::ResponsePending);

  mActor = nullptr;

  mResponse = aResponse;

  mState = State::Finishing;
  MOZ_ALWAYS_SUCCEEDS(mNestedEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
}

}  // namespace dom
}  // namespace mozilla
