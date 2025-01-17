/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const AUTH_ROUTE =
  "https://example.com/browser/browser/base/content/test/about/sandbox_corp_iframe.sjs";

/**
 * Waits for a single TabOpen event, then attaches waitForErrorPage
 * to the newly opened tab’s browser. Returns the tab once the error page loads.
 */
function waitForNewTabAndErrorPage() {
  let tabLoaded = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen"
  );

  return (async () => {
    let event = await tabLoaded;
    let newTab = event.target;
    let newBrowser = newTab.linkedBrowser;

    let errorPageLoaded = BrowserTestUtils.waitForErrorPage(newBrowser);
    await errorPageLoaded;

    return newTab;
  })();
}

add_task(async function test_coopError() {
  let iframeTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    `${AUTH_ROUTE}?error=coop`
  );
  let popupTabLoaded = waitForNewTabAndErrorPage();

  await SpecialPowers.spawn(iframeTab.linkedBrowser, [], async () => {
    let button = content.document
      .querySelector("iframe")
      .contentDocument.querySelector("#openPopupButton");
    if (!button) {
      ok(false, "Popup button not found!");
    }
    button.click();
  });

  let popUpTab = await popupTabLoaded;
  let popUpBrowser = popUpTab.linkedBrowser;

  await SpecialPowers.spawn(popUpBrowser, [], function () {
    const doc = content.document;
    const titleEl = doc.querySelector(".title-text");
    const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
    is(
      actualDataL10nID,
      "general-body-title",
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

  BrowserTestUtils.removeTab(iframeTab);
  BrowserTestUtils.removeTab(popUpTab);
});
