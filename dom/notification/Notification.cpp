/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Notification.h"

#include <utility>

#include "mozilla/BasePrincipal.h"
#include "mozilla/Components.h"
#include "mozilla/Encoding.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/JSONStringWriteFuncs.h"
#include "mozilla/OwningNonNull.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/AppNotificationServiceOptionsBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/QMResult.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "Navigator.h"
#include "NotificationUtils.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIAlertsService.h"
#include "nsIContentPermissionPrompt.h"
#include "nsILoadContext.h"
#include "nsINotificationStorage.h"
#include "nsIPermission.h"
#include "nsIPermissionManager.h"
#include "nsIScriptError.h"
#include "nsIServiceWorkerManager.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsServiceManagerUtils.h"
#include "nsStructuredCloneContainer.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

namespace mozilla::dom {

using namespace notification;

struct NotificationStrings {
  const nsString mID;
  const nsString mTitle;
  const nsString mDir;
  const nsString mLang;
  const nsString mBody;
  const nsString mTag;
  const nsString mIcon;
  const nsString mData;
  const nsString mBehavior;
  const nsString mServiceWorkerRegistrationScope;
};

class ScopeCheckingGetCallback : public nsINotificationStorageCallback {
  const nsString mScope;

 public:
  explicit ScopeCheckingGetCallback(const nsAString& aScope) : mScope(aScope) {}

  NS_IMETHOD Handle(const nsAString& aID, const nsAString& aTitle,
                    const nsAString& aDir, const nsAString& aLang,
                    const nsAString& aBody, const nsAString& aTag,
                    const nsAString& aIcon, const nsAString& aData,
                    const nsAString& aBehavior,
                    const nsAString& aServiceWorkerRegistrationScope) final {
    AssertIsOnMainThread();
    MOZ_ASSERT(!aID.IsEmpty());

    // Skip scopes that don't match when called from getNotifications().
    if (!mScope.IsEmpty() && !mScope.Equals(aServiceWorkerRegistrationScope)) {
      return NS_OK;
    }

    NotificationStrings strings = {
        nsString(aID),       nsString(aTitle),
        nsString(aDir),      nsString(aLang),
        nsString(aBody),     nsString(aTag),
        nsString(aIcon),     nsString(aData),
        nsString(aBehavior), nsString(aServiceWorkerRegistrationScope),
    };

    mStrings.AppendElement(std::move(strings));
    return NS_OK;
  }

  NS_IMETHOD Done() override = 0;

 protected:
  virtual ~ScopeCheckingGetCallback() = default;

  nsTArray<NotificationStrings> mStrings;
};

class NotificationStorageCallback final : public ScopeCheckingGetCallback {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(NotificationStorageCallback)

  NotificationStorageCallback(nsIGlobalObject* aWindow, const nsAString& aScope,
                              Promise* aPromise)
      : ScopeCheckingGetCallback(aScope), mWindow(aWindow), mPromise(aPromise) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aWindow);
    MOZ_ASSERT(aPromise);
  }

  NS_IMETHOD Done() final {
    AutoTArray<RefPtr<Notification>, 5> notifications;

    for (uint32_t i = 0; i < mStrings.Length(); ++i) {
      auto result = Notification::ConstructFromFields(
          mWindow, mStrings[i].mID, mStrings[i].mTitle, mStrings[i].mDir,
          mStrings[i].mLang, mStrings[i].mBody, mStrings[i].mTag,
          mStrings[i].mIcon, mStrings[i].mData,
          /* mStrings[i].mBehavior, not
           * supported */
          mStrings[i].mServiceWorkerRegistrationScope);
      if (result.isErr()) {
        continue;
      }
      RefPtr<Notification> n = result.unwrap();
      notifications.AppendElement(n.forget());
    }

    mPromise->MaybeResolve(notifications);
    return NS_OK;
  }

 private:
  virtual ~NotificationStorageCallback() = default;

  nsCOMPtr<nsIGlobalObject> mWindow;
  RefPtr<Promise> mPromise;
  const nsString mScope;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(NotificationStorageCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NotificationStorageCallback)
