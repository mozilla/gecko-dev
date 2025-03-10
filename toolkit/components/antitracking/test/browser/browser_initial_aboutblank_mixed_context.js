/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1951536 - Ensuring the initial about:blank page is a mixed context is
 *               considered first-party and setting cookies won't hit the
 *               invalid first-party partitioned cookie assertion.
 */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.CHIPS.enabled", true],
      [
        "network.cookie.cookieBehavior",
        BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["dom.security.https_first", false],
      ["dom.security.https_only_mode", false],
    ],
  });
});

add_task(async function test_initial_aboutblank_mixed_context() {
  info("Open a http tab.");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_DOMAIN);
  let browser = tab.linkedBrowser;

  info("Open an HTTPS iframe.");

  let iframeBC = await SpecialPowers.spawn(
    browser,
    [TEST_DOMAIN_HTTPS],
    async src => {
      let iframe = content.document.createElement("iframe");

      await new content.Promise(resolve => {
        iframe.onload = resolve;
        iframe.src = src;
        content.document.body.appendChild(iframe);
      });

      return iframe.browsingContext;
    }
  );

  info(
    "Open an initial about:blank page in the HTTPS iframe and set a cookie."
  );

  await SpecialPowers.spawn(iframeBC, [], async function () {
    let iframe = content.document.createElement("iframe");
    iframe.src = "about:blank";
    content.document.body.appendChild(iframe);

    iframe.contentDocument.cookie = "foo=bar";
  });

  info("Verify the cookie is also set in the first-party context.");

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let cookie = content.document.cookie;
    ok(cookie.includes("foo=bar"), "Cookie is set in the first-party context.");
  });

  BrowserTestUtils.removeTab(tab);
});
