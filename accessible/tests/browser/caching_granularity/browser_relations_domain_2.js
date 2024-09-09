/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Continued from browser_relations_domain_1.js.
// Split up to avoid timeouts in test-verify.

// RelationType::CONTROLLED_BY, CacheDomain::Relations
addAccessibleTask(
  `
  <button id="controller">test</button>
  <output id="controlled" for="controller">test</div>
`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "controlled");
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

// RelationType::DESCRIBED_BY, CacheDomain::Relations
addAccessibleTask(
  `
  <button aria-describedby="label" id="button">test</button>
  <label id="label">button label</label>
`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "button");
    await testAttributeCachePresence(acc, "describedby", () => {
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

// RelationType::FLOWS_TO, CacheDomain::Relations
addAccessibleTask(
  `
  <div id="flowto" aria-flowto="flowfrom">flow to</div>
  <div id="flowfrom">flow from</div>
`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "flowto");
    await testAttributeCachePresence(acc, "flowto", () => {
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

// RelationType::DETAILS, CacheDomain::Relations
addAccessibleTask(
  `
  <input id="has_details" aria-details="details"/>
  <div id="details"></div>
`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "has_details");
    await testAttributeCachePresence(acc, "details", () => {
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

// RelationType::ERRORMSG, CacheDomain::Relations
addAccessibleTask(
  `
  <input id="has_error" aria-errormessage="error">
  <div id="error"></div>
`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "has_error");
    await testAttributeCachePresence(acc, "errormessage", () => {
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
