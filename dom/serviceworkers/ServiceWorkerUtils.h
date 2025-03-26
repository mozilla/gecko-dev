/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _mozilla_dom_ServiceWorkerUtils_h
#define _mozilla_dom_ServiceWorkerUtils_h

#include "mozilla/MozPromise.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/IPCNavigationPreloadState.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "nsTArray.h"

class nsIGlobalObject;
class nsIURI;

namespace mozilla {

class CopyableErrorResult;
class ErrorResult;

namespace dom {

class Client;
class ClientInfo;
class ClientInfoAndState;
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

using NotificationsPromise =
    MozPromise<CopyableTArray<IPCNotification>, nsresult, false>;

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

// WebIDL enabling function; this does *not* consider the StorageAccess value
// for the global, just interface exposure.
bool ServiceWorkersEnabled(JSContext* aCx, JSObject* aGlobal);

// Perform a StorageAccess policy check for whether ServiceWorkers should work
// in this global / be able to communicate with ServiceWorkers from this global.
//
// Note that this check should not directly be used for assertions; callers need
// to ensure that about:blank and Blob URL globals that are defined to inherit
// controllers pass the assertion check.  This is to handle situations like
// those bug 1441133 where a global is controlled when storage access is
// granted to the origin, then the storage access is revoked, and then a global
// is created that would inherit the controller.
//
// Also note that StorageAccess.h defines a function
// `StorageAllowedForServiceWorker` which is a lower level function akin to
// `StorageAllowedForWindow` that helps determine the approprioate
// `StorageAccess` value for a new global that has a principal but not a channel
// or window available.  This method is downstream of those calls and depends
// on the `StorageAccess` value they compute.
bool ServiceWorkersStorageAllowedForGlobal(nsIGlobalObject* aGlobal);

// Perform a StorageAccess policy check for whether the given Client has
// appropriate StorageAccess to be exposed to the Clients API.
//
// Note that Window Clients lose storage access when they become not fully
// active.
bool ServiceWorkersStorageAllowedForClient(
    const ClientInfoAndState& aInfoAndState);

}  // namespace dom
}  // namespace mozilla

#endif  // _mozilla_dom_ServiceWorkerUtils_h
