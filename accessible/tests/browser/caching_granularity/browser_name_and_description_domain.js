/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// NOTE: These aren't testable easily because the NameAndDescription domain is
// required in order to instantiate an accessible task. So, we test only that
// the attributes are present in the cache as expected.

// CacheKey::Description, CacheDomain::NameAndDescription
addAccessibleTask(
  `<div id="test" aria-description="test">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    verifyAttributeCachedNoRetry(acc, "description");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.NameAndDescription,
  }
);

// CacheKey::HTMLPlaceholder, CacheDomain::NameAndDescription
addAccessibleTask(
  `<input type="text" aria-label="label" id="test" placeholder="test"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    verifyAttributeCachedNoRetry(acc, "placeholder");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.NameAndDescription,
  }
);

// CacheKey::Name, CacheDomain::NameAndDescription
addAccessibleTask(
  `<div id="test" aria-label="name">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    verifyAttributeCachedNoRetry(acc, "name");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.NameAndDescription,
  }
);

// CacheKey::NameValueFlag, CacheDomain::NameAndDescription
addAccessibleTask(
  `<h3 id="test"><p>test</p></h3>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    verifyAttributeCachedNoRetry(acc, "explicit-name");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.NameAndDescription,
  }
);
