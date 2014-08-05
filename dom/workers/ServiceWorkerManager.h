/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkermanager_h
#define mozilla_dom_workers_serviceworkermanager_h

#include "nsIServiceWorkerManager.h"
#include "nsCOMPtr.h"

#include "mozilla/Attributes.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Preferences.h"
#include "mozilla/TypedEnum.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Promise.h"
#include "nsRefPtrHashtable.h"
#include "nsTArrayForwardDeclare.h"
#include "nsTObserverArray.h"

class nsIScriptError;

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorker;
class ServiceWorkerContainer;
class ServiceWorkerUpdateInstance;

/**
 * UpdatePromise is a utility class that sort of imitates Promise, but not
 * completely. Using DOM Promise from C++ is a pain when we know the precise types
 * we're dealing with since it involves dealing with JSAPI. In this case we
 * also don't (yet) need the 'thenables added after resolution should trigger
 * immediately' support and other things like that. All we want is something
 * that works reasonably Promise like and can resolve real DOM Promises added
 * pre-emptively.
 */
class UpdatePromise MOZ_FINAL
{
public:
  UpdatePromise();
  ~UpdatePromise();

  void AddPromise(Promise* aPromise);
  void ResolveAllPromises(const nsACString& aScriptSpec, const nsACString& aScope);
  void RejectAllPromises(nsresult aRv);
  void RejectAllPromises(const ErrorEventInit& aErrorDesc);

  bool
  IsRejected() const
  {
    return mState == Rejected;
  }

private:
  enum {
    Pending,
    Resolved,
    Rejected
  } mState;

  // XXXnsm: Right now we don't need to support AddPromise() after
  // already being resolved (i.e. true Promise-like behaviour).
  nsTArray<WeakPtr<Promise>> mPromises;
};

/*
 * Wherever the spec treats a worker instance and a description of said worker
 * as the same thing; i.e. "Resolve foo with
 * _GetNewestWorker(serviceWorkerRegistration)", we represent the description
 * by this class and spawn a ServiceWorker in the right global when required.
 */
class ServiceWorkerInfo MOZ_FINAL
{
  nsCString mScriptSpec;

  ~ServiceWorkerInfo()
  { }

public:
  NS_INLINE_DECL_REFCOUNTING(ServiceWorkerInfo)

  const nsCString&
  GetScriptSpec() const
  {
    return mScriptSpec;
  }

  explicit ServiceWorkerInfo(const nsACString& aScriptSpec)
    : mScriptSpec(aScriptSpec)
  { }
};

// Use multiples of 2 since they can be bitwise ORed when calling
// InvalidateServiceWorkerContainerWorker.
MOZ_BEGIN_ENUM_CLASS(WhichServiceWorker)
  INSTALLING_WORKER = 1,
  WAITING_WORKER    = 2,
  ACTIVE_WORKER     = 4,
MOZ_END_ENUM_CLASS(WhichServiceWorker)
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(WhichServiceWorker)

// Needs to inherit from nsISupports because NS_ProxyRelease() does not support
// non-ISupports classes.
class ServiceWorkerRegistration MOZ_FINAL : public nsISupports
{
  uint32_t mControlledDocumentsCounter;

  virtual ~ServiceWorkerRegistration();

public:
  NS_DECL_ISUPPORTS

  nsCString mScope;
  // The scriptURL for the registration. This may be completely different from
  // the URLs of the following three workers.
  nsCString mScriptSpec;

  nsRefPtr<ServiceWorkerInfo> mCurrentWorker;
  nsRefPtr<ServiceWorkerInfo> mWaitingWorker;
  nsRefPtr<ServiceWorkerInfo> mInstallingWorker;

  nsAutoPtr<UpdatePromise> mUpdatePromise;
  nsRefPtr<ServiceWorkerUpdateInstance> mUpdateInstance;

  void
  AddUpdatePromiseObserver(Promise* aPromise)
  {
    MOZ_ASSERT(HasUpdatePromise());
    mUpdatePromise->AddPromise(aPromise);
  }

  bool
  HasUpdatePromise()
  {
    return mUpdatePromise;
  }

