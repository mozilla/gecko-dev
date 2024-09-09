/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheDomain::Bounds, CacheKey::CrossDocOffset
addAccessibleTask(
  `<div id="test">test</div>`,
  async function (browser, _, docAcc) {
    // Translate the iframe, which should modify cross-process offset.
    info("Setting the transform on the iframe");
    await SpecialPowers.spawn(browser, [DEFAULT_IFRAME_ID], iframeID => {
      let elm = content.document.getElementById(iframeID);
      elm.style.transform = "translate(100px,100px)";
    });
    await waitForContentPaint(browser);

    let acc = findAccessibleChildByID(docAcc, DEFAULT_IFRAME_ID);
    await testAttributeCachePresence(acc, "crossorigin", () => {
      // Querying bounds queries the CrossDocOffset info.
      acc.getBounds({}, {}, {}, {});
    });
  },
  {
    topLevel: false,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::IsClipped, CacheDomain::Bounds
addAccessibleTask(
  `<div id="generic"><span aria-hidden="true" id="visible">Mozilla</span><span id="invisible" style="display: block !important;border: 0 !important;clip: rect(0 0 0 0) !important;height: 1px !important;margin: -1px !important;overflow: hidden !important;padding: 0 !important;position: absolute !important;white-space: nowrap !important;width: 1px !important;">Mozilla</span><br>I am some other text</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "invisible");
    await testAttributeCachePresence(acc, "clip-rule", () => {
      // Querying bounds queries the IsClipped info.
      acc.getBounds({}, {}, {}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::ParentRelativeBounds, CacheDomain::Bounds
addAccessibleTask(
  `<br><div id="test">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await testAttributeCachePresence(acc, "relative-bounds", () => {
      // Querying bounds queries the ParentRelativeBounds info.
      acc.getBounds({}, {}, {}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
