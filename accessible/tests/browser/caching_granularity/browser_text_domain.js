/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::Language, CacheDomain::Text
addAccessibleTask(
  `<input id="test" type="radio" lang="fr"/>`,
  async function (browser, docAcc) {
    console.log("before findAccessibleChildByID");
    let acc = findAccessibleChildByID(docAcc, "test");
    await testCachingPerPlatform(acc, "language", () => {
      acc.language;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::Text, CacheDomain::Text
addAccessibleTask(
  `<div id="test">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test").firstChild;
    await testCachingPerPlatform(acc, "text", () => {
      acc.name;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::TextAttributes, CacheDomain::Text
addAccessibleTask(
  `<div id="test">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test").firstChild;
    await testCachingPerPlatform(acc, "style", () => {
      // This is a bit of a shortcut to TextAttributes because querying
      // the TextAttributes key directly is difficult to do simply.
      acc.language;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
