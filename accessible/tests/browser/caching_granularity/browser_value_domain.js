/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::MaxValue, CacheDomain::Value
addAccessibleTask(
  `<input id="test" type="range" min="0" max="100" value="50"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test", [nsIAccessibleValue]);
    await testAttributeCachePresence(acc, "max", () => {
      acc.maximumValue;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::MinValue, CacheDomain::Value
addAccessibleTask(
  `<input id="test" type="range" min="0" max="100" value="50"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test", [nsIAccessibleValue]);
    await testAttributeCachePresence(acc, "min", () => {
      acc.minimumValue;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::NumericValue, CacheDomain::Value
addAccessibleTask(
  `<input id="test" type="range" min="0" max="100" value="50"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test", [nsIAccessibleValue]);
    await testAttributeCachePresence(acc, "value", () => {
      acc.currentValue;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::SrcURL, CacheDomain::Value
addAccessibleTask(
  `<img id="test" src="image.jpg"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "src", () => {
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

// CacheKey::Step, CacheDomain::Value
addAccessibleTask(
  `<input id="test" type="range" min="0" max="100" value="50"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test", [nsIAccessibleValue]);
    await testAttributeCachePresence(acc, "step", () => {
      acc.minimumIncrement;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::TextValue, CacheDomain::Value
addAccessibleTask(
  `<div id="test" role="slider" aria-valuetext="value"></div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "valuetext", () => {
      acc.value;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::ValueRegion, CacheDomain::Value
addAccessibleTask(
  `<meter id="test"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "valuetype", () => {
      acc.value;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
