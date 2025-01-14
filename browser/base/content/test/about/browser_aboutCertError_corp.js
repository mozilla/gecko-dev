/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const AUTH_ROUTE =
  "https://example.com/browser/browser/base/content/test/about/sandbox_corp_iframe.sjs";

add_task(async function test_coopError() {
  let browser;
  let pageLoaded;

  const uri = `${AUTH_ROUTE}?error=coop`;

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, uri);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.browserLoaded(browser, true);
    },
    false
  );

  await pageLoaded;

  // The error is displayed in the newly opened document for COOP
  let lastTab = gBrowser.tabs.at(-1);
  browser = lastTab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], function () {
    const doc = content.document;

    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const titleEl = doc.querySelector(".title-text");
    const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
    is(
      actualDataL10nID,
      "blocked-by-corp-headers-title",
      "Correct error link title (CORP) is set"
    );

    const anchor = doc.querySelector("a");
    const actualAnchorl10nID = anchor.getAttribute("data-l10n-id");
    is(
      actualAnchorl10nID,
      "certerror-coop-learn-more",
      "Correct error link is set"
    );
  });

  BrowserTestUtils.removeTab(lastTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_coepError() {
  let browser;
  let pageLoaded;

  const uri = `${AUTH_ROUTE}?error=coep`;

  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, uri);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  await pageLoaded;

  await SpecialPowers.spawn(browser, [], function () {
    // The error is displayed in the iframe for COEP
    const doc = content.document.querySelector("iframe").contentDocument;

    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const titleEl = doc.querySelector(".title-text");
    const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
    is(
      actualDataL10nID,
      "blocked-by-corp-headers-title",
      "Correct error link title (CORP) is set"
    );

    const anchor = doc.querySelector("a");
    const actualAnchorl10nID = anchor.getAttribute("data-l10n-id");
    is(
      actualAnchorl10nID,
      "certerror-coep-learn-more",
      "Correct error link is set"
    );
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
