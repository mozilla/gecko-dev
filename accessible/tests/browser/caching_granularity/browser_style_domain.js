/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::CSSDisplay, CacheDomain::Style
addAccessibleTask(
  `<div id="test" style="display:flex;">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "display", () => {
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

// CacheKey::CSSOverflow, CacheDomain::Style
addAccessibleTask(
  `<div id="test" style="overflow:hidden;">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "overflow", () => {
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

// CacheKey::CssPosition, CacheDomain::Style
addAccessibleTask(
  `<div id="test" style="position:fixed;">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "position", () => {
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

// CacheKey::Opacity, CacheDomain::Style
addAccessibleTask(
  `<div id="test" style="opacity:0.5;">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "opacity", () => {
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