NS_IMPL_CYCLE_COLLECTION(NotificationStorageCallback, mWindow, mPromise);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NotificationStorageCallback)
  NS_INTERFACE_MAP_ENTRY(nsINotificationStorageCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

class NotificationGetRunnable final : public Runnable {
  bool mIsPrivate;
  const nsString mOrigin;
  const nsString mTag;
  nsCOMPtr<nsINotificationStorageCallback> mCallback;

 public:
  NotificationGetRunnable(const nsAString& aOrigin, const nsAString& aTag,
                          nsINotificationStorageCallback* aCallback,
                          bool aIsPrivate)
      : Runnable("NotificationGetRunnable"),
        mIsPrivate(aIsPrivate),
        mOrigin(aOrigin),
        mTag(aTag),
        mCallback(aCallback) {}

  NS_IMETHOD
  Run() override {
    nsCOMPtr<nsINotificationStorage> notificationStorage =
        GetNotificationStorage(mIsPrivate);
    if (NS_WARN_IF(!notificationStorage)) {
      return NS_ERROR_UNEXPECTED;
    }

    nsresult rv = notificationStorage->Get(mOrigin, mTag, mCallback);
    // XXXnsm Is it guaranteed mCallback will be called in case of failure?
    Unused << NS_WARN_IF(NS_FAILED(rv));
    return rv;
  }
};

class NotificationPermissionRequest : public ContentPermissionRequestBase,
                                      public nsIRunnable,
                                      public nsINamed {
 public:
  NS_DECL_NSIRUNNABLE
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(NotificationPermissionRequest,
                                           ContentPermissionRequestBase)

  // nsIContentPermissionRequest
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::Handle<JS::Value> choices) override;

  NotificationPermissionRequest(nsIPrincipal* aPrincipal,
                                nsIPrincipal* aEffectiveStoragePrincipal,
                                nsPIDOMWindowInner* aWindow, Promise* aPromise,
                                NotificationPermissionCallback* aCallback)
      : ContentPermissionRequestBase(aPrincipal, aWindow, "notification"_ns,
                                     "desktop-notification"_ns),
        mEffectiveStoragePrincipal(aEffectiveStoragePrincipal),
        mPermission(NotificationPermission::Default),
        mPromise(aPromise),
        mCallback(aCallback) {
    MOZ_ASSERT(aPromise);
  }

  NS_IMETHOD GetName(nsACString& aName) override {
    aName.AssignLiteral("NotificationPermissionRequest");
    return NS_OK;
  }

 protected:
  ~NotificationPermissionRequest() = default;

  MOZ_CAN_RUN_SCRIPT nsresult ResolvePromise();
  nsresult DispatchResolvePromise();
  nsCOMPtr<nsIPrincipal> mEffectiveStoragePrincipal;
  NotificationPermission mPermission;
  RefPtr<Promise> mPromise;
  RefPtr<NotificationPermissionCallback> mCallback;
};

namespace {
class GetPermissionRunnable final : public WorkerMainThreadRunnable {
 public:
  explicit GetPermissionRunnable(WorkerPrivate* aWorker,
                                 bool aUseRegularPrincipal,
                                 PermissionCheckPurpose aPurpose)
      : WorkerMainThreadRunnable(aWorker, "Notification :: Get Permission"_ns),
        mUseRegularPrincipal(aUseRegularPrincipal),
        mPurpose(aPurpose) {}

  bool MainThreadRun() override {
    MOZ_ASSERT(mWorkerRef);
    WorkerPrivate* workerPrivate = mWorkerRef->Private();
    nsIPrincipal* principal = workerPrivate->GetPrincipal();
    nsIPrincipal* effectiveStoragePrincipal =
        mUseRegularPrincipal ? principal
                             : workerPrivate->GetPartitionedPrincipal();
    mPermission =
        GetNotificationPermission(principal, effectiveStoragePrincipal,
                                  workerPrivate->IsSecureContext(), mPurpose);
    return true;
  }

  NotificationPermission GetPermission() { return mPermission; }

 private:
  NotificationPermission mPermission = NotificationPermission::Denied;
  bool mUseRegularPrincipal;
  PermissionCheckPurpose mPurpose;
};

}  // anonymous namespace

// Subclass that can be directly dispatched to child workers from the main
// thread.
class NotificationWorkerRunnable : public MainThreadWorkerRunnable {
 protected:
  explicit NotificationWorkerRunnable(
      WorkerPrivate* aWorkerPrivate,
      const char* aName = "NotificationWorkerRunnable")
      : MainThreadWorkerRunnable(aName) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();
    // WorkerScope might start dying at the moment. And WorkerRunInternal()
    // should not be executed once WorkerScope is dying, since
    // WorkerRunInternal() might access resources which already been freed
    // during WorkerRef::Notify().
    if (aWorkerPrivate->GlobalScope() &&
        !aWorkerPrivate->GlobalScope()->IsDying()) {
      WorkerRunInternal(aWorkerPrivate);
    }
    return true;
  }

  virtual void WorkerRunInternal(WorkerPrivate* aWorkerPrivate) = 0;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(NotificationPermissionRequest,
                                   ContentPermissionRequestBase, mCallback)
NS_IMPL_ADDREF_INHERITED(NotificationPermissionRequest,
                         ContentPermissionRequestBase)
NS_IMPL_RELEASE_INHERITED(NotificationPermissionRequest,
                          ContentPermissionRequestBase)

NS_IMPL_QUERY_INTERFACE_CYCLE_COLLECTION_INHERITED(
    NotificationPermissionRequest, ContentPermissionRequestBase, nsIRunnable,
    nsINamed)

