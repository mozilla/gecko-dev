/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ServiceWorkerUtils_h
#define _mozilla_dom_ServiceWorkerUtils_h

#include "mozilla/MozPromise.h"
#include "mozilla/dom/IPCNavigationPreloadState.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "nsTArray.h"

class nsIGlobalObject;
class nsIURI;

namespace mozilla {

class CopyableErrorResult;
class ErrorResult;

namespace dom {

class ClientInfo;
class ServiceWorkerRegistrationData;
class ServiceWorkerRegistrationDescriptor;
struct NavigationPreloadState;

using ServiceWorkerRegistrationPromise =
    MozPromise<ServiceWorkerRegistrationDescriptor, CopyableErrorResult, false>;

using ServiceWorkerRegistrationListPromise =
    MozPromise<CopyableTArray<ServiceWorkerRegistrationDescriptor>,
               CopyableErrorResult, false>;

using NavigationPreloadStatePromise =
    MozPromise<IPCNavigationPreloadState, CopyableErrorResult, false>;

using ServiceWorkerRegistrationCallback =
    std::function<void(const ServiceWorkerRegistrationDescriptor&)>;

using ServiceWorkerRegistrationListCallback =
    std::function<void(const nsTArray<ServiceWorkerRegistrationDescriptor>&)>;

using ServiceWorkerBoolCallback = std::function<void(bool)>;

using ServiceWorkerFailureCallback = std::function<void(ErrorResult&&)>;

using NavigationPreloadGetStateCallback =
    std::function<void(NavigationPreloadState&&)>;

bool ServiceWorkerRegistrationDataIsValid(
    const ServiceWorkerRegistrationData& aData);

// Performs key spec validation steps of
// https://w3c.github.io/ServiceWorker/#start-register-algorithm and
// https://w3c.github.io/ServiceWorker/#register-algorithm as well as CSP
// validation corresponding to
// https://w3c.github.io/webappsec-csp/#directive-worker-src.
//
// This is extracted out of ServiceWorkerContainer::Register because we validate
// both in the content process as the site of the call, as well as in the parent
// process in the ServiceWorkerManager.
//
// On worker threads, this will involve use of a syncloop until Bug 1901387 is
// addressed, allowing us to call CheckMayLoad off main-thread (OMT).
//
// A global may be optionally provided for reporting purposes; this is desired
// when this is used by ServiceWorkerContainer::Register but not necessary in
// the parent process.
void ServiceWorkerScopeAndScriptAreValid(
    const ClientInfo& aClientInfo, nsIURI* aScopeURI, nsIURI* aScriptURI,
    ErrorResult& aRv, nsIGlobalObject* aGlobalForReporting = nullptr);

bool ServiceWorkersEnabled(JSContext* aCx, JSObject* aGlobal);

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ServiceWorkerUtils_h
