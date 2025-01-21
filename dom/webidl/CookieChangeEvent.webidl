/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is:
 * https://wicg.github.io/cookie-store/#CookieChangeEvent
 */

[Exposed=(ServiceWorker,Window),
 SecureContext,
 Pref="dom.cookieStore.enabled"]
interface CookieChangeEvent : Event {
  constructor(DOMString type, optional CookieChangeEventInit eventInitDict = {});
  // TODO: Use FrozenArray once available. (Bug 1236777)
  // [SameObject] readonly attribute FrozenArray<CookieListItem> changed;
  // [SameObject] readonly attribute FrozenArray<CookieListItem> deleted;
  [Frozen, Cached, Constant]
  readonly attribute sequence<CookieListItem> changed;
  [Frozen, Cached, Constant]
  readonly attribute sequence<CookieListItem> deleted;
};

dictionary CookieChangeEventInit : EventInit {
  CookieList changed;
  CookieList deleted;
};