NS_IMETHODIMP
NotificationPermissionRequest::Run() {
  if (IsNotificationAllowedFor(mPrincipal)) {
    mPermission = NotificationPermission::Granted;
  } else if (IsNotificationForbiddenFor(
                 mPrincipal, mEffectiveStoragePrincipal,
                 mWindow->IsSecureContext(),
                 PermissionCheckPurpose::PermissionRequest,
                 mWindow->GetExtantDoc())) {
    mPermission = NotificationPermission::Denied;
  }

  // We can't call ShowPrompt() directly here since our logic for determining
  // whether to display a prompt depends on the checks above as well as the
  // result of CheckPromptPrefs().  So we have to manually check the prompt
  // prefs and decide what to do based on that.
  PromptResult pr = CheckPromptPrefs();
  switch (pr) {
    case PromptResult::Granted:
      mPermission = NotificationPermission::Granted;
      break;
    case PromptResult::Denied:
      mPermission = NotificationPermission::Denied;
      break;
    default:
      // ignore
      break;
  }

  if (!mHasValidTransientUserGestureActivation &&
      !StaticPrefs::dom_webnotifications_requireuserinteraction()) {
    nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
    if (doc) {
      doc->WarnOnceAbout(Document::eNotificationsRequireUserGestureDeprecation);
    }
  }

  if (mPermission != NotificationPermission::Default) {
    return DispatchResolvePromise();
  }

  return nsContentPermissionUtils::AskPermission(this, mWindow);
}

NS_IMETHODIMP
NotificationPermissionRequest::Cancel() {
  // `Cancel` is called if the user denied permission or dismissed the
  // permission request. To distinguish between the two, we set the
  // permission to "default" and query the permission manager in
  // `ResolvePromise`.
  mPermission = NotificationPermission::Default;
  return DispatchResolvePromise();
}

NS_IMETHODIMP
NotificationPermissionRequest::Allow(JS::Handle<JS::Value> aChoices) {
  MOZ_ASSERT(aChoices.isUndefined());

  mPermission = NotificationPermission::Granted;
  return DispatchResolvePromise();
}

inline nsresult NotificationPermissionRequest::DispatchResolvePromise() {
  nsCOMPtr<nsIRunnable> resolver =
      NewRunnableMethod("NotificationPermissionRequest::DispatchResolvePromise",
                        this, &NotificationPermissionRequest::ResolvePromise);
  return nsGlobalWindowInner::Cast(mWindow.get())->Dispatch(resolver.forget());
}

nsresult NotificationPermissionRequest::ResolvePromise() {
  nsresult rv = NS_OK;
  // This will still be "default" if the user dismissed the doorhanger,
  // or "denied" otherwise.
  if (mPermission == NotificationPermission::Default) {
    // When the front-end has decided to deny the permission request
    // automatically and we are not handling user input, then log a
    // warning in the current document that this happened because
    // Notifications require a user gesture.
    if (!mHasValidTransientUserGestureActivation &&
        StaticPrefs::dom_webnotifications_requireuserinteraction()) {
      nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
      if (doc) {
        nsContentUtils::ReportToConsole(nsIScriptError::errorFlag, "DOM"_ns,
                                        doc, nsContentUtils::eDOM_PROPERTIES,
                                        "NotificationsRequireUserGesture");
      }
    }

    mPermission = GetRawNotificationPermission(mPrincipal);
  }
  if (mCallback) {
    ErrorResult error;
    RefPtr<NotificationPermissionCallback> callback(mCallback);
    callback->Call(mPermission, error);
    rv = error.StealNSResult();
  }
  mPromise->MaybeResolve(mPermission);
  return rv;
}

// static
bool Notification::PrefEnabled(JSContext* aCx, JSObject* aObj) {
  return StaticPrefs::dom_webnotifications_enabled();
}

Notification::Notification(nsIGlobalObject* aGlobal, const nsAString& aID,
                           const nsAString& aTitle, const nsAString& aBody,
                           NotificationDirection aDir, const nsAString& aLang,
                           const nsAString& aTag, const nsAString& aIconUrl,
                           bool aRequireInteraction, bool aSilent,
                           nsTArray<uint32_t>&& aVibrate,
                           const NotificationBehavior& aBehavior)
    : DOMEventTargetHelper(aGlobal),
      mID(aID),
      mTitle(aTitle),
      mBody(aBody),
      mDir(aDir),
      mLang(aLang),
      mTag(aTag),
      mIconUrl(aIconUrl),
      mRequireInteraction(aRequireInteraction),
      mSilent(aSilent),
      mVibrate(std::move(aVibrate)),
      mBehavior(aBehavior),
      mData(JS::NullValue()) {
  KeepAliveIfHasListenersFor(nsGkAtoms::onclick);
  KeepAliveIfHasListenersFor(nsGkAtoms::onshow);
  KeepAliveIfHasListenersFor(nsGkAtoms::onerror);
  KeepAliveIfHasListenersFor(nsGkAtoms::onclose);
}

