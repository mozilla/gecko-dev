/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ServiceWorkerRegistrarTypes.h"
#include "nsIURL.h"

namespace mozilla {
namespace dom {

bool ServiceWorkerParentInterceptEnabled() {
  static Atomic<bool> sEnabled;
  static Atomic<bool> sInitialized;
  if (!sInitialized) {
    AssertIsOnMainThread();
    sInitialized = true;
    sEnabled =
        Preferences::GetBool("dom.serviceWorkers.parent_intercept", false);
  }
  return sEnabled;
}

bool ServiceWorkerRegistrationDataIsValid(
    const ServiceWorkerRegistrationData& aData) {
  return !aData.scope().IsEmpty() && !aData.currentWorkerURL().IsEmpty() &&
         !aData.cacheName().IsEmpty();
}

namespace {

nsresult CheckForSlashEscapedCharsInPath(nsIURI* aURI) {
  MOZ_ASSERT(aURI);

  // A URL that can't be downcast to a standard URL is an invalid URL and should
  // be treated as such and fail with SecurityError.
  nsCOMPtr<nsIURL> url(do_QueryInterface(aURI));
  if (NS_WARN_IF(!url)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsAutoCString path;
  nsresult rv = url->GetFilePath(path);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ToLowerCase(path);
  if (path.Find("%2f") != kNotFound || path.Find("%5c") != kNotFound) {
    return NS_ERROR_DOM_TYPE_ERR;
  }

  return NS_OK;
}

}  // anonymous namespace

nsresult ServiceWorkerScopeAndScriptAreValid(const ClientInfo& aClientInfo,
                                             nsIURI* aScopeURI,
                                             nsIURI* aScriptURI) {
  MOZ_DIAGNOSTIC_ASSERT(aScopeURI);
  MOZ_DIAGNOSTIC_ASSERT(aScriptURI);

  nsCOMPtr<nsIPrincipal> principal = aClientInfo.GetPrincipal();
  NS_ENSURE_TRUE(principal, NS_ERROR_DOM_INVALID_STATE_ERR);

  bool isHttp = false;
  bool isHttps = false;
  Unused << aScriptURI->SchemeIs("http", &isHttp);
  Unused << aScriptURI->SchemeIs("https", &isHttps);
  NS_ENSURE_TRUE(isHttp || isHttps, NS_ERROR_DOM_SECURITY_ERR);

  nsresult rv = CheckForSlashEscapedCharsInPath(aScopeURI);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CheckForSlashEscapedCharsInPath(aScriptURI);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString ref;
  Unused << aScopeURI->GetRef(ref);
  NS_ENSURE_TRUE(ref.IsEmpty(), NS_ERROR_DOM_SECURITY_ERR);

  Unused << aScriptURI->GetRef(ref);
  NS_ENSURE_TRUE(ref.IsEmpty(), NS_ERROR_DOM_SECURITY_ERR);

  rv = principal->CheckMayLoad(aScopeURI, true /* report */,
                               false /* allowIfInheritsPrincipal */);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SECURITY_ERR);

  rv = principal->CheckMayLoad(aScriptURI, true /* report */,
                               false /* allowIfInheritsPrincipal */);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SECURITY_ERR);

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
