/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushManager.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/dom/PushManagerBinding.h"
#include "mozilla/dom/PushSubscriptionBinding.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/HoldDropJSObjects.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"

#include "nsIGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIPushClient.h"

#include "nsFrameMessageManager.h"
#include "nsContentCID.h"

#include "WorkerRunnable.h"
#include "WorkerPrivate.h"
#include "WorkerScope.h"

namespace mozilla {
namespace dom {

using namespace workers;

class UnsubscribeResultCallback final : public nsIUnsubscribeResultCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit UnsubscribeResultCallback(Promise* aPromise)
    : mPromise(aPromise)
  {
    AssertIsOnMainThread();
  }

  NS_IMETHOD
  OnUnsubscribe(nsresult aStatus, bool aSuccess) override
  {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(aSuccess);
    } else {
      mPromise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    }

    return NS_OK;
  }

private:
  ~UnsubscribeResultCallback()
  {}

  nsRefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(UnsubscribeResultCallback, nsIUnsubscribeResultCallback)

void
PushSubscription::GetP256dh(JSContext* aCx, JS::MutableHandle<JSObject*> aPublicKey)
{
  if (!mPublicKey) {
    mPublicKey = ArrayBuffer::Create(aCx, mRawPublicKey.Length(), mRawPublicKey.Elements());
    MOZ_ALWAYS_TRUE(mPublicKey);
    mRawPublicKey.Clear();
  }
  JS::ExposeObjectToActiveJS(mPublicKey);
  aPublicKey.set(mPublicKey);
}

already_AddRefed<Promise>
PushSubscription::Unsubscribe(ErrorResult& aRv)
{
  MOZ_ASSERT(mPrincipal);

  nsCOMPtr<nsIPushClient> client =
    do_CreateInstance("@mozilla.org/push/PushClient;1");
  if (NS_WARN_IF(!client)) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }

  nsRefPtr<Promise> p = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<UnsubscribeResultCallback> callback =
    new UnsubscribeResultCallback(p);
  client->Unsubscribe(mScope, mPrincipal, callback);
  return p.forget();
}

PushSubscription::PushSubscription(nsIGlobalObject* aGlobal,
                                   const nsAString& aEndpoint,
                                   const nsTArray<uint8_t>& aRawPublicKey,
                                   const nsAString& aScope)
  : mGlobal(aGlobal), mEndpoint(aEndpoint), mRawPublicKey(aRawPublicKey), mScope(aScope)
{
  mozilla::HoldJSObjects(this);
}

PushSubscription::~PushSubscription()
{
  mPublicKey = nullptr;
  mozilla::DropJSObjects(this);
}

JSObject*
PushSubscription::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PushSubscriptionBinding::Wrap(aCx, this, aGivenProto);
}

void
PushSubscription::SetPrincipal(nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(!mPrincipal);
  mPrincipal = aPrincipal;
}

// static
already_AddRefed<PushSubscription>
PushSubscription::Constructor(GlobalObject& aGlobal, const nsAString& aEndpoint, const Nullable<ArrayBuffer>& aMaybePublicKey, const nsAString& aScope, ErrorResult& aRv)
{
  MOZ_ASSERT(!aEndpoint.IsEmpty());
  MOZ_ASSERT(!aScope.IsEmpty());
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  nsTArray<uint8_t> rawPublicKey;
  if (!aMaybePublicKey.IsNull()) {
    const ArrayBuffer& aPublicKey = aMaybePublicKey.Value();
    aPublicKey.ComputeLengthAndData();
    rawPublicKey.SetLength(aPublicKey.Length());
    rawPublicKey.ReplaceElementsAt(0, aPublicKey.Length(), aPublicKey.Data(), aPublicKey.Length());
  }

  nsRefPtr<PushSubscription> sub = new PushSubscription(global, aEndpoint, rawPublicKey, aScope);
  return sub.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PushSubscription)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PushSubscription)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal, mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mPublicKey = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PushSubscription)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal, mPrincipal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(PushSubscription)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPublicKey)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(PushSubscription)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushSubscription)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushSubscription)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PushManager::PushManager(nsIGlobalObject* aGlobal, const nsAString& aScope)
  : mGlobal(aGlobal), mScope(aScope)
{
  AssertIsOnMainThread();
}

