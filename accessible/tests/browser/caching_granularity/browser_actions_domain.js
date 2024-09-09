/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::AccessKey, CacheDomain::Actions
addAccessibleTask(
  `<div id="test" accesskey="x">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "accesskey", () => {
      acc.accessKey;
    });
  },
  {
    topLevel: true,
    iframe: false, // AccessKey issues with iframe - See Bug 1796846
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::HasLongdesc, CacheDomain::Actions
addAccessibleTask(
  `<img id="test" src="http://example.com/a11y/accessible/tests/mochitest/moz.png" longdesc="http://example.com">`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "longdesc", () => {
      acc.actionCount;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::PrimaryAction, CacheDomain::Actions
addAccessibleTask(
  `<button id="test" onclick="console.log('test');">test</button>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "action", () => {
      acc.actionCount;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
