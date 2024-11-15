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

using namespace mozilla::dom::notification;

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
      n->SetStoredState(true);
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
class ReleaseNotificationControlRunnable final
    : public MainThreadWorkerControlRunnable {
  Notification* mNotification;

 public:
  explicit ReleaseNotificationControlRunnable(Notification* aNotification)
      : MainThreadWorkerControlRunnable("ReleaseNotificationControlRunnable"),
        mNotification(aNotification) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    mNotification->ReleaseObject();
    return true;
  }
};

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

class FocusWindowRunnable final : public Runnable {
  nsMainThreadPtrHandle<nsPIDOMWindowInner> mWindow;

 public:
  explicit FocusWindowRunnable(
      const nsMainThreadPtrHandle<nsPIDOMWindowInner>& aWindow)
      : Runnable("FocusWindowRunnable"), mWindow(aWindow) {}

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
  // bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override {
    AssertIsOnMainThread();
    if (!mWindow->IsCurrentInnerWindow()) {
      // Window has been closed, this observer is not valid anymore
      return NS_OK;
    }

    nsCOMPtr<nsPIDOMWindowOuter> outerWindow = mWindow->GetOuterWindow();
    nsFocusManager::FocusWindow(outerWindow, CallerType::System);
    return NS_OK;
  }
};

nsresult CheckScope(nsIPrincipal* aPrincipal, const nsACString& aScope,
                    uint64_t aWindowID) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  nsCOMPtr<nsIURI> scopeURI;
  nsresult rv = NS_NewURI(getter_AddRefs(scopeURI), aScope);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return aPrincipal->CheckMayLoadWithReporting(
      scopeURI,
      /* allowIfInheritsPrincipal = */ false, aWindowID);
}
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

// Overrides dispatch and run handlers so we can directly dispatch from main
// thread to child workers.
class NotificationEventWorkerRunnable final
    : public NotificationWorkerRunnable {
  Notification* mNotification;
  const nsString mEventName;

 public:
  NotificationEventWorkerRunnable(Notification* aNotification,
                                  const nsString& aEventName)
      : NotificationWorkerRunnable(aNotification->mWorkerPrivate,
                                   "NotificationEventWorkerRunnable"),
        mNotification(aNotification),
        mEventName(aEventName) {}

  void WorkerRunInternal(WorkerPrivate* aWorkerPrivate) override {
    mNotification->DispatchTrustedEvent(mEventName);
  }
};

class ReleaseNotificationRunnable final : public NotificationWorkerRunnable {
  Notification* mNotification;

 public:
  explicit ReleaseNotificationRunnable(Notification* aNotification)
      : NotificationWorkerRunnable(aNotification->mWorkerPrivate,
                                   "ReleaseNotificationRunnable"),
        mNotification(aNotification) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();
    // ReleaseNotificationRunnable is only used in StrongWorkerRef's shutdown
    // callback. At the moment, it is supposed to executing
    // mNotification->ReleaseObject() safely even though the corresponding
    // WorkerScope::IsDying() is true. It is unlike other
    // NotificationWorkerRunnable.
    WorkerRunInternal(aWorkerPrivate);
    return true;
  }

  void WorkerRunInternal(WorkerPrivate* aWorkerPrivate) override {
    mNotification->ReleaseObject();
  }

  nsresult Cancel() override {
    mNotification->ReleaseObject();
    return NS_OK;
  }
};

// Create one whenever you require ownership of the notification. Use with
// UniquePtr<>. See Notification.h for details.
class NotificationRef final {
  friend class WorkerNotificationObserver;

 private:
  Notification* mNotification;
  bool mInited;

  // Only useful for workers.
  void Forget() { mNotification = nullptr; }

 public:
  explicit NotificationRef(Notification* aNotification)
      : mNotification(aNotification) {
    MOZ_ASSERT(mNotification);
    if (mNotification->mWorkerPrivate) {
      mNotification->mWorkerPrivate->AssertIsOnWorkerThread();
    } else {
      AssertIsOnMainThread();
    }

    mInited = mNotification->AddRefObject();
  }

