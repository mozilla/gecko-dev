/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUtils.h"

#include "nsContentPolicyUtils.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "nsCOMPtr.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"
#include "nsIURL.h"
#include "nsPrintfCString.h"

namespace mozilla::dom {

static bool IsServiceWorkersTestingEnabledInGlobal(JSObject* const aGlobal) {
  if (const nsCOMPtr<nsPIDOMWindowInner> innerWindow =
          Navigator::GetWindowFromGlobal(aGlobal)) {
    if (auto* bc = innerWindow->GetBrowsingContext()) {
      return bc->Top()->ServiceWorkersTestingEnabled();
    }
    return false;
  }
  if (WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate()) {
    return workerPrivate->ServiceWorkersTestingInWindow();
  }
  return false;
}

bool ServiceWorkersEnabled(JSContext* aCx, JSObject* aGlobal) {
  if (!StaticPrefs::dom_serviceWorkers_enabled()) {
    return false;
  }

  // xpc::CurrentNativeGlobal below requires rooting
  JS::Rooted<JSObject*> jsGlobal(aCx, aGlobal);
  nsIGlobalObject* global = xpc::CurrentNativeGlobal(aCx);

  if (const nsCOMPtr<nsIPrincipal> principal = global->PrincipalOrNull()) {
    // ServiceWorkers are currently not available in PrivateBrowsing.
    // Bug 1320796 will change this.
    if (principal->GetIsInPrivateBrowsing()) {
      return false;
    }

    // Allow a webextension principal to register a service worker script with
    // a moz-extension url only if 'extensions.service_worker_register.allowed'
    // is true.
    if (!StaticPrefs::extensions_serviceWorkerRegister_allowed()) {
      if (principal->GetIsAddonOrExpandedAddonPrincipal()) {
        return false;
      }
    }
  }

  if (IsSecureContextOrObjectIsFromSecureContext(aCx, jsGlobal)) {
    return true;
  }

  return StaticPrefs::dom_serviceWorkers_testing_enabled() ||
         IsServiceWorkersTestingEnabledInGlobal(jsGlobal);
}

bool ServiceWorkerRegistrationDataIsValid(
    const ServiceWorkerRegistrationData& aData) {
  return !aData.scope().IsEmpty() && !aData.currentWorkerURL().IsEmpty() &&
         !aData.cacheName().IsEmpty();
}

class WorkerCheckMayLoadSyncRunnable final : public WorkerMainThreadRunnable {
 public:
  WorkerCheckMayLoadSyncRunnable(std::function<void(ErrorResult&)>&& aCheckFunc,
                                 ErrorResult& aRv)
      : WorkerMainThreadRunnable(GetCurrentThreadWorkerPrivate(),
                                 "WorkerCheckMayLoadSyncRunnable"_ns),
        mCheckFunc(aCheckFunc),
        mRv(aRv) {}

  bool MainThreadRun() override {
    mCheckFunc(mRv);
    return true;
  }

 private:
  std::function<void(ErrorResult&)> mCheckFunc;
  // This reference is safe because we are a synchronously dispatched runnable
  // and while we expect the ErrorResult to be stack-allocated, our runnable
  // holds that stack alive during the sync dispatch.
  ErrorResult& mRv;
};

namespace {

void CheckForSlashEscapedCharsInPath(nsIURI* aURI, const char* aURLDescription,
                                     ErrorResult& aRv) {
  MOZ_ASSERT(aURI);

  // A URL that can't be downcast to a standard URL is an invalid URL and should
  // be treated as such and fail with SecurityError.
  nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
  if (NS_WARN_IF(!url)) {
    // This really should not happen, since the caller checks that we
    // have an http: or https: URL!
    aRv.ThrowInvalidStateError("http: or https: URL without a concept of path");
    return;
  }

  nsAutoCString path;
  nsresult rv = url->GetFilePath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Again, should not happen.
    aRv.ThrowInvalidStateError("http: or https: URL without a concept of path");
    return;
  }

