/**
 * Test: browser_identityPopup_clearSiteData_privateBrowsingMode.js
 *
 * This browser-chrome test verifies that the "Clear Site Data" controls
 * (button and footer) in the identity popup are correctly hidden in
 * private browsing mode.
 *
 * The test opens a private browsing window, loads a known HTTPS page
 * (`https://example.com/`), opens the identity popup via the identity
 * icon, and checks for the visibility of:
 *
 * - `identity-popup-clear-sitedata-footer`
 * - `identity-popup-clear-sitedata-button`
 *
 * Expected behavior:
 * These elements should not be visible while in private browsing mode.
 *
 * The test uses `BrowserTestUtils.withNewTab()` to load the page in a new
 * tab within the private window and ensures cleanup of both the tab and
 * window after the test completes.
 *
 */

add_task(async function clearSiteDataHidden() {
  const TEST_URL = "https://example.com/";

  // Open a private browsing window
  const winPrivate = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Use withNewTab to open a tab in the private window
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: winPrivate.gBrowser,
      url: TEST_URL,
    },
    async _browser => {
      let doc = winPrivate.document;
      let { gIdentityHandler } = winPrivate;

      let panelShown = BrowserTestUtils.waitForEvent(doc, "popupshown");
      gIdentityHandler._identityIconBox.click();
      await panelShown;

      let clearFooter = doc.getElementById(
        "identity-popup-clear-sitedata-footer"
      );
      let clearButton = doc.getElementById(
        "identity-popup-clear-sitedata-button"
      );

      Assert.ok(
        BrowserTestUtils.isHidden(clearFooter),
        "The clear data footer should be hidden in private browsing."
      );
      Assert.ok(
        BrowserTestUtils.isHidden(clearButton),
        "The clear data button should be hidden in private browsing."
      );
    }
  );

  await BrowserTestUtils.closeWindow(winPrivate);
});