  // This is only required because Gecko runs script in a worker's onclose
  // handler (non-standard, Bug 790919) where calls to HoldWorker() will
  // fail. Due to non-standardness and added complications if we decide to
  // support this, attempts to create a Notification in onclose just throw
  // exceptions.
  bool Initialized() { return mInited; }

  ~NotificationRef() {
    if (Initialized() && mNotification) {
      Notification* notification = mNotification;
      mNotification = nullptr;
      if (notification->mWorkerPrivate && NS_IsMainThread()) {
        // Try to pass ownership back to the worker. If the dispatch succeeds we
        // are guaranteed this runnable will run, and that it will run after
        // queued event runnables, so event runnables will have a safe pointer
        // to the Notification.
        //
        // If the dispatch fails, the worker isn't running anymore and the event
        // runnables have already run or been canceled. We can use a control
        // runnable to release the reference.
        RefPtr<ReleaseNotificationRunnable> r =
            new ReleaseNotificationRunnable(notification);

        if (!r->Dispatch(notification->mWorkerPrivate)) {
          RefPtr<ReleaseNotificationControlRunnable> r =
              new ReleaseNotificationControlRunnable(notification);
          MOZ_ALWAYS_TRUE(r->Dispatch(notification->mWorkerPrivate));
        }
      } else {
        notification->AssertIsOnTargetThread();
        notification->ReleaseObject();
      }
    }
  }

  // XXXnsm, is it worth having some sort of WeakPtr like wrapper instead of
  // a rawptr that the NotificationRef can invalidate?
  Notification* GetNotification() {
    MOZ_ASSERT(Initialized());
    return mNotification;
  }
};

class NotificationTask : public Runnable {
 public:
  enum NotificationAction { eShow, eClose };

  NotificationTask(const char* aName, UniquePtr<NotificationRef> aRef,
                   NotificationAction aAction)
      : Runnable(aName), mNotificationRef(std::move(aRef)), mAction(aAction) {}

  NS_IMETHOD
  Run() override;

 protected:
  virtual ~NotificationTask() = default;

  UniquePtr<NotificationRef> mNotificationRef;
  NotificationAction mAction;
};

uint32_t Notification::sCount = 0;

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

// Observer that the alert service calls to do common tasks and/or dispatch to
// the specific observer for the context e.g. main thread, worker, or service
// worker.
class NotificationObserver final : public nsIObserver {
 public:
  nsCOMPtr<nsIObserver> mObserver;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  bool mInPrivateBrowsing;
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  NotificationObserver(nsIObserver* aObserver, nsIPrincipal* aPrincipal,
                       bool aInPrivateBrowsing)
      : mObserver(aObserver),
        mPrincipal(aPrincipal),
        mInPrivateBrowsing(aInPrivateBrowsing) {
    AssertIsOnMainThread();
    MOZ_ASSERT(mObserver);
    MOZ_ASSERT(mPrincipal);
  }

 protected:
  virtual ~NotificationObserver() { AssertIsOnMainThread(); }
};

NS_IMPL_ISUPPORTS(NotificationObserver, nsIObserver)