  // When unregister() is called on a registration, it is not immediately
  // removed since documents may be controlled. It is marked as
  // pendingUninstall and when all controlling documents go away, removed.
  bool mPendingUninstall;

  explicit ServiceWorkerRegistration(const nsACString& aScope);

  already_AddRefed<ServiceWorkerInfo>
  Newest()
  {
    nsRefPtr<ServiceWorkerInfo> newest;
    if (mInstallingWorker) {
      newest = mInstallingWorker;
    } else if (mWaitingWorker) {
      newest = mWaitingWorker;
    } else {
      newest = mCurrentWorker;
    }

    return newest.forget();
  }

  void
  StartControllingADocument()
  {
    ++mControlledDocumentsCounter;
  }

  void
  StopControllingADocument()
  {
    --mControlledDocumentsCounter;
  }

  bool
  IsControllingDocuments() const
  {
    return mControlledDocumentsCounter > 0;
  }
};

#define NS_SERVICEWORKERMANAGER_IMPL_IID                 \
{ /* f4f8755a-69ca-46e8-a65d-775745535990 */             \
  0xf4f8755a,                                            \
  0x69ca,                                                \
  0x46e8,                                                \
  { 0xa6, 0x5d, 0x77, 0x57, 0x45, 0x53, 0x59, 0x90 }     \
}

/*
 * The ServiceWorkerManager is a per-process global that deals with the
 * installation, querying and event dispatch of ServiceWorkers for all the
 * origins in the process.
 */
class ServiceWorkerManager MOZ_FINAL : public nsIServiceWorkerManager
{
  friend class ActivationRunnable;
  friend class RegisterRunnable;
  friend class CallInstallRunnable;
  friend class CancelServiceWorkerInstallationRunnable;
  friend class ServiceWorkerUpdateInstance;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERMANAGER
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_SERVICEWORKERMANAGER_IMPL_IID)

  static ServiceWorkerManager* FactoryCreate()
  {
    AssertIsOnMainThread();
    if (!Preferences::GetBool("dom.serviceWorkers.enabled")) {
      return nullptr;
    }

    ServiceWorkerManager* res = new ServiceWorkerManager;
    NS_ADDREF(res);
    return res;
  }

  /*
   * This struct is used for passive ServiceWorker management.
   * Actively running ServiceWorkers use the SharedWorker infrastructure in
   * RuntimeService for execution and lifetime management.
   */
  struct ServiceWorkerDomainInfo
  {
    // Ordered list of scopes for glob matching.
    // Each entry is an absolute URL representing the scope.
    //
    // An array is used for now since the number of controlled scopes per
    // domain is expected to be relatively low. If that assumption was proved
    // wrong this should be replaced with a better structure to avoid the
    // memmoves associated with inserting stuff in the middle of the array.
    nsTArray<nsCString> mOrderedScopes;

    // Scope to registration.
    nsRefPtrHashtable<nsCStringHashKey, ServiceWorkerRegistration> mServiceWorkerRegistrations;

    // This array can't be stored in ServiceWorkerRegistration because one may
    // not exist when a certain window is opened, but we still want that
    // window's container to be notified if it's in scope.
    // The containers inform the SWM on creation and destruction.
    nsTObserverArray<ServiceWorkerContainer*> mServiceWorkerContainers;

    nsRefPtrHashtable<nsISupportsHashKey, ServiceWorkerRegistration> mControlledDocuments;

    ServiceWorkerDomainInfo()
    { }

    already_AddRefed<ServiceWorkerRegistration>
    GetRegistration(const nsCString& aScope) const
    {
      nsRefPtr<ServiceWorkerRegistration> reg;
      mServiceWorkerRegistrations.Get(aScope, getter_AddRefs(reg));
      return reg.forget();
    }

    ServiceWorkerRegistration*
    CreateNewRegistration(const nsCString& aScope)
    {
      ServiceWorkerRegistration* registration =
        new ServiceWorkerRegistration(aScope);
      // From now on ownership of registration is with
      // mServiceWorkerRegistrations.
      mServiceWorkerRegistrations.Put(aScope, registration);
      ServiceWorkerManager::AddScope(mOrderedScopes, aScope);
      return registration;
    }

    NS_INLINE_DECL_REFCOUNTING(ServiceWorkerDomainInfo)

  private:
    ~ServiceWorkerDomainInfo()
    { }
  };

  nsRefPtrHashtable<nsCStringHashKey, ServiceWorkerDomainInfo> mDomainMap;

  void
  ResolveRegisterPromises(ServiceWorkerRegistration* aRegistration,
                          const nsACString& aWorkerScriptSpec);

  void
  RejectUpdatePromiseObservers(ServiceWorkerRegistration* aRegistration,
                               nsresult aResult);

  void
  RejectUpdatePromiseObservers(ServiceWorkerRegistration* aRegistration,
                               const ErrorEventInit& aErrorDesc);

  void
  FinishFetch(ServiceWorkerRegistration* aRegistration,
              nsPIDOMWindow* aWindow);

  void
  FinishInstall(ServiceWorkerRegistration* aRegistration);

  void
  FinishActivate(ServiceWorkerRegistration* aRegistration);

  void
  HandleError(JSContext* aCx,
              const nsACString& aScope,
              const nsAString& aWorkerURL,
              nsString aMessage,
              nsString aFilename,
              nsString aLine,
              uint32_t aLineNumber,
              uint32_t aColumnNumber,
              uint32_t aFlags);

  static already_AddRefed<ServiceWorkerManager>
  GetInstance();