// May be called on any thread.
// static
already_AddRefed<Notification> Notification::Constructor(
    const GlobalObject& aGlobal, const nsAString& aTitle,
    const NotificationOptions& aOptions, ErrorResult& aRv) {
  // FIXME(nsm): If the sticky flag is set, throw an error.
  RefPtr<ServiceWorkerGlobalScope> scope;
  UNWRAP_OBJECT(ServiceWorkerGlobalScope, aGlobal.Get(), scope);
  if (scope) {
    aRv.ThrowTypeError(
        "Notification constructor cannot be used in ServiceWorkerGlobalScope. "
        "Use registration.showNotification() instead.");
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Notification> notification =
      Create(aGlobal.Context(), global, aTitle, aOptions, u""_ns, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::CreateInfallible(global);
  promise->AddCallbacksWithCycleCollectedArgs(
      [](JSContext*, JS::Handle<JS::Value>, ErrorResult&,
         Notification* aNotification) {
        aNotification->DispatchTrustedEvent(u"show"_ns);
      },
      [](JSContext*, JS::Handle<JS::Value> aValue, ErrorResult&,
         Notification* aNotification) {
        aNotification->DispatchTrustedEvent(u"error"_ns);
        aNotification->Deactivate();
      },
      notification);
  if (!notification->CreateActor() || !notification->SendShow(promise)) {
    notification->Deactivate();
    return nullptr;
  }

  notification->KeepAliveIfHasListenersFor(nsGkAtoms::onclick);
  notification->KeepAliveIfHasListenersFor(nsGkAtoms::onshow);
  notification->KeepAliveIfHasListenersFor(nsGkAtoms::onerror);
  notification->KeepAliveIfHasListenersFor(nsGkAtoms::onclose);

  return notification.forget();
}

// static
Result<already_AddRefed<Notification>, QMResult>
Notification::ConstructFromFields(
    nsIGlobalObject* aGlobal, const nsAString& aID, const nsAString& aTitle,
    const nsAString& aDir, const nsAString& aLang, const nsAString& aBody,
    const nsAString& aTag, const nsAString& aIcon, const nsAString& aData,
    const nsAString& aServiceWorkerRegistrationScope) {
  MOZ_ASSERT(aGlobal);

  RootedDictionary<NotificationOptions> options(RootingCx());
  options.mDir = StringToEnum<NotificationDirection>(aDir).valueOr(
      NotificationDirection::Auto);
  options.mLang = aLang;
  options.mBody = aBody;
  options.mTag = aTag;
  options.mIcon = aIcon;
  IgnoredErrorResult rv;
  RefPtr<Notification> notification =
      CreateInternal(aGlobal, aID, aTitle, options, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return Err(ToQMResult(NS_ERROR_FAILURE));
  }

  QM_TRY(notification->InitFromBase64(aData));

  notification->SetScope(aServiceWorkerRegistrationScope);

  return notification.forget();
}

void Notification::MaybeNotifyClose() {
  if (mIsClosed) {
    return;
  }
  mIsClosed = true;
  DispatchTrustedEvent(u"close"_ns);
}

// https://notifications.spec.whatwg.org/#create-a-notification
already_AddRefed<Notification> Notification::CreateInternal(
    nsIGlobalObject* aGlobal, const nsAString& aID, const nsAString& aTitle,
    const NotificationOptions& aOptions, ErrorResult& aRv) {
  nsresult rv;
  nsString id;
  if (!aID.IsEmpty()) {
    id = aID;
  } else {
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
        do_GetService("@mozilla.org/uuid-generator;1");
    NS_ENSURE_TRUE(uuidgen, nullptr);
    nsID uuid;
    rv = uuidgen->GenerateUUIDInPlace(&uuid);
    NS_ENSURE_SUCCESS(rv, nullptr);

    char buffer[NSID_LENGTH];
    uuid.ToProvidedString(buffer);
    NS_ConvertASCIItoUTF16 convertedID(buffer);
    id = convertedID;
  }

  // Step 20: Set notification’s silent preference to options["silent"].
  bool silent = false;
  if (StaticPrefs::dom_webnotifications_silent_enabled()) {
    silent = aOptions.mSilent;
  }

  nsTArray<uint32_t> vibrate;
  if (StaticPrefs::dom_webnotifications_vibrate_enabled() &&
      aOptions.mVibrate.WasPassed()) {
    // Step 4: If options["silent"] is true and options["vibrate"] exists, then
    // throw a TypeError.
    if (silent) {
      aRv.ThrowTypeError(
          "Silent notifications must not specify vibration patterns.");
      return nullptr;
    }

    // Step 17: If options["vibrate"] exists, then validate and normalize it and
    // set notification’s vibration pattern to the return value.
    const OwningUnsignedLongOrUnsignedLongSequence& value =
        aOptions.mVibrate.Value();
    if (value.IsUnsignedLong()) {
      AutoTArray<uint32_t, 1> array;
      array.AppendElement(value.GetAsUnsignedLong());
      vibrate = SanitizeVibratePattern(array);
    } else {
      vibrate = SanitizeVibratePattern(value.GetAsUnsignedLongSequence());
    }
  }

  // Step 15: If options["icon"] exists, then parse it using baseURL, and if
  // that does not return failure, set notification’s icon URL to the return
  // value. (Otherwise icon URL is not set.)
  nsString iconUrl = aOptions.mIcon;
  NotificationBehavior behavior{aOptions.mMozbehavior};
  ResolveIconAndSoundURL(aGlobal, iconUrl, behavior.mSoundFile);

  RefPtr<Notification> notification = new Notification(
      aGlobal, id, aTitle, aOptions.mBody, aOptions.mDir, aOptions.mLang,
      aOptions.mTag, iconUrl, aOptions.mRequireInteraction, silent,
      std::move(vibrate), behavior);
  return notification.forget();
}

Notification::~Notification() { mozilla::DropJSObjects(this); }

NS_IMPL_CYCLE_COLLECTION_CLASS(Notification)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Notification,
                                                DOMEventTargetHelper)
  tmp->mData.setUndefined();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_PTR
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Notification,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(Notification,
                                               DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(Notification, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Notification, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Notification)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

/* static */
bool Notification::RequestPermissionEnabledForScope(JSContext* aCx,
                                                    JSObject* /* unused */) {
  // requestPermission() is not allowed on workers. The calling page should ask
  // for permission on the worker's behalf. This is to prevent 'which window
  // should show the browser pop-up'. See discussion:
  // http://lists.whatwg.org/pipermail/whatwg-whatwg.org/2013-October/041272.html
  return NS_IsMainThread();
}

// static
already_AddRefed<Promise> Notification::RequestPermission(
    const GlobalObject& aGlobal,
    const Optional<OwningNonNull<NotificationPermissionCallback> >& aCallback,
    ErrorResult& aRv) {
  AssertIsOnMainThread();

  // Get principal from global to make permission request for notifications.
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  nsCOMPtr<nsIScriptObjectPrincipal> sop =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!sop || !window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  nsCOMPtr<nsIPrincipal> effectiveStoragePrincipal =
      sop->GetEffectiveStoragePrincipal();
  if (!principal || !effectiveStoragePrincipal) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(window->AsGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  NotificationPermissionCallback* permissionCallback = nullptr;
  if (aCallback.WasPassed()) {
    permissionCallback = &aCallback.Value();
  }
  nsCOMPtr<nsIRunnable> request =
      new NotificationPermissionRequest(principal, effectiveStoragePrincipal,
                                        window, promise, permissionCallback);

  window->AsGlobal()->Dispatch(request.forget());

  return promise.forget();
}

// static
NotificationPermission Notification::GetPermission(const GlobalObject& aGlobal,
                                                   ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  return GetPermission(global, PermissionCheckPurpose::PermissionAttribute,
                       aRv);
}

// static
NotificationPermission Notification::GetPermission(
    nsIGlobalObject* aGlobal, PermissionCheckPurpose aPurpose,
    ErrorResult& aRv) {
  if (NS_IsMainThread()) {
    return GetPermissionInternal(aGlobal->GetAsInnerWindow(), aPurpose, aRv);
  }

  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  RefPtr<GetPermissionRunnable> r = new GetPermissionRunnable(
      worker, worker->UseRegularPrincipal(), aPurpose);
  r->Dispatch(worker, Canceling, aRv);
  if (aRv.Failed()) {
    return NotificationPermission::Denied;
  }

  return r->GetPermission();
}

/* static */
NotificationPermission Notification::GetPermissionInternal(
    nsPIDOMWindowInner* aWindow, PermissionCheckPurpose aPurpose,
    ErrorResult& aRv) {
  // Get principal from global to check permission for notifications.
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aWindow);
  if (!sop) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return NotificationPermission::Denied;
  }

  nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
  nsCOMPtr<nsIPrincipal> effectiveStoragePrincipal =
      sop->GetEffectiveStoragePrincipal();
  if (!principal || !effectiveStoragePrincipal) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return NotificationPermission::Denied;
  }

  return GetNotificationPermission(principal, effectiveStoragePrincipal,
                                   aWindow->IsSecureContext(), aPurpose);
}

