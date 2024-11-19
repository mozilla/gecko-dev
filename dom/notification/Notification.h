/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_notification_h__
#define mozilla_dom_notification_h__

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/NotificationBinding.h"
#include "mozilla/dom/notification/NotificationChild.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/quota/QuotaCommon.h"

#include "nsISupports.h"

#include "nsCycleCollectionParticipant.h"

class nsIPrincipal;
class nsIVariant;

namespace mozilla::dom {

class NotificationRef;
class WorkerNotificationObserver;
class Promise;
class StrongWorkerRef;

namespace notification {
enum class PermissionCheckPurpose : uint8_t;
}

/*
 * A Notification gets a corresponding IPC actor after successful construction.
 * The notification object and the actor do not own each other and their
 * lifetimes are controlled semi-independently.
 *
 * The Notification object can be cycle collected when either:
 * - no one is listening for the events, or
 * - the backend notification is closed.
 *
 * The actor goes away when either:
 * - the backend notification is closed, or
 * - the tab is closed or bfcached.
 *
 * (It cannot just go away on cycle collection because nsIAlertsService wants to
 * know whether the triggered page is still open to decide whether to open a new
 * tab or focus on the existing tab.)
 */
class Notification : public DOMEventTargetHelper, public SupportsWeakPtr {
  friend class CloseNotificationRunnable;
  friend class NotificationTask;
  friend class NotificationPermissionRequest;
  friend class MainThreadNotificationObserver;
  friend class NotificationStorageCallback;
  friend class ServiceWorkerNotificationObserver;
  friend class WorkerGetRunnable;
  friend class WorkerNotificationObserver;

 public:
  IMPL_EVENT_HANDLER(click)
  IMPL_EVENT_HANDLER(show)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(close)

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(Notification,
                                                         DOMEventTargetHelper)

  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  static already_AddRefed<Notification> Constructor(
      const GlobalObject& aGlobal, const nsAString& aTitle,
      const NotificationOptions& aOption, ErrorResult& aRv);

  /**
   * Used when dispatching the ServiceWorkerEvent.
   *
   * Does not initialize the Notification's behavior.
   * This is because:
   * 1) The Notification is not shown to the user and so the behavior
   *    parameters don't matter.
   * 2) The default binding requires main thread for parsing the JSON from the
   *    string behavior.
   */
  static Result<already_AddRefed<Notification>, QMResult> ConstructFromFields(
      nsIGlobalObject* aGlobal, const nsAString& aID, const nsAString& aTitle,
      const nsAString& aDir, const nsAString& aLang, const nsAString& aBody,
      const nsAString& aTag, const nsAString& aIcon, const nsAString& aData,
      const nsAString& aServiceWorkerRegistrationScope);

  void GetID(nsAString& aRetval) { aRetval = mID; }

  void GetTitle(nsAString& aRetval) { aRetval = mTitle; }

  NotificationDirection Dir() { return mDir; }

  void GetLang(nsAString& aRetval) { aRetval = mLang; }

  void GetBody(nsAString& aRetval) { aRetval = mBody; }

  void GetTag(nsAString& aRetval) { aRetval = mTag; }

  void GetIcon(nsAString& aRetval) { aRetval = mIconUrl; }

  void MaybeNotifyClose();

  static bool RequestPermissionEnabledForScope(JSContext* aCx,
                                               JSObject* /* unused */);

  static already_AddRefed<Promise> RequestPermission(
      const GlobalObject& aGlobal,
      const Optional<OwningNonNull<NotificationPermissionCallback> >& aCallback,
      ErrorResult& aRv);

  static NotificationPermission GetPermission(const GlobalObject& aGlobal,
                                              ErrorResult& aRv);

  static already_AddRefed<Promise> Get(nsPIDOMWindowInner* aWindow,
                                       const GetNotificationOptions& aFilter,
                                       const nsAString& aScope,
                                       ErrorResult& aRv);