PushManager::~PushManager()
{}

JSObject*
PushManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  // XXXnsm I don't know if this is the right way to do it, but I want to assert
  // that an implementation has been set before this object gets exposed to JS.
  MOZ_ASSERT(mImpl);
  return PushManagerBinding::Wrap(aCx, this, aGivenProto);
}

void
PushManager::SetPushManagerImpl(PushManagerImpl& foo, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mImpl);
  mImpl = &foo;
}

already_AddRefed<Promise>
PushManager::Subscribe(ErrorResult& aRv)
{
  MOZ_ASSERT(mImpl);
  return mImpl->Subscribe(aRv);
}

already_AddRefed<Promise>
PushManager::GetSubscription(ErrorResult& aRv)
{
  MOZ_ASSERT(mImpl);
  return mImpl->GetSubscription(aRv);
}

already_AddRefed<Promise>
PushManager::PermissionState(ErrorResult& aRv)
{
  MOZ_ASSERT(mImpl);
  return mImpl->PermissionState(aRv);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushManager, mGlobal, mImpl)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PushManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// WorkerPushSubscription

WorkerPushSubscription::WorkerPushSubscription(const nsAString& aEndpoint,
                                               const nsTArray<uint8_t>& aRawPublicKey,
                                               const nsAString& aScope)
  : mEndpoint(aEndpoint), mRawPublicKey(aRawPublicKey), mScope(aScope)
{
  MOZ_ASSERT(!aScope.IsEmpty());
  MOZ_ASSERT(!aEndpoint.IsEmpty());
  mozilla::HoldJSObjects(this);
}

WorkerPushSubscription::~WorkerPushSubscription()
{
  mPublicKey = nullptr;
  mozilla::DropJSObjects(this);
}

JSObject*
WorkerPushSubscription::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PushSubscriptionBinding_workers::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<WorkerPushSubscription>
WorkerPushSubscription::Constructor(GlobalObject& aGlobal, const nsAString& aEndpoint, const Nullable<ArrayBuffer>& aMaybePublicKey, const nsAString& aScope, ErrorResult& aRv)
{
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  nsTArray<uint8_t> rawPublicKey;
  if (!aMaybePublicKey.IsNull()) {
    const ArrayBuffer& aPublicKey = aMaybePublicKey.Value();
    aPublicKey.ComputeLengthAndData();
    rawPublicKey.SetLength(aPublicKey.Length());
    rawPublicKey.ReplaceElementsAt(0, aPublicKey.Length(), aPublicKey.Data(), aPublicKey.Length());
  }

  nsRefPtr<WorkerPushSubscription> sub = new WorkerPushSubscription(aEndpoint, rawPublicKey, aScope);
  return sub.forget();
}

namespace {
// The caller MUST take ownership of the proxy's lock before it calls this.
void
ReleasePromiseWorkerProxy(already_AddRefed<PromiseWorkerProxy> aProxy)
{
  AssertIsOnMainThread();
  nsRefPtr<PromiseWorkerProxy> proxy = aProxy;
  MOZ_ASSERT(proxy);
  proxy->GetCleanUpLock().AssertCurrentThreadOwns();
  if (proxy->IsClean()) {
    return;
  }

  AutoJSAPI jsapi;
  jsapi.Init();

  nsRefPtr<PromiseWorkerProxyControlRunnable> cr =
    new PromiseWorkerProxyControlRunnable(proxy->GetWorkerPrivate(),
                                          proxy);

  MOZ_ALWAYS_TRUE(cr->Dispatch(jsapi.cx()));
}
} // anonymous namespace