nsresult Notification::ResolveIconAndSoundURL(nsIGlobalObject* aGlobal,
                                              nsString& iconUrl,
                                              nsString& soundUrl) {
  nsresult rv = NS_OK;

  nsCOMPtr<nsIURI> baseUri = nullptr;

  // XXXnsm If I understand correctly, the character encoding for resolving
  // URIs in new specs is dictated by the URL spec, which states that unless
  // the URL parser is passed an override encoding, the charset to be used is
  // UTF-8. The new Notification icon/sound specification just says to use the
  // Fetch API, where the Request constructor defers to URL parsing specifying
  // the API base URL and no override encoding. So we've to use UTF-8 on
  // workers, but for backwards compat keeping it document charset on main
  // thread.
  auto encoding = UTF_8_ENCODING;

  if (nsCOMPtr<nsPIDOMWindowInner> window = aGlobal->GetAsInnerWindow()) {
    if (RefPtr<Document> doc = window->GetExtantDoc()) {
      baseUri = doc->GetBaseURI();
      encoding = doc->GetDocumentCharacterSet();
    } else {
      NS_WARNING("No document found for main thread notification!");
      return NS_ERROR_FAILURE;
    }
  } else if (WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate()) {
    baseUri = workerPrivate->GetBaseURI();
  }

  if (baseUri) {
    if (iconUrl.Length() > 0) {
      nsCOMPtr<nsIURI> srcUri;
      rv = NS_NewURI(getter_AddRefs(srcUri), iconUrl, encoding, baseUri);
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString src;
        srcUri->GetSpec(src);
        CopyUTF8toUTF16(src, iconUrl);
      }
    }
    if (soundUrl.Length() > 0) {
      nsCOMPtr<nsIURI> srcUri;
      rv = NS_NewURI(getter_AddRefs(srcUri), soundUrl, encoding, baseUri);
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString src;
        srcUri->GetSpec(src);
        CopyUTF8toUTF16(src, soundUrl);
      }
    }
  }

  return rv;
}

