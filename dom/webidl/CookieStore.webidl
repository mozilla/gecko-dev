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
  [Throws, UseCounter]
  Promise<CookieListItem?> get(USVString name);
  [Throws, UseCounter]
  Promise<CookieListItem?> get(optional CookieStoreGetOptions options = {});

  [Throws, UseCounter]
  Promise<CookieList> getAll(USVString name);
  [Throws, UseCounter]
  Promise<CookieList> getAll(optional CookieStoreGetOptions options = {});

  [Throws, UseCounter]
  Promise<undefined> set(USVString name, USVString value);
  [Throws, UseCounter]
  Promise<undefined> set(CookieInit options);

  [Throws, UseCounter]
  Promise<undefined> delete(USVString name);
  [Throws, UseCounter]
  Promise<undefined> delete(CookieStoreDeleteOptions options);

  [Exposed=Window]
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
  /* UTF8String semantics match USVString */

  UTF8String name;
  UTF8String value;

  [Pref="dom.cookieStore.extra.enabled"]
  UTF8String path;

  [Pref="dom.cookieStore.extra.enabled"]
  UTF8String? domain;

  [Pref="dom.cookieStore.extra.enabled"]
  DOMHighResTimeStamp? expires;

  [Pref="dom.cookieStore.extra.enabled"]
  boolean secure;

  [Pref="dom.cookieStore.extra.enabled"]
  CookieSameSite sameSite;

  [Pref="dom.cookieStore.extra.enabled"]
  boolean partitioned;
};

typedef sequence<CookieListItem> CookieList;