  static already_AddRefed<Promise> WorkerGet(
      WorkerPrivate* aWorkerPrivate, const GetNotificationOptions& aFilter,
      const nsAString& aScope, ErrorResult& aRv);

  // Notification implementation of
  // ServiceWorkerRegistration.showNotification.
  //
  //
  // Note that aCx may not be in the compartment of aGlobal, but aOptions will
  // have its JS things in the compartment of aCx.
  static already_AddRefed<Promise> ShowPersistentNotification(
      JSContext* aCx, nsIGlobalObject* aGlobal, const nsAString& aScope,
      const nsAString& aTitle, const NotificationOptions& aOptions,
      const ServiceWorkerRegistrationDescriptor& aDescriptor, ErrorResult& aRv);

  void Close();

  nsIGlobalObject* GetParentObject() const { return GetOwnerGlobal(); }

  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  bool RequireInteraction() const;

  bool Silent() const;

  void GetVibrate(nsTArray<uint32_t>& aRetval) const;

  void GetData(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval);

  void InitFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aData,
                     ErrorResult& aRv);

  Result<Ok, QMResult> InitFromBase64(const nsAString& aData);

  static NotificationPermission GetPermission(
      nsIGlobalObject* aGlobal, notification::PermissionCheckPurpose aPurpose,
      ErrorResult& aRv);

  bool DispatchClickEvent();

  nsresult DispatchToMainThread(already_AddRefed<nsIRunnable>&& aRunnable);

 protected:
  Notification(nsIGlobalObject* aGlobal, const nsAString& aID,
               const nsAString& aTitle, const nsAString& aBody,
               NotificationDirection aDir, const nsAString& aLang,
               const nsAString& aTag, const nsAString& aIconUrl,
               bool aRequireInteraction, bool aSilent,
               nsTArray<uint32_t>&& aVibrate,
               const NotificationBehavior& aBehavior);

  static already_AddRefed<Notification> CreateInternal(
      nsIGlobalObject* aGlobal, const nsAString& aID, const nsAString& aTitle,
      const NotificationOptions& aOptions, ErrorResult& aRv);

  void Deactivate();

  static NotificationPermission GetPermissionInternal(
      nsPIDOMWindowInner* aWindow,
      notification::PermissionCheckPurpose aPurpose, ErrorResult& rv);

  void SetScope(const nsAString& aScope) {
    MOZ_ASSERT(mScope.IsEmpty());
    mScope = aScope;
  }

  WeakPtr<notification::NotificationChild> mActor;

  const nsString mID;
  const nsString mTitle;
  const nsString mBody;
  const NotificationDirection mDir;
  const nsString mLang;
  const nsString mTag;
  const nsString mIconUrl;
  const bool mRequireInteraction;
  const bool mSilent;
  nsTArray<uint32_t> mVibrate;
  nsString mDataAsBase64;
  const NotificationBehavior mBehavior;

  // It's null until GetData is first called
  JS::Heap<JS::Value> mData;

  nsString mScope;

  bool mIsClosed = false;

 private:
  virtual ~Notification();

  // Creates a Notification and shows it. Returns a reference to the
  // Notification if result is NS_OK. The lifetime of this Notification is tied
  // to an underlying NotificationRef. Do not hold a non-stack raw pointer to
  // it. Be careful about thread safety if acquiring a strong reference.
  //
  // Note that aCx may not be in the compartment of aGlobal, but aOptions will
  // have its JS things in the compartment of aCx.
  static already_AddRefed<Notification> Create(
      JSContext* aCx, nsIGlobalObject* aGlobal, const nsAString& aTitle,
      const NotificationOptions& aOptions, const nsAString& aScope,
      ErrorResult& aRv);

  bool CreateActor();
  bool SendShow(Promise* aPromise);

  static nsresult ResolveIconAndSoundURL(nsIGlobalObject* aGlobal,
                                         nsString& aIconURL,
                                         nsString& aSoundURL);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_notification_h__