already_AddRefed<Promise> Notification::Get(
    nsPIDOMWindowInner* aWindow, const GetNotificationOptions& aFilter,
    const nsAString& aScope, ErrorResult& aRv) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  nsCOMPtr<Document> doc = aWindow->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsString origin;
  aRv = GetOrigin(doc->NodePrincipal(), origin);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(aWindow->AsGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsINotificationStorageCallback> callback =
      new NotificationStorageCallback(aWindow->AsGlobal(), aScope, promise);

  RefPtr<NotificationGetRunnable> r = new NotificationGetRunnable(
      origin, aFilter.mTag, callback, doc->IsInPrivateBrowsing());

  aRv = aWindow->AsGlobal()->Dispatch(r.forget());
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return promise.forget();
}

class WorkerGetResultRunnable final : public NotificationWorkerRunnable {
  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  const nsTArray<NotificationStrings> mStrings;

 public:
  WorkerGetResultRunnable(WorkerPrivate* aWorkerPrivate,
                          PromiseWorkerProxy* aPromiseProxy,
                          nsTArray<NotificationStrings>&& aStrings)
      : NotificationWorkerRunnable(aWorkerPrivate, "WorkerGetResultRunnable"),
        mPromiseProxy(aPromiseProxy),
        mStrings(std::move(aStrings)) {}

  void WorkerRunInternal(WorkerPrivate* aWorkerPrivate) override {
    RefPtr<Promise> workerPromise = mPromiseProxy->GetWorkerPromise();
    // Once Worker had already started shutdown, workerPromise would be nullptr
    if (!workerPromise) {
      return;
    }

    AutoTArray<RefPtr<Notification>, 5> notifications;
    for (uint32_t i = 0; i < mStrings.Length(); ++i) {
      auto result = Notification::ConstructFromFields(
          aWorkerPrivate->GlobalScope(), mStrings[i].mID, mStrings[i].mTitle,
          mStrings[i].mDir, mStrings[i].mLang, mStrings[i].mBody,
          mStrings[i].mTag, mStrings[i].mIcon, mStrings[i].mData,
          /* mStrings[i].mBehavior, not
           * supported */
          mStrings[i].mServiceWorkerRegistrationScope);
      if (result.isErr()) {
        continue;
      }
      RefPtr<Notification> n = result.unwrap();
      notifications.AppendElement(n.forget());
    }

    workerPromise->MaybeResolve(notifications);
    mPromiseProxy->CleanUp();
  }
};

class WorkerGetCallback final : public ScopeCheckingGetCallback {
  RefPtr<PromiseWorkerProxy> mPromiseProxy;

 public:
  NS_DECL_ISUPPORTS

  WorkerGetCallback(PromiseWorkerProxy* aProxy, const nsAString& aScope)
      : ScopeCheckingGetCallback(aScope), mPromiseProxy(aProxy) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aProxy);
  }

  NS_IMETHOD Done() final {
    AssertIsOnMainThread();
    MOZ_ASSERT(mPromiseProxy, "Was Done() called twice?");

    RefPtr<PromiseWorkerProxy> proxy = std::move(mPromiseProxy);
    MutexAutoLock lock(proxy->Lock());
    if (proxy->CleanedUp()) {
      return NS_OK;
    }

    RefPtr<WorkerGetResultRunnable> r = new WorkerGetResultRunnable(
        proxy->GetWorkerPrivate(), proxy, std::move(mStrings));

    r->Dispatch(proxy->GetWorkerPrivate());
    return NS_OK;
  }

 private:
  ~WorkerGetCallback() = default;
};

NS_IMPL_ISUPPORTS(WorkerGetCallback, nsINotificationStorageCallback)

class WorkerGetRunnable final : public Runnable {
  RefPtr<PromiseWorkerProxy> mPromiseProxy;
  const nsString mTag;
  const nsString mScope;

 public:
  WorkerGetRunnable(PromiseWorkerProxy* aProxy, const nsAString& aTag,
                    const nsAString& aScope)
      : Runnable("WorkerGetRunnable"),
        mPromiseProxy(aProxy),
        mTag(aTag),
        mScope(aScope) {
    MOZ_ASSERT(mPromiseProxy);
  }

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();

    MutexAutoLock lock(mPromiseProxy->Lock());
    if (mPromiseProxy->CleanedUp()) {
      return NS_OK;
    }

    auto* principal = mPromiseProxy->GetWorkerPrivate()->GetPrincipal();
    auto isPrivate = principal->GetIsInPrivateBrowsing();

