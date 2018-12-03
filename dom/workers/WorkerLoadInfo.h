/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_WorkerLoadInfo_h
#define mozilla_dom_workers_WorkerLoadInfo_h

#include "mozilla/dom/ChannelInfo.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsIRequest.h"
#include "nsISupportsImpl.h"
#include "nsIWeakReferenceUtils.h"

class nsIChannel;
class nsIContentSecurityPolicy;
class nsILoadGroup;
class nsIPrincipal;
class nsIRunnable;
class nsIScriptContext;
class nsITabChild;
class nsIURI;
class nsPIDOMWindowInner;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}  // namespace ipc

namespace dom {

class WorkerPrivate;

struct WorkerLoadInfoData {
  // All of these should be released in
  // WorkerPrivateParent::ForgetMainThreadObjects.
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIURI> mResolvedScriptURI;

  // This is the principal of the global (parent worker or a window) loading
  // the worker. It can be null if we are executing a ServiceWorker, otherwise,
  // except for data: URL, it must subsumes the worker principal.
  // If we load a data: URL, mPrincipal will be a null principal.
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  nsCOMPtr<nsIPrincipal> mPrincipal;

  nsCOMPtr<nsIScriptContext> mScriptContext;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsIContentSecurityPolicy> mCSP;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsILoadGroup> mLoadGroup;

  class InterfaceRequestor final : public nsIInterfaceRequestor {
    NS_DECL_ISUPPORTS

   public:
    InterfaceRequestor(nsIPrincipal* aPrincipal, nsILoadGroup* aLoadGroup);
    void MaybeAddTabChild(nsILoadGroup* aLoadGroup);
    NS_IMETHOD GetInterface(const nsIID& aIID, void** aSink) override;

    void SetOuterRequestor(nsIInterfaceRequestor* aOuterRequestor) {
      MOZ_ASSERT(!mOuterRequestor);
      MOZ_ASSERT(aOuterRequestor);
      mOuterRequestor = aOuterRequestor;
    }

   private:
    ~InterfaceRequestor() {}

    already_AddRefed<nsITabChild> GetAnyLiveTabChild();

    nsCOMPtr<nsILoadContext> mLoadContext;
    nsCOMPtr<nsIInterfaceRequestor> mOuterRequestor;

    // Array of weak references to nsITabChild.  We do not want to keep TabChild
    // actors alive for long after their ActorDestroy() methods are called.
    nsTArray<nsWeakPtr> mTabChildList;
  };

  // Only set if we have a custom overriden load group
  RefPtr<InterfaceRequestor> mInterfaceRequestor;

  nsAutoPtr<mozilla::ipc::PrincipalInfo> mPrincipalInfo;
  nsCString mDomain;
  nsString mOrigin;  // Derived from mPrincipal; can be used on worker thread.

  nsString mServiceWorkerCacheName;
  Maybe<ServiceWorkerDescriptor> mServiceWorkerDescriptor;
  Maybe<ServiceWorkerRegistrationDescriptor>
      mServiceWorkerRegistrationDescriptor;

  Maybe<ServiceWorkerDescriptor> mParentController;

  ChannelInfo mChannelInfo;
  nsLoadFlags mLoadFlags;

  uint64_t mWindowID;

  net::ReferrerPolicy mReferrerPolicy;
  bool mFromWindow;
  bool mEvalAllowed;
  bool mReportCSPViolations;
  bool mXHRParamsAllowed;
  bool mPrincipalIsSystem;
  bool mStorageAllowed;
  bool mFirstPartyStorageAccessGranted;
  bool mServiceWorkersTestingInWindow;
  OriginAttributes mOriginAttributes;

  enum {
    eNotSet,
    eInsecureContext,
    eSecureContext,
  } mSecureContext;

  WorkerLoadInfoData();
  WorkerLoadInfoData(WorkerLoadInfoData&& aOther) = default;

  WorkerLoadInfoData& operator=(WorkerLoadInfoData&& aOther) = default;
};

struct WorkerLoadInfo : WorkerLoadInfoData {
  WorkerLoadInfo();
  WorkerLoadInfo(WorkerLoadInfo&& aOther) noexcept;
  ~WorkerLoadInfo();

  WorkerLoadInfo& operator=(WorkerLoadInfo&& aOther) = default;

  nsresult SetPrincipalOnMainThread(nsIPrincipal* aPrincipal,
                                    nsILoadGroup* aLoadGroup);

  nsresult GetPrincipalAndLoadGroupFromChannel(nsIChannel* aChannel,
                                               nsIPrincipal** aPrincipalOut,
                                               nsILoadGroup** aLoadGroupOut);

  nsresult SetPrincipalFromChannel(nsIChannel* aChannel);

  bool FinalChannelPrincipalIsValid(nsIChannel* aChannel);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  bool PrincipalIsValid() const;

  bool PrincipalURIMatchesScriptURL();
#endif

  bool ProxyReleaseMainThreadObjects(WorkerPrivate* aWorkerPrivate);

  bool ProxyReleaseMainThreadObjects(
      WorkerPrivate* aWorkerPrivate,
      nsCOMPtr<nsILoadGroup>& aLoadGroupToCancel);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workers_WorkerLoadInfo_h
