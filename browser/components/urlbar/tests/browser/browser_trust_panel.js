/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test TrustPanel.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ContentBlockingAllowList:
    "resource://gre/modules/ContentBlockingAllowList.sys.mjs",
});

const ETP_ACTIVE_ICON = 'url("chrome://browser/skin/trust-icon-active.svg")';
const ETP_DISABLED_ICON =
  'url("chrome://browser/skin/trust-icon-disabled.svg")';

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.scotchBonnet.enableOverride", true],
      ["browser.urlbar.trustPanel.featureGate", true],
    ],
  });
});

let urlbarBtn = win => win.document.getElementById("trust-icon");
let urlbarIcon = win =>
  gBrowser.ownerGlobal
    .getComputedStyle(urlbarBtn(win))
    .getPropertyValue("list-style-image");

add_task(async function basic_test() {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "https://example.com",
    waitForLoad: true,
  });

  Assert.equal(urlbarIcon(window), ETP_ACTIVE_ICON, "Showing trusted icon");

  let popupShown = BrowserTestUtils.waitForEvent(window.document, "popupshown");
  EventUtils.synthesizeMouseAtCenter(urlbarBtn(window), {}, window);
  await popupShown;

  let waitForReload = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeMouseAtCenter(
    window.document.getElementById("trustpanel-toggle"),
    {},
    window
  );
  await waitForReload;

  Assert.equal(
    urlbarIcon(window),
    ETP_DISABLED_ICON,
    "Showing ETP disabled icon"
  );

  ContentBlockingAllowList.remove(window.gBrowser.selectedBrowser);
  BrowserTestUtils.removeTab(tab);
});
