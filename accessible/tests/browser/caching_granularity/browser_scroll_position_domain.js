/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::ScrollPosition, CacheDomain::ScrollPosition
addAccessibleTask(
  `<div id="test" style="height:200vh;"></div>`,
  async function (browser, docAcc) {
    await testAttributeCachePresence(docAcc, "scroll-position", () => {
      docAcc.getBounds({}, {}, {}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
