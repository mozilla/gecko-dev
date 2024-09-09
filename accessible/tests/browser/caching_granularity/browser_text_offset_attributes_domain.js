/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// CacheKey::Language, CacheDomain::TextOffsetAttributes | Text
// Cache population triggered via TextOffsetAttributes domain query.
addAccessibleTask(
  `<div id="test" contenteditable spellcheck="true">misspelld txt</div>`,
  async function (browser, docAcc) {
    let acc = findAccessibleChildByID(docAcc, "test", [nsIAccessibleText]);
    // Just focusing the text should not trigger cache presence of
    // TextOffsetAttributes. The AT must request it directly.
    info("Focusing the misspelled text.");
    acc.takeFocus();
    await waitForEvent(EVENT_TEXT_ATTRIBUTE_CHANGED);
    let textLeaf = acc.firstChild;
    await testAttributeCachePresence(textLeaf, "spelling", () => {
      acc.getTextAttributes({}, {}, {}, {});
    });
  },
  {
    topLevel: true,
    iframe: true,
    remoteIframe: true,
    cacheDomains: CacheDomain.None,
  }
);
