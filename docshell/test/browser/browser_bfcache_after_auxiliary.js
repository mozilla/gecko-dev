const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function restore_bfcache_after_auxiliary() {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "dummy_page.html",
    async browser => {
      let initialBC = browser.browsingContext;

      // Mark the initial dummy page as having the dynamic state from the bfcache by replacing the body.
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML = "bfcached";
      });

      BrowserTestUtils.startLoadingURIString(
        browser,
        TEST_PATH + "file_open_about_blank.html"
      );
      await BrowserTestUtils.browserLoaded(browser);
      let openerBC = browser.browsingContext;
      isnot(
        initialBC,
        openerBC,
        "should have put the previous document into the bfcache"
      );

      // Check that the document is currently BFCached
      info("going back for bfcache smoke test");
      browser.goBack();
      await BrowserTestUtils.waitForContentEvent(browser, "pageshow");

      let restoredInnerHTML = await SpecialPowers.spawn(
        browser,
        [],
        () => content.document.body.innerHTML
      );
      is(restoredInnerHTML, "bfcached", "page was restored from bfcache");
      is(
        initialBC,
        browser.browsingContext,
        "returned to previous browsingcontext"
      );

      // Go back forward, and open a pop-up.
      info("restoring after bfcache smoke test");
      browser.goForward();
      await BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      is(
        openerBC,
        browser.browsingContext,
        "returned to opener browsingContext"
      );

      info("opening a popup");
      let waitForPopup = BrowserTestUtils.waitForNewTab(gBrowser);
      await SpecialPowers.spawn(browser, [], () => {
        content.eval(`window.open("dummy_page.html", "popup");`);
        // content.document.querySelector("#open").click()
      });
      let popup = await waitForPopup;
      info("got the popup");
      let popupBC = popup.linkedBrowser.browsingContext;
      is(popupBC.opener, openerBC, "opener is openerBC");

      // Now that the pop-up is opened, try to go back again.
      info("trying to go back with a popup");
      browser.goBack();
      await BrowserTestUtils.waitForContentEvent(browser, "pageshow");

      let nonRestoredInnerHTML = await SpecialPowers.spawn(
        browser,
        [],
        () => content.document.body.innerHTML
      );
      isnot(
        nonRestoredInnerHTML,
        "bfcached",
        "page was not restored from bfcache"
      );
      is(openerBC, browser.browsingContext, "returned to the new bc");

      await BrowserTestUtils.removeTab(popup);
    }
  );
});
