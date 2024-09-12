/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://wicg.github.io/cookie-store/#idl-index
 *
 * Copyright © 2024 the Contributors to the Cookie Store API Specification,
 * published by the Web Platform Incubator Community Group under the W3C
 * Community Contributor License Agreement (CLA). A human-readable summary is
 * available.
 */

[Exposed=(ServiceWorker,Window),
 SecureContext,
 Pref="dom.cookieStore.enabled"]
interface CookieStore : EventTarget {
  [Throws]
  Promise<CookieListItem?> get(USVString name);
  [Throws]
  Promise<CookieListItem?> get(optional CookieStoreGetOptions options = {});

  [Throws]
  Promise<CookieList> getAll(USVString name);
  [Throws]
  Promise<CookieList> getAll(optional CookieStoreGetOptions options = {});

  [Throws]
  Promise<undefined> set(USVString name, USVString value);
  [Throws]
  Promise<undefined> set(CookieInit options);

  [Throws]
  Promise<undefined> delete(USVString name);
  [Throws]
  Promise<undefined> delete(CookieStoreDeleteOptions options);

  /* Bug 1475599 - By spec, `onchange` should be available only on `Window`,
   * but because we do not want to implement the ExtendableCookieChangeEvent
   * for ServiceWorker, we have decided to expose this EventHandler everywhere.
   */
  //[Exposed=Window]
  attribute EventHandler onchange;
};

dictionary CookieStoreGetOptions {
  USVString name;
  USVString url;
};

enum CookieSameSite {
  "strict",
  "lax",
  "none"
};

dictionary CookieInit {
  required USVString name;
  required USVString value;
  DOMHighResTimeStamp? expires = null;
  USVString? domain = null;
  USVString path = "/";
  CookieSameSite sameSite = "strict";
  boolean partitioned = false;
};

dictionary CookieStoreDeleteOptions {
  required USVString name;
  USVString? domain = null;
  USVString path = "/";
  boolean partitioned = false;
};

dictionary CookieListItem {
  USVString name;
  USVString value;

  /* Bug 1475599 - We decide to do not implement the entire cookie-store spec.
   * Instead, we implement only the subset that is compatible with document.cookie */
  // USVString? domain;
  // USVString path;
  // DOMHighResTimeStamp? expires;
  // boolean secure;
  // CookieSameSite sameSite;
  // boolean partitioned;
};

typedef sequence<CookieListItem> CookieList;

/* Bug 1475599 - We decide to do not implement the entire cookie-store spec.
 * Instead, we implement only the subset that is compatible with document.cookie
[Exposed=(ServiceWorker,Window),
 SecureContext]
interface CookieStoreManager {
  Promise<undefined> subscribe(sequence<CookieStoreGetOptions> subscriptions);
  Promise<sequence<CookieStoreGetOptions>> getSubscriptions();
  Promise<undefined> unsubscribe(sequence<CookieStoreGetOptions> subscriptions);
};
*/
