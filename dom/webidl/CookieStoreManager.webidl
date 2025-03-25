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
 Pref="dom.cookieStore.manager.enabled"]
interface CookieStoreManager {
  [Throws]
  Promise<undefined> subscribe(sequence<CookieStoreGetOptions> subscriptions);

  [Throws]
  Promise<sequence<CookieStoreGetOptions>> getSubscriptions();

  [Throws]
  Promise<undefined> unsubscribe(sequence<CookieStoreGetOptions> subscriptions);
};