class UnsubscribeResultRunnable final : public WorkerRunnable
{
public:
  UnsubscribeResultRunnable(PromiseWorkerProxy* aProxy,
                            nsresult aStatus,
                            bool aSuccess)
    : WorkerRunnable(aProxy->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
    , mProxy(aProxy)
    , mStatus(aStatus)
    , mSuccess(aSuccess)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<PromiseWorkerProxy> proxy = mProxy.forget();
    nsRefPtr<Promise> promise = proxy->GetWorkerPromise();
    if (NS_SUCCEEDED(mStatus)) {
      promise->MaybeResolve(mSuccess);
    } else {
      promise->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    }

    proxy->CleanUp(aCx);
    return true;
  }
private:
  ~UnsubscribeResultRunnable()
  {}

  nsRefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
  bool mSuccess;
};

class WorkerUnsubscribeResultCallback final : public nsIUnsubscribeResultCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit WorkerUnsubscribeResultCallback(PromiseWorkerProxy* aProxy)
    : mProxy(aProxy)
  {
    AssertIsOnMainThread();
  }

  NS_IMETHOD
  OnUnsubscribe(nsresult aStatus, bool aSuccess) override
  {
    AssertIsOnMainThread();
    if (!mProxy) {
      return NS_OK;
    }

    MutexAutoLock lock(mProxy->GetCleanUpLock());
    if (mProxy->IsClean()) {
      return NS_OK;
    }

    AutoJSAPI jsapi;
    jsapi.Init();

    nsRefPtr<UnsubscribeResultRunnable> r =
      new UnsubscribeResultRunnable(mProxy, aStatus, aSuccess);
    if (!r->Dispatch(jsapi.cx())) {
      ReleasePromiseWorkerProxy(mProxy.forget());
    }

    mProxy = nullptr;
    return NS_OK;
  }

private:
  ~WorkerUnsubscribeResultCallback()
  {
    AssertIsOnMainThread();
    if (mProxy) {
      MutexAutoLock lock(mProxy->GetCleanUpLock());
      if (!mProxy->IsClean()) {
        AutoJSAPI jsapi;
        jsapi.Init();
        nsRefPtr<PromiseWorkerProxyControlRunnable> cr =
          new PromiseWorkerProxyControlRunnable(mProxy->GetWorkerPrivate(), mProxy);
        cr->Dispatch(jsapi.cx());
      }
    }
  }

  nsRefPtr<PromiseWorkerProxy> mProxy;
};

NS_IMPL_ISUPPORTS(WorkerUnsubscribeResultCallback, nsIUnsubscribeResultCallback)

class UnsubscribeRunnable final : public nsRunnable
{
public:
  UnsubscribeRunnable(PromiseWorkerProxy* aProxy,
                      const nsAString& aScope)
    : mProxy(aProxy)
    , mScope(aScope)
  {
    MOZ_ASSERT(aProxy);
    MOZ_ASSERT(!aScope.IsEmpty());
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    MutexAutoLock lock(mProxy->GetCleanUpLock());
    if (mProxy->IsClean()) {
      return NS_OK;
    }

    nsRefPtr<WorkerUnsubscribeResultCallback> callback =
      new WorkerUnsubscribeResultCallback(mProxy);

    nsCOMPtr<nsIPushClient> client =
      do_CreateInstance("@mozilla.org/push/PushClient;1");
    if (!client) {
      callback->OnUnsubscribe(NS_ERROR_FAILURE, false);
    }

    nsCOMPtr<nsIPrincipal> principal = mProxy->GetWorkerPrivate()->GetPrincipal();
    if (NS_WARN_IF(NS_FAILED(client->Unsubscribe(mScope, principal, callback)))) {
      callback->OnUnsubscribe(NS_ERROR_FAILURE, false);
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

private:
  ~UnsubscribeRunnable()
  {}

  nsRefPtr<PromiseWorkerProxy> mProxy;
  nsString mScope;
};

void
WorkerPushSubscription::GetP256dh(JSContext* aCx, JS::MutableHandle<JSObject*> aPublicKey)
{
  if (!mPublicKey) {
    mPublicKey = ArrayBuffer::Create(aCx, mRawPublicKey.Length(), mRawPublicKey.Elements());
    MOZ_ALWAYS_TRUE(mPublicKey);
    mRawPublicKey.Clear();
  }
  JS::ExposeObjectToActiveJS(mPublicKey);
  aPublicKey.set(mPublicKey);
}

already_AddRefed<Promise>
WorkerPushSubscription::Unsubscribe(ErrorResult &aRv)
{
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  nsCOMPtr<nsIGlobalObject> global = worker->GlobalScope();
  nsRefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<PromiseWorkerProxy> proxy = PromiseWorkerProxy::Create(worker, p);
  if (!proxy) {
    p->MaybeReject(NS_ERROR_DOM_NETWORK_ERR);
    return p.forget();
  }

  nsRefPtr<UnsubscribeRunnable> r =
    new UnsubscribeRunnable(proxy, mScope);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(r)));

