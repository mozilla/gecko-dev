/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::InnerHTML, CacheDomain::InnerHTML
addAccessibleTask(
  `<math id="test"><mfrac><mi>x</mi><mi>y</mi></mfrac></math>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await verifyAttributeCachedNoRetry(acc, "html");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    // Entire test runs with domain already active because there's no XPC method
    // to trigger caching of InnerHTML.
    cacheDomains: CacheDomain.InnerHTML,
  }
);
