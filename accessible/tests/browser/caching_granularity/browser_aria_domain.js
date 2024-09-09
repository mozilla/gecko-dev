/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::ARIAAttributes, CacheDomain::ARIA
addAccessibleTask(
  `<div id="test" aria-live="polite">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "aria", () => {
      acc.attributes;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