  return p.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkerPushSubscription)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WorkerPushSubscription)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mPublicKey = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WorkerPushSubscription)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WorkerPushSubscription)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mPublicKey)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkerPushSubscription)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkerPushSubscription)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerPushSubscription)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// WorkerPushManager

WorkerPushManager::WorkerPushManager(const nsAString& aScope)
  : mScope(aScope)
{
}

JSObject*
WorkerPushManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PushManagerBinding_workers::Wrap(aCx, this, aGivenProto);
}

class GetSubscriptionResultRunnable final : public WorkerRunnable
{
public:
  GetSubscriptionResultRunnable(PromiseWorkerProxy* aProxy,
                                nsresult aStatus,
                                const nsAString& aEndpoint,
                                const nsTArray<uint8_t>& aRawPublicKey,
                                const nsAString& aScope)
    : WorkerRunnable(aProxy->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
    , mProxy(aProxy)
    , mStatus(aStatus)
    , mEndpoint(aEndpoint)
    , mRawPublicKey(aRawPublicKey)
    , mScope(aScope)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    nsRefPtr<PromiseWorkerProxy> proxy = mProxy.forget();
    nsRefPtr<Promise> promise = proxy->GetWorkerPromise();
    if (NS_SUCCEEDED(mStatus)) {
      if (mEndpoint.IsEmpty()) {
        promise->MaybeResolve(JS::NullHandleValue);
      } else {
        nsRefPtr<WorkerPushSubscription> sub =
          new WorkerPushSubscription(mEndpoint, mRawPublicKey, mScope);
        promise->MaybeResolve(sub);
      }
    } else {
      promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    }

    proxy->CleanUp(aCx);
    return true;
  }
private:
  ~GetSubscriptionResultRunnable()
  {}

  nsRefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
  nsString mEndpoint;
  nsTArray<uint8_t> mRawPublicKey;
  nsString mScope;
};

class GetSubscriptionCallback final : public nsIPushEndpointCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit GetSubscriptionCallback(PromiseWorkerProxy* aProxy,
                                   const nsAString& aScope)
    : mProxy(aProxy)
    , mScope(aScope)
  {}

  NS_IMETHOD
  OnPushEndpoint(nsresult aStatus,
                 const nsAString& aEndpoint,
                 uint32_t aKeyLength,
                 uint8_t* aKeyBytes) override
  {
    AssertIsOnMainThread();

    if (!mProxy) {
      return NS_OK;
    }

    MutexAutoLock lock(mProxy->GetCleanUpLock());
    if (mProxy->IsClean()) {
      return NS_OK;
    }

    AutoJSAPI jsapi;
    jsapi.Init();

    nsTArray<uint8_t> rawPublicKey(aKeyLength);
    rawPublicKey.InsertElementsAt(0, aKeyBytes, aKeyLength);

    nsRefPtr<GetSubscriptionResultRunnable> r =
      new GetSubscriptionResultRunnable(mProxy, aStatus, aEndpoint, rawPublicKey, mScope);
    if (!r->Dispatch(jsapi.cx())) {
      ReleasePromiseWorkerProxy(mProxy.forget());
    }

    mProxy = nullptr;
    return NS_OK;
  }

