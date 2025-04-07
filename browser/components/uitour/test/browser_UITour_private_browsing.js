/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that UITour will work in private browsing windows with
 * tabs pointed at sites that have the uitour permission set to
 * allowed.
 */
add_task(async function test_privatebrowsing_window() {
  // These two constants point to origins that have a default
  // uitour allow entry in browser/app/permissions. The expectation is
  // that these defaults are special, in that they'll apply for both
  // private and non-private contexts.
  const ABOUT_ORIGIN_WITH_UITOUR_DEFAULT = "about:newtab";
  const HTTPS_ORIGIN_WITH_UITOUR_DEFAULT = "https://www.mozilla.org";

  let { UITourUtils } = ChromeUtils.importESModule(
    "moz-src:///browser/components/uitour/UITourUtils.sys.mjs"
  );

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let browser = win.gBrowser.selectedBrowser;

  for (let uri of [
    ABOUT_ORIGIN_WITH_UITOUR_DEFAULT,
    HTTPS_ORIGIN_WITH_UITOUR_DEFAULT,
  ]) {
    BrowserTestUtils.startLoadingURIString(browser, uri);
    await BrowserTestUtils.browserLoaded(browser);

    Assert.ok(
      UITourUtils.ensureTrustedOrigin(
        browser.browsingContext.currentWindowGlobal
      ),
      "Page should be considered trusted for UITour in the parent."
    );

    await SpecialPowers.spawn(browser, [], async () => {
      let actor = content.windowGlobalChild.getActor("UITour");
      // eslint-disable-next-line no-shadow
      let { UITourUtils } = ChromeUtils.importESModule(
        "moz-src:///browser/components/uitour/UITourUtils.sys.mjs"
      );
      Assert.ok(
        UITourUtils.ensureTrustedOrigin(actor.manager),
        "Page should be considered trusted for UITour."
      );
    });
  }
  await BrowserTestUtils.closeWindow(win);
});
