/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const tableMarkup = `
<table>
  <thead>
    <tr><th id="header">a</th><th id="colspan-test" colspan="2">b</th></tr>
  </thead>
  <tbody>
    <tr><th id="rowspan-test" rowspan="2">c</th><td rowspan="0">d</td><td>d</td></tr>
    <tr><td id="headers-test" headers="header">f</td></tr>
  </tbody>
</table>
`;

// CacheKey::CellHeaders, CacheDomain::Table
addAccessibleTask(
  tableMarkup,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "headers-test", [
      nsIAccessibleTableCell,
    ]);
    await testCachingPerPlatform(acc, "headers", () => {
      acc.columnHeaderCells;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::ColSpan, CacheDomain::Table
addAccessibleTask(
  tableMarkup,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "colspan-test", [
      nsIAccessibleTableCell,
    ]);
    await testCachingPerPlatform(acc, "colspan", () => {
      acc.columnExtent;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::RowSpan, CacheDomain::Table
addAccessibleTask(
  tableMarkup,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "rowspan-test", [
      nsIAccessibleTableCell,
    ]);
    await testCachingPerPlatform(acc, "rowspan", () => {
      acc.rowExtent;
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::TableLayoutGuess, CacheDomain::Table
addAccessibleTask(
  `<table id="test"><tr><td>a</td></tr></table>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testCachingPerPlatform(acc, "layout-guess", () => {
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