protected:
  ~GetSubscriptionCallback()
  {
    AssertIsOnMainThread();
    if (mProxy) {
      MutexAutoLock lock(mProxy->GetCleanUpLock());
      if (!mProxy->IsClean()) {
        AutoJSAPI jsapi;
        jsapi.Init();
        nsRefPtr<PromiseWorkerProxyControlRunnable> cr =
          new PromiseWorkerProxyControlRunnable(mProxy->GetWorkerPrivate(), mProxy);
        cr->Dispatch(jsapi.cx());
      }
    }
  }

private:
  nsRefPtr<PromiseWorkerProxy> mProxy;
  nsString mScope;
};

NS_IMPL_ISUPPORTS(GetSubscriptionCallback, nsIPushEndpointCallback)

class GetSubscriptionRunnable final : public nsRunnable
{
public:
  GetSubscriptionRunnable(PromiseWorkerProxy* aProxy,
                          const nsAString& aScope,
                          WorkerPushManager::SubscriptionAction aAction)
    : mProxy(aProxy)
    , mScope(aScope), mAction(aAction)
  {}

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    MutexAutoLock lock(mProxy->GetCleanUpLock());
    if (mProxy->IsClean()) {
      return NS_OK;
    }

    nsRefPtr<GetSubscriptionCallback> callback = new GetSubscriptionCallback(mProxy, mScope);

    nsCOMPtr<nsIPermissionManager> permManager =
      mozilla::services::GetPermissionManager();
    if (!permManager) {
      callback->OnPushEndpoint(NS_ERROR_FAILURE, EmptyString(), 0, nullptr);
      return NS_OK;
    }

    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    nsresult rv = permManager->TestExactPermissionFromPrincipal(
                    mProxy->GetWorkerPrivate()->GetPrincipal(),
                    "push",
                    &permission);

    if (NS_WARN_IF(NS_FAILED(rv)) || permission != nsIPermissionManager::ALLOW_ACTION) {
      callback->OnPushEndpoint(NS_ERROR_FAILURE, EmptyString(), 0, nullptr);
      return NS_OK;
    }

    nsCOMPtr<nsIPushClient> client =
      do_CreateInstance("@mozilla.org/push/PushClient;1");
    if (!client) {
      callback->OnPushEndpoint(NS_ERROR_FAILURE, EmptyString(), 0, nullptr);
      return NS_OK;
    }

    nsCOMPtr<nsIPrincipal> principal = mProxy->GetWorkerPrivate()->GetPrincipal();
    mProxy = nullptr;

    if (mAction == WorkerPushManager::SubscribeAction) {
      rv = client->Subscribe(mScope, principal, callback);
    } else {
      MOZ_ASSERT(mAction == WorkerPushManager::GetSubscriptionAction);
      rv = client->GetSubscription(mScope, principal, callback);
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      callback->OnPushEndpoint(NS_ERROR_FAILURE, EmptyString(), 0, nullptr);
      return rv;
    }

    return NS_OK;
  }

private:
  ~GetSubscriptionRunnable()
  {}

  nsRefPtr<PromiseWorkerProxy> mProxy;
  nsString mScope;
  WorkerPushManager::SubscriptionAction mAction;
};

already_AddRefed<Promise>
WorkerPushManager::PerformSubscriptionAction(SubscriptionAction aAction, ErrorResult& aRv)
{
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  nsCOMPtr<nsIGlobalObject> global = worker->GlobalScope();
  nsRefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<PromiseWorkerProxy> proxy = PromiseWorkerProxy::Create(worker, p);
  if (!proxy) {
    p->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return p.forget();
  }

  nsRefPtr<GetSubscriptionRunnable> r =
    new GetSubscriptionRunnable(proxy, mScope, aAction);
  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(r)));

  return p.forget();
}

already_AddRefed<Promise>
WorkerPushManager::Subscribe(ErrorResult& aRv)
{
  return PerformSubscriptionAction(SubscribeAction, aRv);
}