NS_IMETHODIMP
NotificationTask::Run() {
  AssertIsOnMainThread();

  // Get a pointer to notification before the notification takes ownership of
  // the ref (it owns itself temporarily, with ShowInternal() and
  // CloseInternal() passing on the ownership appropriately.)
  Notification* notif = mNotificationRef->GetNotification();
  notif->mTempRef.swap(mNotificationRef);
  if (mAction == eShow) {
    notif->ShowInternal();
  } else if (mAction == eClose) {
    notif->CloseInternal();
  } else {
    MOZ_CRASH("Invalid action");
  }

  MOZ_ASSERT(!mNotificationRef);
  return NS_OK;
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
      mWorkerPrivate(nullptr),
      mObserver(nullptr),
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
      mData(JS::NullValue()),
      mIsClosed(false),
      mIsStored(false),
      mTaskCount(0) {
  if (!NS_IsMainThread() && mScope.IsEmpty()) {
    mWorkerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerUseRegularPrincipal = mWorkerPrivate->UseRegularPrincipal();
  }
}

void Notification::SetAlertName() {
  AssertIsOnMainThread();
  if (!mAlertName.IsEmpty()) {
    return;
  }

  ComputeAlertName(GetPrincipal(), mTag, mID, mAlertName);
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

  if (!NS_IsMainThread()) {
    notification->ShowOnMainThread(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
    return notification.forget();
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

nsresult Notification::Persist() {
  AssertIsOnMainThread();

  nsString alertName;
  GetAlertName(alertName);

  IPCNotificationOptions options(mTitle, mDir, mLang, mBody, mTag, mIconUrl,
                                 mRequireInteraction, mSilent, mVibrate,
                                 mDataAsBase64, mBehavior);

  nsresult rv =
      PersistNotification(GetPrincipal(), mID, alertName, options, mScope);

  if (NS_FAILED(rv)) {
    return rv;
  }

  SetStoredState(true);
  return NS_OK;
}

void Notification::Unpersist() {
  AssertIsOnMainThread();
  if (IsStored()) {
    UnpersistNotification(GetPrincipal(), mID);
  }
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

Notification::~Notification() {
  mozilla::DropJSObjects(this);
  AssertIsOnTargetThread();
  MOZ_ASSERT(!mWorkerRef);
  MOZ_ASSERT(!mTempRef);
}

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

nsIPrincipal* Notification::GetPrincipal() {
  AssertIsOnMainThread();
  if (mWorkerPrivate) {
    return mWorkerPrivate->GetPrincipal();
  }
  nsGlobalWindowInner* win = GetOwnerWindow();
  NS_ENSURE_TRUE(win, nullptr);
  return win->GetPrincipal();
}

class WorkerNotificationObserver : public nsIObserver {
 public:
  UniquePtr<NotificationRef> mNotificationRef;
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  explicit WorkerNotificationObserver(UniquePtr<NotificationRef> aRef)
      : mNotificationRef(std::move(aRef)) {
    AssertIsOnMainThread();
    MOZ_ASSERT(mNotificationRef->GetNotification()->mWorkerPrivate);
  }

  void ForgetNotification() {
    AssertIsOnMainThread();
    mNotificationRef->Forget();
  }

 protected:
  virtual ~WorkerNotificationObserver() {
    AssertIsOnMainThread();

    MOZ_ASSERT(mNotificationRef);
    Notification* notification = mNotificationRef->GetNotification();
    if (notification) {
      notification->mObserver = nullptr;
    }
  }
};

NS_IMPL_ISUPPORTS(WorkerNotificationObserver, nsIObserver)

bool Notification::DispatchClickEvent() {
  AssertIsOnTargetThread();
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(u"click"_ns, false, true);
  event->SetTrusted(true);
  WantsPopupControlCheck popupControlCheck(event);
  return DispatchEvent(*event, CallerType::System, IgnoreErrors());
}

// Overrides dispatch and run handlers so we can directly dispatch from main
// thread to child workers.
class NotificationClickWorkerRunnable final
    : public NotificationWorkerRunnable {
  Notification* mNotification;
  // Optional window that gets focused if click event is not
  // preventDefault()ed.
  nsMainThreadPtrHandle<nsPIDOMWindowInner> mWindow;

 public:
  NotificationClickWorkerRunnable(
      Notification* aNotification,
      const nsMainThreadPtrHandle<nsPIDOMWindowInner>& aWindow)
      : NotificationWorkerRunnable(aNotification->mWorkerPrivate,
                                   "NotificationClickWorkerRunnable"),
        mNotification(aNotification),
        mWindow(aWindow) {
    MOZ_ASSERT_IF(mNotification->mWorkerPrivate->IsServiceWorker(), !mWindow);
  }

  void WorkerRunInternal(WorkerPrivate* aWorkerPrivate) override {
    bool doDefaultAction = mNotification->DispatchClickEvent();
    MOZ_ASSERT_IF(mNotification->mWorkerPrivate->IsServiceWorker(),
                  !doDefaultAction);
    if (doDefaultAction) {
      RefPtr<FocusWindowRunnable> r = new FocusWindowRunnable(mWindow);
      mNotification->mWorkerPrivate->DispatchToMainThread(r.forget());
    }
  }
};

NS_IMETHODIMP
NotificationObserver::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  AssertIsOnMainThread();

  if (!strcmp("alertdisablecallback", aTopic)) {
    if (XRE_IsParentProcess()) {
      return RemovePermission(mPrincipal);
    }
    // Permissions can't be removed from the content process. Send a message
    // to the parent; `ContentParent::RecvDisableNotifications` will call
    // `RemovePermission`.
    ContentChild::GetSingleton()->SendDisableNotifications(mPrincipal);
    return NS_OK;
  }
  if (!strcmp("alertsettingscallback", aTopic)) {
    if (XRE_IsParentProcess()) {
      return OpenSettings(mPrincipal);
    }
    // `ContentParent::RecvOpenNotificationSettings` notifies observers in the
    // parent process.
    ContentChild::GetSingleton()->SendOpenNotificationSettings(mPrincipal);
    return NS_OK;
  }
  if (!strcmp("alertshow", aTopic)) {
    (void)NS_WARN_IF(NS_FAILED(
        AdjustPushQuota(mPrincipal, NotificationStatusChange::Shown)));
  }
  if (!strcmp("alertfinished", aTopic)) {
    (void)NS_WARN_IF(NS_FAILED(
        AdjustPushQuota(mPrincipal, NotificationStatusChange::Closed)));
  }

  return mObserver->Observe(aSubject, aTopic, aData);
}

NS_IMETHODIMP
WorkerNotificationObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData) {
  AssertIsOnMainThread();
  MOZ_ASSERT(mNotificationRef);
  // For an explanation of why it is OK to pass this rawptr to the event
  // runnables, see the Notification class comment.
  Notification* notification = mNotificationRef->GetNotification();
  // We can't assert notification here since the feature could've unset it.
  if (NS_WARN_IF(!notification)) {
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(notification->mWorkerPrivate);

  RefPtr<WorkerThreadRunnable> r;
  if (!strcmp("alertclickcallback", aTopic)) {
    nsPIDOMWindowInner* window = nullptr;
    if (!notification->mWorkerPrivate->IsServiceWorker()) {
      window = notification->mWorkerPrivate->GetAncestorWindow();
      if (NS_WARN_IF(!window || !window->IsCurrentInnerWindow())) {
        // Window has been closed, this observer is not valid anymore
        return NS_ERROR_FAILURE;
      }
    }

    // Instead of bothering with adding features and other worker lifecycle
    // management, we simply hold strongrefs to the window and document.
    nsMainThreadPtrHandle<nsPIDOMWindowInner> windowHandle(
        new nsMainThreadPtrHolder<nsPIDOMWindowInner>(
            "WorkerNotificationObserver::Observe::nsPIDOMWindowInner", window));

    r = new NotificationClickWorkerRunnable(notification, windowHandle);
  } else if (!strcmp("alertfinished", aTopic)) {
    notification->Unpersist();
    notification->mIsClosed = true;
    r = new NotificationEventWorkerRunnable(notification, u"close"_ns);
  } else if (!strcmp("alertshow", aTopic)) {
    r = new NotificationEventWorkerRunnable(notification, u"show"_ns);
  }

  MOZ_ASSERT(r);
  if (!r->Dispatch(notification->mWorkerPrivate)) {
    NS_WARNING("Could not dispatch event to worker notification");
  }
  return NS_OK;
}

bool Notification::IsInPrivateBrowsing() {
  AssertIsOnMainThread();

  Document* doc = nullptr;

  if (mWorkerPrivate) {
    doc = mWorkerPrivate->GetDocument();
  } else if (nsGlobalWindowInner* win = GetOwnerWindow()) {
    doc = win->GetExtantDoc();
  }

  if (doc) {
    nsCOMPtr<nsILoadContext> loadContext = doc->GetLoadContext();
    return loadContext && loadContext->UsePrivateBrowsing();
  }

  if (mWorkerPrivate) {
    // Not all workers may have a document, but with Bug 1107516 fixed, they
    // should all have a loadcontext.
    nsCOMPtr<nsILoadGroup> loadGroup = mWorkerPrivate->GetLoadGroup();
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(nullptr, loadGroup,
                                  NS_GET_IID(nsILoadContext),
                                  getter_AddRefs(loadContext));
    return loadContext && loadContext->UsePrivateBrowsing();
  }

  // XXXnsm Should this default to true?
  return false;
}

// Step 4 of
// https://notifications.spec.whatwg.org/#dom-notification-notification
void Notification::ShowInternal() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mTempRef,
             "Notification should take ownership of itself before"
             "calling ShowInternal!");
  // A notification can only have one observer and one call to ShowInternal.
  MOZ_ASSERT(!mObserver);

  // Transfer ownership to local scope so we can either release it at the end
  // of this function or transfer it to the observer.
  UniquePtr<NotificationRef> ownership;
  std::swap(ownership, mTempRef);
  MOZ_ASSERT(ownership->GetNotification() == this);

  nsCOMPtr<nsIAlertsService> alertService = components::Alerts::Service();

  // Step 4.1: If the result of getting the notifications permission state is
  // not "granted", then queue a task to fire an event named error on this, and
  // abort these steps.
  //
  // XXX(krosylight): But this function is also triggered by
  // Notification::ShowPersistentNotification which already does its own
  // permission check. Can we split this?
  ErrorResult result;
  NotificationPermission permission = NotificationPermission::Denied;
  if (mWorkerPrivate) {
    nsIPrincipal* principal = mWorkerPrivate->GetPrincipal();
    nsIPrincipal* effectiveStoragePrincipal =
        mWorkerUseRegularPrincipal ? principal
                                   : mWorkerPrivate->GetPartitionedPrincipal();
    permission = GetNotificationPermission(
        principal, effectiveStoragePrincipal, mWorkerPrivate->IsSecureContext(),
        PermissionCheckPurpose::NotificationShow);
  } else {
    permission = GetPermissionInternal(
        GetOwnerWindow(), PermissionCheckPurpose::NotificationShow, result);
  }
  // We rely on GetPermissionInternal returning Denied on all failure codepaths.
  MOZ_ASSERT_IF(result.Failed(), permission == NotificationPermission::Denied);
  result.SuppressException();
  if (permission != NotificationPermission::Granted || !alertService) {
    if (mWorkerPrivate) {
      RefPtr<NotificationEventWorkerRunnable> r =
          new NotificationEventWorkerRunnable(this, u"error"_ns);
      if (!r->Dispatch(mWorkerPrivate)) {
        NS_WARNING("Could not dispatch event to worker notification");
      }
    } else {
      DispatchTrustedEvent(u"error"_ns);
    }
    mIsClosed = true;
    return;
  }

  // Step 4.3 the show steps, which are almost all about processing `tag` and
  // then displaying the notification. Both are handled by
  // nsIAlertsService::ShowAlert/PersistentNotification. The below is all about
  // constructing the observer (for show and close events) right and ultimately
  // call the alerts service function.

  // XXX(krosylight): Non-persistent notifications probably don't need this
  nsresult rv = Persist();
  if (NS_FAILED(rv)) {
    NS_WARNING("Could not persist Notification");
  }

  nsCOMPtr<nsIObserver> observer;
  MOZ_ASSERT(mScope.IsEmpty());
  MOZ_ASSERT(mWorkerPrivate);
  // Ownership passed to observer.
  // Scope better be set on ServiceWorker initiated requests.
  MOZ_ASSERT(!mWorkerPrivate->IsServiceWorker());
  // Keep a pointer so that the feature can tell the observer not to release
  // the notification.
  mObserver = new WorkerNotificationObserver(std::move(ownership));
  observer = mObserver;
  MOZ_ASSERT(observer);
  nsCOMPtr<nsIObserver> alertObserver =
      new NotificationObserver(observer, GetPrincipal(), IsInPrivateBrowsing());

  // In the case of IPC, the parent process uses the cookie to map to
  // nsIObserver. Thus the cookie must be unique to differentiate observers.
  nsString uniqueCookie = u"notification:"_ns;
  uniqueCookie.AppendInt(sCount++);
  bool inPrivateBrowsing = IsInPrivateBrowsing();

  bool requireInteraction = mRequireInteraction;
  if (!StaticPrefs::dom_webnotifications_requireinteraction_enabled()) {
    requireInteraction = false;
  }

  nsAutoString alertName;
  GetAlertName(alertName);
  nsCOMPtr<nsIAlertNotification> alert =
      do_CreateInstance(ALERT_NOTIFICATION_CONTRACTID);
  NS_ENSURE_TRUE_VOID(alert);
  rv = alert->Init(alertName, mIconUrl, mTitle, mBody, true, uniqueCookie,
                   NS_ConvertASCIItoUTF16(GetEnumString(mDir)), mLang,
                   mDataAsBase64, GetPrincipal(), inPrivateBrowsing,
                   requireInteraction, mSilent, mVibrate);
  NS_ENSURE_SUCCESS_VOID(rv);

  alertService->ShowAlert(alert, alertObserver);
}

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
      n->SetStoredState(true);
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
  AssertIsOnTargetThread();

  if (NS_IsMainThread()) {
    if (mIsClosed) {
      return;
    }
    if (!mActor) {
      CreateActor();
    }
    if (mActor) {
      (void)mActor->SendClose();
    }
    return;
  }

  auto ref = MakeUnique<NotificationRef>(this);
  if (!ref->Initialized()) {
    return;
  }

  nsCOMPtr<nsIRunnable> closeNotificationTask = new NotificationTask(
      "Notification::Close", std::move(ref), NotificationTask::eClose);
  nsresult rv = DispatchToMainThread(closeNotificationTask.forget());

  if (NS_FAILED(rv)) {
    DispatchTrustedEvent(u"error"_ns);
    // If dispatch fails, NotificationTask will release the ref when it goes
    // out of scope at the end of this function.
  }
}

void Notification::CloseInternal(bool aContextClosed) {
  AssertIsOnMainThread();
  // Transfer ownership (if any) to local scope so we can release it at the end
  // of this function. This is relevant when the call is from
  // NotificationTask::Run().
  UniquePtr<NotificationRef> ownership;
  std::swap(ownership, mTempRef);

  if (mIsClosed) {
    return;
  }

  nsAutoString alertName;
  GetAlertName(alertName);
  UnregisterNotification(
      GetPrincipal(), mID, alertName,
      aContextClosed ? CloseMode::InactiveGlobal : CloseMode::CloseMethod);
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

bool Notification::AddRefObject() {
  AssertIsOnTargetThread();
  MOZ_ASSERT_IF(mWorkerPrivate && !mWorkerRef, mTaskCount == 0);
  MOZ_ASSERT_IF(mWorkerPrivate && mWorkerRef, mTaskCount > 0);
  if (mWorkerPrivate && !mWorkerRef) {
    if (!CreateWorkerRef()) {
      return false;
    }
  }
  AddRef();
  ++mTaskCount;
  return true;
}

void Notification::ReleaseObject() {
  AssertIsOnTargetThread();
  MOZ_ASSERT(mTaskCount > 0);
  MOZ_ASSERT_IF(mWorkerPrivate, mWorkerRef);

  --mTaskCount;
  if (mWorkerPrivate && mTaskCount == 0) {
    MOZ_ASSERT(mWorkerRef);
    mWorkerRef = nullptr;
  }
  Release();
}

/*
 * Called from the worker, runs on main thread, blocks worker.
 *
 * We can freely access mNotification here because the feature supplied it and
 * the Notification owns the feature.
 */
class CloseNotificationRunnable final : public WorkerMainThreadRunnable {
  Notification* mNotification;
  bool mHadObserver;

 public:
  explicit CloseNotificationRunnable(Notification* aNotification)
      : WorkerMainThreadRunnable(aNotification->mWorkerPrivate,
                                 "Notification :: Close Notification"_ns),
        mNotification(aNotification),
        mHadObserver(false) {}

  bool MainThreadRun() override {
    if (mNotification->mObserver) {
      // The Notify() take's responsibility of releasing the Notification.
      mNotification->mObserver->ForgetNotification();
      mNotification->mObserver = nullptr;
      mHadObserver = true;
    }
    mNotification->CloseInternal();
    return true;
  }

  bool HadObserver() { return mHadObserver; }
};

bool Notification::CreateWorkerRef() {
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(!mWorkerRef);

  RefPtr<Notification> self = this;
  mWorkerRef =
      StrongWorkerRef::Create(mWorkerPrivate, "Notification", [self]() {
        // CloseNotificationRunnable blocks the worker by pushing a sync event
        // loop on the stack. Meanwhile, WorkerControlRunnables dispatched to
        // the worker can still continue running. One of these is
        // ReleaseNotificationControlRunnable that releases the notification,
        // invalidating the notification and this feature. We hold this
        // reference to keep the notification valid until we are done with it.
        //
        // An example of when the control runnable could get dispatched to the
        // worker is if a Notification is created and the worker is immediately
        // closed, but there is no permission to show it so that the main thread
        // immediately drops the NotificationRef. In this case, this function
        // blocks on the main thread, but the main thread dispatches the control
        // runnable, invalidating mNotification.

        // Dispatched to main thread, blocks on closing the Notification.
        RefPtr<CloseNotificationRunnable> r =
            new CloseNotificationRunnable(self);
        ErrorResult rv;
        r->Dispatch(self->mWorkerPrivate, Killing, rv);
        // XXXbz I'm told throwing and returning false from here is pointless
        // (and also that doing sync stuff from here is really weird), so I
        // guess we just suppress the exception on rv, if any.
        rv.SuppressException();

        // Only call ReleaseObject() to match the observer's NotificationRef
        // ownership (since CloseNotificationRunnable asked the observer to drop
        // the reference to the notification).
        if (r->HadObserver()) {
          self->ReleaseObject();
        }

        // From this point we cannot touch properties of this feature because
        // ReleaseObject() may have led to the notification going away and the
        // notification owns this feature!
      });

  if (NS_WARN_IF(!mWorkerRef)) {
    return false;
  }

  return true;
}

/*
 * Checks:
 * 1) Is aWorker allowed to show a notification for scope?
 * 2) Is aWorker an active worker?
 *
 * If it is not an active worker, Result() will be NS_ERROR_NOT_AVAILABLE.
 */
class CheckLoadRunnable final : public WorkerMainThreadRunnable {
  nsresult mRv;
  nsCString mScope;
  ServiceWorkerRegistrationDescriptor mDescriptor;

 public:
  explicit CheckLoadRunnable(
      WorkerPrivate* aWorker, const nsACString& aScope,
      const ServiceWorkerRegistrationDescriptor& aDescriptor)
      : WorkerMainThreadRunnable(aWorker, "Notification :: Check Load"_ns),
        mRv(NS_ERROR_DOM_SECURITY_ERR),
        mScope(aScope),
        mDescriptor(aDescriptor) {}

  bool MainThreadRun() override {
    MOZ_ASSERT(mWorkerRef);
    nsIPrincipal* principal = mWorkerRef->Private()->GetPrincipal();
    mRv = CheckScope(principal, mScope, mWorkerRef->Private()->WindowID());

    if (NS_FAILED(mRv)) {
      return true;
    }

    auto activeWorker = mDescriptor.GetActive();

    if (!activeWorker ||
        activeWorker.ref().Id() != mWorkerRef->Private()->ServiceWorkerID()) {
      mRv = NS_ERROR_NOT_AVAILABLE;
    }

    return true;
  }

  nsresult Result() { return mRv; }
};

// Step 2, 5, 6 of
// https://notifications.spec.whatwg.org/#dom-serviceworkerregistration-shownotification
/* static */
already_AddRefed<Promise> Notification::ShowPersistentNotification(
    JSContext* aCx, nsIGlobalObject* aGlobal, const nsAString& aScope,
    const nsAString& aTitle, const NotificationOptions& aOptions,
    const ServiceWorkerRegistrationDescriptor& aDescriptor, ErrorResult& aRv) {
  MOZ_ASSERT(aGlobal);

  // Validate scope.
  // XXXnsm: This may be slow due to blocking the worker and waiting on the main
  // thread. On calls from content, we can be sure the scope is valid since
  // ServiceWorkerRegistrations have their scope set correctly. Can this be made
  // debug only? The problem is that there would be different semantics in
  // debug and non-debug builds in such a case.
  if (NS_IsMainThread()) {
    nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(aGlobal);
    if (NS_WARN_IF(!sop)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    nsIPrincipal* principal = sop->GetPrincipal();
    if (NS_WARN_IF(!principal)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    uint64_t windowID = 0;
    nsCOMPtr<nsPIDOMWindowInner> win = do_QueryInterface(aGlobal);
    if (win) {
      windowID = win->WindowID();
    }

    aRv = CheckScope(principal, NS_ConvertUTF16toUTF8(aScope), windowID);
    if (NS_WARN_IF(aRv.Failed())) {
      aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      return nullptr;
    }
  } else {
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();

    RefPtr<CheckLoadRunnable> loadChecker = new CheckLoadRunnable(
        worker, NS_ConvertUTF16toUTF8(aScope), aDescriptor);
    loadChecker->Dispatch(worker, Canceling, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    if (NS_WARN_IF(NS_FAILED(loadChecker->Result()))) {
      if (loadChecker->Result() == NS_ERROR_NOT_AVAILABLE) {
        aRv.ThrowTypeError<MSG_NO_ACTIVE_WORKER>(NS_ConvertUTF16toUTF8(aScope));
      } else {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
      }
      return nullptr;
    }
  }

  // Step 2: Let promise be a new promise in this’s relevant Realm.
  RefPtr<Promise> p = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  // Step 5: Let notification be the result of creating a notification given
  // title, options, this’s relevant settings object, and
  // serviceWorkerRegistration. If this threw an exception, then reject promise
  // with that exception and return promise.
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

void Notification::ShowOnMainThread(ErrorResult& aRv) {
  auto ref = MakeUnique<NotificationRef>(this);
  if (NS_WARN_IF(!ref->Initialized())) {
    aRv.Throw(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Queue a task to show the notification.
  nsCOMPtr<nsIRunnable> showNotificationTask = new NotificationTask(
      "Notification::CreateAndShow", std::move(ref), NotificationTask::eShow);

  nsresult rv = DispatchToMainThread(showNotificationTask.forget());

  if (NS_WARN_IF(NS_FAILED(rv))) {
    DispatchTrustedEvent(u"error"_ns);
  }
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
  if (!childEndpoint.Bind(mActor)) {
    return false;
  }

  nsIPrincipal* principal;
  nsIPrincipal* effectiveStoragePrincipal;
  bool isSecureContext;

  // TODO: Should get nsIGlobalObject methods for each method
  if (WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate()) {
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

        promise->MaybeResolveWithUndefined();
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
  if (mWorkerPrivate) {
    return mWorkerPrivate->DispatchToMainThread(std::move(aRunnable));
  }
  AssertIsOnMainThread();
  return NS_DispatchToCurrentThread(std::move(aRunnable));
}

}  // namespace mozilla::dom
