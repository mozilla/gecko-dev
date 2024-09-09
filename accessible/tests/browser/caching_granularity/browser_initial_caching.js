/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::InputType, no cache domain
addAccessibleTask(
  `<input id="test" type="search"/>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await verifyAttributeCachedNoRetry(acc, "text-input-type");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::MimeType, no cache domain
addAccessibleTask(
  ``,
  async function (browser, docAcc) {
    await verifyAttributeCachedNoRetry(docAcc, "content-type");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::PopupType, no cache domain
addAccessibleTask(
  `<div popover id="test">test</div>`,
  async function (browser, _) {
    info("Showing popover");
    let shown = waitForEvent(EVENT_SHOW, "test");
    await invokeContentTask(browser, [], () => {
      content.document.getElementById("test").showPopover();
    });
    let popover = (await shown).accessible;
    await verifyAttributeCachedNoRetry(popover, "ispopup");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::TagName, no cache domain
addAccessibleTask(
  ``,
  async function (browser, docAcc) {
    await verifyAttributeCachedNoRetry(docAcc, "tag");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);

// CacheKey::ARIARole, no cache domain
addAccessibleTask(
  `<div id="test" role="invalid-role">test</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test");
    await verifyAttributeCachedNoRetry(acc, "role");
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