private:
  ServiceWorkerManager();
  ~ServiceWorkerManager();

  NS_IMETHOD
  Update(ServiceWorkerRegistration* aRegistration, nsPIDOMWindow* aWindow);

  void
  Install(ServiceWorkerRegistration* aRegistration,
          ServiceWorkerInfo* aServiceWorkerInfo);

  NS_IMETHOD
  CreateServiceWorkerForWindow(nsPIDOMWindow* aWindow,
                               const nsACString& aScriptSpec,
                               const nsACString& aScope,
                               ServiceWorker** aServiceWorker);

  NS_IMETHOD
  CreateServiceWorker(const nsACString& aScriptSpec,
                      const nsACString& aScope,
                      ServiceWorker** aServiceWorker);

  static PLDHashOperator
  CleanupServiceWorkerInformation(const nsACString& aDomain,
                                  ServiceWorkerDomainInfo* aDomainInfo,
                                  void *aUnused);

  already_AddRefed<ServiceWorkerDomainInfo>
  GetDomainInfo(nsIDocument* aDoc);

  already_AddRefed<ServiceWorkerDomainInfo>
  GetDomainInfo(nsIURI* aURI);

  already_AddRefed<ServiceWorkerDomainInfo>
  GetDomainInfo(const nsCString& aURL);

  NS_IMETHODIMP
  GetServiceWorkerForWindow(nsIDOMWindow* aWindow,
                            WhichServiceWorker aWhichWorker,
                            nsISupports** aServiceWorker);

  void
  InvalidateServiceWorkerContainerWorker(ServiceWorkerRegistration* aRegistration,
                                         WhichServiceWorker aWhichOnes);

  already_AddRefed<ServiceWorkerRegistration>
  GetServiceWorkerRegistration(nsPIDOMWindow* aWindow);

  already_AddRefed<ServiceWorkerRegistration>
  GetServiceWorkerRegistration(nsIDocument* aDoc);

  already_AddRefed<ServiceWorkerRegistration>
  GetServiceWorkerRegistration(nsIURI* aURI);

  static void
  AddScope(nsTArray<nsCString>& aList, const nsACString& aScope);

  static nsCString
  FindScopeForPath(nsTArray<nsCString>& aList, const nsACString& aPath);

  static void
  RemoveScope(nsTArray<nsCString>& aList, const nsACString& aScope);

  void
  FireEventOnServiceWorkerContainers(ServiceWorkerRegistration* aRegistration,
                                     const nsAString& aName);

};

NS_DEFINE_STATIC_IID_ACCESSOR(ServiceWorkerManager,
                              NS_SERVICEWORKERMANAGER_IMPL_IID);

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkermanager_h
