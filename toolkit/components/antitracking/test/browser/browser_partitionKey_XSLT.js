/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Bug 1939516 - Verify if we populate the partitionKey for XSLT pages */

"use strict";

const TEST_XSLT_URL = TEST_DOMAIN_HTTPS + TEST_PATH + "file_xslt.xsl";

add_task(async function test_partitionKey_XSLT() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_XSLT_URL
  );

  is(
    tab.linkedBrowser.browsingContext.currentWindowGlobal.cookieJarSettings
      .partitionKey,
    "(https,example.net)",
    "The XSLT page has the correct partitionKey"
  );

  BrowserTestUtils.removeTab(tab);
});
