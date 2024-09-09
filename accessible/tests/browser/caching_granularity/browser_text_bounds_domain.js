/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::TextBounds, CacheDomain::TextBounds
addAccessibleTask(
  `<div id="test">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test").firstChild;
    await testAttributeCachePresence(acc, "characterData", () => {
      acc.getChildAtPoint({}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::TextLineStarts, CacheDomain::TextBounds
addAccessibleTask(
  `<div id="test">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test").firstChild;
    await testAttributeCachePresence(acc, "line", () => {
      acc.getChildAtPoint({}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
