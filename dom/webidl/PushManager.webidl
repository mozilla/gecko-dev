/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/.
*
* The origin of this IDL file is
* https://w3c.github.io/push-api/
*/

dictionary PushSubscriptionOptionsInit {
  // boolean userVisibleOnly = false;
  (BufferSource or DOMString)? applicationServerKey = null;
};

// The main thread JS implementation. Please see comments in
// dom/push/PushManager.h for the split between PushManagerImpl and PushManager.
[JSImplementation="@mozilla.org/push/PushManager;1",
 ChromeOnly,
 Exposed=Window]
interface PushManagerImpl {
  [Throws]
  constructor(DOMString scope);

  Promise<PushSubscription>    subscribe(optional PushSubscriptionOptionsInit options = {});
  Promise<PushSubscription?>   getSubscription();
  Promise<PermissionState> permissionState(optional PushSubscriptionOptionsInit options = {});
};

[Exposed=(Window,Worker), Func="PushManager::IsEnabled"]
interface PushManager {
  [Throws, ChromeOnly]
  constructor(DOMString scope);

  // TODO: Use FrozenArray once available. (Bug 1236777)
  // [SameObject] static readonly attribute FrozenArray<DOMString> supportedContentEncodings;
  // XXX: We can't use sequence here either:
  // 1. [Cached] is not supported for static members (Bug 1363870)
  // 2. [Cached] is required for sequence typed attributes
  // [Frozen, Cached, Pure] static readonly attribute sequence<DOMString> supportedContentEncodings;
  [Throws] static readonly attribute object supportedContentEncodings;

  [Throws, UseCounter]
  Promise<PushSubscription>    subscribe(optional PushSubscriptionOptionsInit options = {});
  [Throws]
  Promise<PushSubscription?>   getSubscription();
  [Throws]
  Promise<PermissionState> permissionState(optional PushSubscriptionOptionsInit options = {});
};
