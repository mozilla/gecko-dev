/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::Viewport, CacheDomain::Viewport
addAccessibleTask(
  `<div id="test">test</div>`,
  async function (browser, docAcc) {
    // On Linux and macOS, inconsistently, the viewport cache is populated on
    // the DocAccessible without explicitly requesting it and is present once
    // the document has loaded.
    await testCachingPerPlatform(docAcc, "viewport", () => {
      docAcc.getState({}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