already_AddRefed<Promise>
WorkerPushManager::GetSubscription(ErrorResult& aRv)
{
  return PerformSubscriptionAction(GetSubscriptionAction, aRv);
}

class PermissionResultRunnable final : public WorkerRunnable
{
public:
  PermissionResultRunnable(PromiseWorkerProxy *aProxy,
                           nsresult aStatus,
                           PushPermissionState aState)
    : WorkerRunnable(aProxy->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
    , mProxy(aProxy)
    , mStatus(aStatus)
    , mState(aState)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    nsRefPtr<PromiseWorkerProxy> proxy = mProxy.forget();
    nsRefPtr<Promise> promise = proxy->GetWorkerPromise();
    if (NS_SUCCEEDED(mStatus)) {
      MOZ_ASSERT(uint32_t(mState) < ArrayLength(PushPermissionStateValues::strings));
      nsAutoCString stringState(PushPermissionStateValues::strings[uint32_t(mState)].value, PushPermissionStateValues::strings[uint32_t(mState)].length);
      promise->MaybeResolve(NS_ConvertUTF8toUTF16(stringState));
    } else {
      promise->MaybeReject(aCx, JS::UndefinedHandleValue);
    }

    proxy->CleanUp(aCx);
    return true;
  }

private:
  ~PermissionResultRunnable()
  {}

  nsRefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
  PushPermissionState mState;
};

class PermissionStateRunnable final : public nsRunnable
{
public:
  explicit PermissionStateRunnable(PromiseWorkerProxy* aProxy)
    : mProxy(aProxy)
  {}

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    MutexAutoLock lock(mProxy->GetCleanUpLock());
    if (mProxy->IsClean()) {
      return NS_OK;
    }

    nsCOMPtr<nsIPermissionManager> permManager =
      mozilla::services::GetPermissionManager();

    nsresult rv = NS_ERROR_FAILURE;
    PushPermissionState state = PushPermissionState::Denied;

    if (permManager) {
      uint32_t permission = nsIPermissionManager::DENY_ACTION;
      rv = permManager->TestExactPermissionFromPrincipal(
             mProxy->GetWorkerPrivate()->GetPrincipal(),
             "push",
             &permission);

      if (NS_SUCCEEDED(rv)) {
        switch (permission) {
          case nsIPermissionManager::ALLOW_ACTION:
            state = PushPermissionState::Granted;
            break;
          case nsIPermissionManager::DENY_ACTION:
            state = PushPermissionState::Denied;
            break;
          case nsIPermissionManager::PROMPT_ACTION:
            state = PushPermissionState::Prompt;
            break;
          default:
            MOZ_CRASH("Unexpected case!");
        }
      }
    }

    AutoJSAPI jsapi;
    jsapi.Init();
    nsRefPtr<PermissionResultRunnable> r =
      new PermissionResultRunnable(mProxy, rv, state);
    if (!r->Dispatch(jsapi.cx())) {
      ReleasePromiseWorkerProxy(mProxy.forget());
    }
    return NS_OK;
  }

private:
  ~PermissionStateRunnable()
  {}

  nsRefPtr<PromiseWorkerProxy> mProxy;
};

already_AddRefed<Promise>
WorkerPushManager::PermissionState(ErrorResult& aRv)
{
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  nsCOMPtr<nsIGlobalObject> global = worker->GlobalScope();
  nsRefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsRefPtr<PromiseWorkerProxy> proxy = PromiseWorkerProxy::Create(worker, p);
  if (!proxy) {
    p->MaybeReject(worker->GetJSContext(), JS::UndefinedHandleValue);
    return p.forget();
  }

  nsRefPtr<PermissionStateRunnable> r =
    new PermissionStateRunnable(proxy);
  NS_DispatchToMainThread(r);

  return p.forget();
}

WorkerPushManager::~WorkerPushManager()
{}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WorkerPushManager)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkerPushManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkerPushManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerPushManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
} // namespace dom
} // namespace mozilla
