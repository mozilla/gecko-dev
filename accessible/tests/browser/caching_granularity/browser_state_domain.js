/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::ARIASelected, CacheDomain::State
addAccessibleTask(
  `<div id="test" role="tab" aria-selected="true"></div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "selected", () => {
      acc.getState({}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::State, CacheDomain::State
addAccessibleTask(
  `<div id="test"></div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "state", () => {
      acc.getState({}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
