/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/ServiceWorker/#serviceworkercontainer
 *
 */

// ServiceWorkersEnabled internalizes the SecureContext check because we have a
// devtools affordance that allows this to pass on http as well as a test pref.
[Func="ServiceWorkersEnabled",
 Exposed=(Window,Worker)]
interface ServiceWorkerContainer : EventTarget {
  readonly attribute ServiceWorker? controller;

  [Throws]
  readonly attribute Promise<ServiceWorkerRegistration> ready;

  [NewObject, NeedsCallerType]
  Promise<ServiceWorkerRegistration> register(USVString scriptURL,
                                              optional RegistrationOptions options = {});

  [NewObject]
  Promise<(ServiceWorkerRegistration or undefined)> getRegistration(optional USVString documentURL = "");

  [NewObject]
  Promise<sequence<ServiceWorkerRegistration>> getRegistrations();

  undefined startMessages();

  attribute EventHandler oncontrollerchange;
  attribute EventHandler onmessage;
  attribute EventHandler onmessageerror;
};

dictionary RegistrationOptions {
  USVString scope;
  ServiceWorkerUpdateViaCache updateViaCache = "imports";
};
