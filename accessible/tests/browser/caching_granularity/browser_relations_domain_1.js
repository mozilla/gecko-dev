/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::DOMName, CacheDomain::Relations
addAccessibleTask(
  `<input id="test" type="radio" name="test"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "attributeName", () => {
      acc.getRelationByType(0);
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// The following tests are for cache keys that are actually relation keys.
// They don't have a CacheKey:: prefix but are cached nonetheless. We don't
// test reverse relations here since they're not stored in the cache.

// RelationType::LABELLEDBY, CacheDomain::Relations
addAccessibleTask(
  `
  <button aria-labelledby="label" id="labelled">test</button>
  <label id="label">button label</label>
`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "labelled");
    await testAttributeCachePresence(acc, "labelledby", () => {
      acc.getRelationByType(0);
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// RelationType::LABEL_FOR, CacheDomain::Relations
addAccessibleTask(
  `
  <button id="button">test</button>
  <label for="button" id="label">button label</label>
`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "label");
    await testAttributeCachePresence(acc, "for", () => {
      acc.getRelationByType(0);
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// RelationType::CONTROLLER_FOR, CacheDomain::Relations
addAccessibleTask(
  `
  <button aria-controls="controlled" id="controller">test</button>
  <div id="controlled">test</div>
`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "controller");
    await testAttributeCachePresence(acc, "controls", () => {
      acc.getRelationByType(0);
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// Continued in browser_relations_domain_2.js.
// Split up to avoid timeouts in test-verify.