  ToLowerCase(path);
  if (path.Find("%2f") != kNotFound || path.Find("%5c") != kNotFound) {
    nsPrintfCString err("%s contains %%2f or %%5c", aURLDescription);
    aRv.ThrowTypeError(err);
  }
}

// Helper to take a lambda and, if we are already on the main thread, run it
// right now on the main thread, otherwise we use the
// WorkerCheckMayLoadSyncRunnable which spins a sync loop and run that on the
// main thread.  When Bug 1901387 makes it possible to run CheckMayLoad logic
// on worker threads, this helper can be removed and the lambda flattened.
//
// This method takes an ErrorResult to pass as an argument to the lambda because
// the ErrorResult will also be used to capture dispatch failures.
void CheckMayLoadOnMainThread(ErrorResult& aRv,
                              std::function<void(ErrorResult&)>&& aCheckFunc) {
  if (NS_IsMainThread()) {
    aCheckFunc(aRv);
    return;
  }

  RefPtr<WorkerCheckMayLoadSyncRunnable> runnable =
      new WorkerCheckMayLoadSyncRunnable(std::move(aCheckFunc), aRv);
  runnable->Dispatch(GetCurrentThreadWorkerPrivate(), Canceling, aRv);
}

}  // anonymous namespace

void ServiceWorkerScopeAndScriptAreValid(const ClientInfo& aClientInfo,
                                         nsIURI* aScopeURI, nsIURI* aScriptURI,
                                         ErrorResult& aRv,
                                         nsIGlobalObject* aGlobalForReporting) {
  MOZ_DIAGNOSTIC_ASSERT(aScopeURI);
  MOZ_DIAGNOSTIC_ASSERT(aScriptURI);

  auto principalOrErr = aClientInfo.GetPrincipal();
  if (NS_WARN_IF(principalOrErr.isErr())) {
    aRv.ThrowInvalidStateError("Can't make security decisions about Client");
    return;
  }

  auto hasHTTPScheme = [](nsIURI* aURI) -> bool {
    return aURI->SchemeIs("http") || aURI->SchemeIs("https");
  };
  auto hasMozExtScheme = [](nsIURI* aURI) -> bool {
    return aURI->SchemeIs("moz-extension");
  };

  nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();

  auto isExtension = principal->GetIsAddonOrExpandedAddonPrincipal();
  auto hasValidURISchemes = !isExtension ? hasHTTPScheme : hasMozExtScheme;

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm step 3.
  if (!hasValidURISchemes(aScriptURI)) {
    auto message = !isExtension
                       ? "Script URL's scheme is not 'http' or 'https'"_ns
                       : "Script URL's scheme is not 'moz-extension'"_ns;
    aRv.ThrowTypeError(message);
    return;
  }

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm step 4.
  CheckForSlashEscapedCharsInPath(aScriptURI, "script URL", aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm step 8.
  if (!hasValidURISchemes(aScopeURI)) {
    auto message = !isExtension
                       ? "Scope URL's scheme is not 'http' or 'https'"_ns
                       : "Scope URL's scheme is not 'moz-extension'"_ns;
    aRv.ThrowTypeError(message);
    return;
  }

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm step 9.
  CheckForSlashEscapedCharsInPath(aScopeURI, "scope URL", aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  // The refs should really be empty coming in here, but if someone
  // injects bad data into IPC, who knows.  So let's revalidate that.
  nsAutoCString ref;
  Unused << aScopeURI->GetRef(ref);
  if (NS_WARN_IF(!ref.IsEmpty())) {
    aRv.ThrowSecurityError("Non-empty fragment on scope URL");
    return;
  }

  Unused << aScriptURI->GetRef(ref);
  if (NS_WARN_IF(!ref.IsEmpty())) {
    aRv.ThrowSecurityError("Non-empty fragment on script URL");
    return;
  }

  // CSP reporting on the main thread relies on the document node.
  Document* maybeDoc = nullptr;
  // CSP reporting for the worker relies on a helper listener.
  nsCOMPtr<nsICSPEventListener> cspListener;
  if (aGlobalForReporting) {
    if (auto* win = aGlobalForReporting->GetAsInnerWindow()) {
      maybeDoc = win->GetExtantDoc();
      if (!maybeDoc) {
        aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return;
      }
      // LoadInfo has assertions about the Principal passed to it being the
      // same object as the doc NodePrincipal(), so clobber principal to be
      // that rather than the Principal we pulled out of the ClientInfo.
      principal = maybeDoc->NodePrincipal();
    } else if (auto* wp = GetCurrentThreadWorkerPrivate()) {
      cspListener = wp->CSPEventListener();
    }
  }

  // If this runs on the main thread, it is done synchronously.  On workers all
  // the references are safe due to the use of a sync runnable that blocks
  // execution of the worker.  The caveat is that control runnables can run
  // while the syncloop spins and these can cause a worker global to start dying
  // and WorkerRefs to be notified.  However, GlobalTeardownObservers will only
  // be torn down when the stack completely unwinds and no syncloops are on the
  // stack.
  CheckMayLoadOnMainThread(aRv, [&](ErrorResult& aResult) {
    nsresult rv = principal->CheckMayLoadWithReporting(
        aScopeURI, false /* allowIfInheritsPrincipal */, 0 /* innerWindowID */);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResult.ThrowSecurityError("Scope URL is not same-origin with Client");
      return;
    }

    rv = principal->CheckMayLoadWithReporting(
        aScriptURI, false /* allowIfInheritsPrincipal */,
        0 /* innerWindowID */);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aResult.ThrowSecurityError("Script URL is not same-origin with Client");
      return;
    }

    // We perform a CSP check where the check will retrieve the CSP from the
    // ClientInfo and validate worker-src directives or its fallbacks
    // (https://w3c.github.io/webappsec-csp/#directive-worker-src).
    //
    // https://w3c.github.io/webappsec-csp/#fetch-integration explains how CSP
    // integrates with fetch (although exact step numbers are currently out of
    // sync).  Specifically main fetch
    // (https://fetch.spec.whatwg.org/#concept-main-fetch) does report-only
    // checks in step 4, checks for request blocks in step 7, and response
    // blocks in step 19.
    //
    // We are performing this check prior to our use of fetch due to asymmetries
    // about application of CSP raised in Bug 1455077 and in more detail in the
    // still-open https://github.com/w3c/ServiceWorker/issues/755.
    //
    // Also note that while fetch explicitly returns network errors for CSP, our
    // logic here (and the CheckMayLoad calls above) corresponds to the steps of
    // the register (https://w3c.github.io/ServiceWorker/#register-algorithm)
    // which explicitly throws a SecurityError.
    nsCOMPtr<nsILoadInfo> secCheckLoadInfo = new mozilla::net::LoadInfo(
        principal,  // loading principal
        principal,  // triggering principal
        maybeDoc,   // loading node
        nsILoadInfo::SEC_ONLY_FOR_EXPLICIT_CONTENTSEC_CHECK,
        nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER, Some(aClientInfo));

    if (cspListener) {
      rv = secCheckLoadInfo->SetCspEventListener(cspListener);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return;
      }
    }

    // Check content policy.
    int16_t decision = nsIContentPolicy::ACCEPT;
    rv = NS_CheckContentLoadPolicy(aScriptURI, secCheckLoadInfo, &decision);
    if (NS_FAILED(rv) || NS_WARN_IF(decision != nsIContentPolicy::ACCEPT)) {
      aResult.ThrowSecurityError("Script URL is not allowed by policy.");
      return;
    }
  });
}

}  // namespace mozilla::dom