    nsCOMPtr<nsINotificationStorageCallback> callback =
        new WorkerGetCallback(mPromiseProxy, mScope);

    nsCOMPtr<nsINotificationStorage> notificationStorage =
        GetNotificationStorage(isPrivate);
    if (NS_WARN_IF(!notificationStorage)) {
      callback->Done();
      return NS_ERROR_UNEXPECTED;
    }
    nsString origin;
    nsresult rv = GetOrigin(principal, origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      callback->Done();
      return rv;
    }

    rv = notificationStorage->Get(origin, mTag, callback);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      callback->Done();
      return rv;
    }

    return NS_OK;
  }

 private:
  ~WorkerGetRunnable() = default;
};

// static
already_AddRefed<Promise> Notification::WorkerGet(
    WorkerPrivate* aWorkerPrivate, const GetNotificationOptions& aFilter,
    const nsAString& aScope, ErrorResult& aRv) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
  RefPtr<Promise> p = Promise::Create(aWorkerPrivate->GlobalScope(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<PromiseWorkerProxy> proxy =
      PromiseWorkerProxy::Create(aWorkerPrivate, p);
  if (!proxy) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return nullptr;
  }

  RefPtr<WorkerGetRunnable> r =
      new WorkerGetRunnable(proxy, aFilter.mTag, aScope);
  // Since this is called from script via
  // ServiceWorkerRegistration::GetNotifications, we can assert dispatch.
  MOZ_ALWAYS_SUCCEEDS(aWorkerPrivate->DispatchToMainThread(r.forget()));
  return p.forget();
}

JSObject* Notification::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::Notification_Binding::Wrap(aCx, this, aGivenProto);
}

void Notification::Close() {
  if (mIsClosed) {
    return;
  }
  if (!mActor) {
    CreateActor();
  }
  if (mActor) {
    (void)mActor->SendClose();
  }
}

bool Notification::RequireInteraction() const { return mRequireInteraction; }

bool Notification::Silent() const { return mSilent; }

void Notification::GetVibrate(nsTArray<uint32_t>& aRetval) const {
  aRetval = mVibrate.Clone();
}

void Notification::GetData(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aRetval) {
  if (mData.isNull() && !mDataAsBase64.IsEmpty()) {
    nsresult rv;
    RefPtr<nsStructuredCloneContainer> container =
        new nsStructuredCloneContainer();
    rv = container->InitFromBase64(mDataAsBase64, JS_STRUCTURED_CLONE_VERSION);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRetval.setNull();
      return;
    }

    JS::Rooted<JS::Value> data(aCx);
    rv = container->DeserializeToJsval(aCx, &data);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRetval.setNull();
      return;
    }

    if (data.isGCThing()) {
      mozilla::HoldJSObjects(this);
    }
    mData = data;
  }
  if (mData.isNull()) {
    aRetval.setNull();
    return;
  }

  aRetval.set(mData);
}

void Notification::InitFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aData,
                                 ErrorResult& aRv) {
  if (!mDataAsBase64.IsEmpty() || aData.isNull()) {
    return;
  }
  RefPtr<nsStructuredCloneContainer> dataObjectContainer =
      new nsStructuredCloneContainer();
  aRv = dataObjectContainer->InitFromJSVal(aData, aCx);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  aRv = dataObjectContainer->GetDataAsBase64(mDataAsBase64);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }
}

Result<Ok, QMResult> Notification::InitFromBase64(const nsAString& aData) {
  MOZ_ASSERT(mDataAsBase64.IsEmpty());
  if (aData.IsEmpty()) {
    // No data; skipping
    return Ok();
  }

  // To and fro to ensure it is valid base64.
  RefPtr<nsStructuredCloneContainer> container =
      new nsStructuredCloneContainer();
  QM_TRY(QM_TO_RESULT(
      container->InitFromBase64(aData, JS_STRUCTURED_CLONE_VERSION)));
  QM_TRY(QM_TO_RESULT(container->GetDataAsBase64(mDataAsBase64)));

  return Ok();
}

