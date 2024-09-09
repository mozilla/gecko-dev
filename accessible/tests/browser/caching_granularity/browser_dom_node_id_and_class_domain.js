/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// NOTE: These aren't testable easily because the DOMNodeIDAndClass domain is
// required in order to instantiate an accessible task. So, we test only that
// the attributes are present in the cache as expected.

// CacheKey::DOMNodeClass, CacheDomain::DOMNodeIDAndClass
addAccessibleTask(
  `<div id="test" class="test">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    verifyAttributeCachedNoRetry(acc, "class");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.DOMNodeIDAndClass,
  }
);

// CacheKey::DOMNodeID, CacheDomain::DOMNodeIDAndClass
addAccessibleTask(
  `<div id="test" class="test">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    verifyAttributeCachedNoRetry(acc, "id");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.DOMNodeIDAndClass,
  }
);