// Steps 2-5 of
// https://notifications.spec.whatwg.org/#dom-serviceworkerregistration-shownotification
/* static */
already_AddRefed<Promise> Notification::ShowPersistentNotification(
    JSContext* aCx, nsIGlobalObject* aGlobal, const nsAString& aScope,
    const nsAString& aTitle, const NotificationOptions& aOptions,
    const ServiceWorkerRegistrationDescriptor& aDescriptor, ErrorResult& aRv) {
  MOZ_ASSERT(aGlobal);

  // Step 2: Let promise be a new promise in this’s relevant Realm.
  RefPtr<Promise> p = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Step 3: If this’s active worker is null, then reject promise with a
  // TypeError and return promise.
  if (!aDescriptor.GetActive()) {
    aRv.ThrowTypeError<MSG_NO_ACTIVE_WORKER>(NS_ConvertUTF16toUTF8(aScope));
    return nullptr;
  }

  // Step 4: Let notification be the result of creating a notification with a
  // settings object given title, options, and this’s relevant settings object.
  // If this threw an exception, then reject promise with that exception and
  // return promise.
  //
  // Step 5: Set notification’s service worker registration to this.
  //
  // Note: We currently use the scope as the unique identifier for the
  // registration (and there currently is no durable registration identifier,
  // so this is necessary), which is why we pass in the scope.  See
  // https://github.com/whatwg/notifications/issues/205 for some scope-related
  // discussion.
  //
  // XXX: We create Notification object almost solely to share the parameter
  // normalization steps. It would be nice to export that and skip creating
  // object here.
  RefPtr<Notification> notification =
      Create(aCx, aGlobal, aTitle, aOptions, aScope, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  if (!notification->CreateActor() || !notification->SendShow(p)) {
    return nullptr;
  }

  return p.forget();
}

/* static */
already_AddRefed<Notification> Notification::Create(
    JSContext* aCx, nsIGlobalObject* aGlobal, const nsAString& aTitle,
    const NotificationOptions& aOptions, const nsAString& aScope,
    ErrorResult& aRv) {
  MOZ_ASSERT(aGlobal);

  RefPtr<Notification> notification =
      CreateInternal(aGlobal, u""_ns, aTitle, aOptions, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Make a structured clone of the aOptions.mData object
  JS::Rooted<JS::Value> data(aCx, aOptions.mData);
  notification->InitFromJSVal(aCx, data, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  notification->SetScope(aScope);

  return notification.forget();
}

bool Notification::CreateActor() {
  mozilla::ipc::PBackgroundChild* backgroundActor =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  IPCNotificationOptions options(mTitle, mDir, mLang, mBody, mTag, mIconUrl,
                                 mRequireInteraction, mSilent, mVibrate,
                                 mDataAsBase64, mBehavior);

  // Note: We are not using the typical PBackground managed actor here as we
  // want the actor to be in the main thread of the main process. Instead we
  // pass the endpoint to PBackground, it dispatches a runnable to the main
  // thread, and the endpoint is bound there.

  mozilla::ipc::Endpoint<notification::PNotificationParent> parentEndpoint;
  mozilla::ipc::Endpoint<notification::PNotificationChild> childEndpoint;
  notification::PNotification::CreateEndpoints(&parentEndpoint, &childEndpoint);

  bool persistent = !mScope.IsEmpty();
  RefPtr<nsPIDOMWindowInner> window = GetOwnerWindow();
  mActor = new notification::NotificationChild(
      persistent ? nullptr : this,
      window ? window->GetWindowGlobalChild() : nullptr);

  nsISerialEventTarget* target = nullptr;
  nsIPrincipal* principal;
  nsIPrincipal* effectiveStoragePrincipal;
  bool isSecureContext;

  // TODO: Should get nsIGlobalObject methods for each method
  if (WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate()) {
    target = workerPrivate->HybridEventTarget();
    principal = workerPrivate->GetPrincipal();
    effectiveStoragePrincipal = workerPrivate->GetEffectiveStoragePrincipal();
    isSecureContext = workerPrivate->IsSecureContext();
  } else {
    nsGlobalWindowInner* win = GetOwnerWindow();
    NS_ENSURE_TRUE(win, false);
    principal = win->GetPrincipal();
    effectiveStoragePrincipal = win->GetEffectiveStoragePrincipal();
    isSecureContext = win->IsSecureContext();
  }

  if (!childEndpoint.Bind(mActor, target)) {
    return false;
  }

  (void)backgroundActor->SendCreateNotificationParent(
      std::move(parentEndpoint), WrapNotNull(principal),
      WrapNotNull(effectiveStoragePrincipal), isSecureContext, mID, mScope,
      options);

  return true;
}

bool Notification::SendShow(Promise* aPromise) {
  mActor->SendShow()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [self = RefPtr{this}, promise = RefPtr(aPromise)](
          notification::PNotificationChild::ShowPromise::ResolveOrRejectValue&&
              aResult) {
        if (aResult.IsReject()) {
          promise->MaybeRejectWithUnknownError("Failed to open notification");
          self->Deactivate();
          return;
        }

        CopyableErrorResult rv = aResult.ResolveValue();
        if (rv.Failed()) {
          promise->MaybeReject(std::move(rv));
          self->Deactivate();
          return;
        }

        if (promise) {
          promise->MaybeResolveWithUndefined();
        } else {
          self->DispatchTrustedEvent(u"show"_ns);
        }
      });

  return true;
}

void Notification::Deactivate() {
  IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onclick);
  IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onshow);
  IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onerror);
  IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onclose);
  mIsClosed = true;
  if (mActor) {
    mActor->Close();
    mActor = nullptr;
  }
}

nsresult Notification::DispatchToMainThread(
    already_AddRefed<nsIRunnable>&& aRunnable) {
  if (WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate()) {
    return workerPrivate->DispatchToMainThread(std::move(aRunnable));
  }
  AssertIsOnMainThread();
  return NS_DispatchToCurrentThread(std::move(aRunnable));
}

}  // namespace mozilla::dom
