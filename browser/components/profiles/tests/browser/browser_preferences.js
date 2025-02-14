/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Note: copied from preferences head.js. We can remove this when we migrate
// this test into that component.
async function openPreferencesViaOpenPreferencesAPI(aPane, aOptions) {
  let finalPaneEvent = Services.prefs.getBoolPref("identity.fxaccounts.enabled")
    ? "sync-pane-loaded"
    : "privacy-pane-loaded";
  let finalPrefPaneLoaded = TestUtils.topicObserved(finalPaneEvent, () => true);
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, "about:blank", {
    allowInheritPrincipal: true,
  });
  openPreferences(aPane, aOptions);
  let newTabBrowser = gBrowser.selectedBrowser;

  if (!newTabBrowser.contentWindow) {
    await BrowserTestUtils.waitForEvent(newTabBrowser, "Initialized", true);
    await BrowserTestUtils.waitForEvent(newTabBrowser.contentWindow, "load");
    await finalPrefPaneLoaded;
  }

  let win = gBrowser.contentWindow;
  let selectedPane = win.history.state;
  if (!aOptions || !aOptions.leaveOpen) {
    gBrowser.removeCurrentTab();
  }
  return { selectedPane };
}

// Note: copied from preferences head.js. We can remove this when we migrate
// this test into that component.
function promiseLoadSubDialog(aURL) {
  return new Promise(resolve => {
    content.gSubDialog._dialogStack.addEventListener(
      "dialogopen",
      function dialogopen(aEvent) {
        if (
          aEvent.detail.dialog._frame.contentWindow.location == "about:blank"
        ) {
          return;
        }
        content.gSubDialog._dialogStack.removeEventListener(
          "dialogopen",
          dialogopen
        );

        is(
          aEvent.detail.dialog._frame.contentWindow.location.toString(),
          aURL,
          "Check the proper URL is loaded"
        );

        // Check visibility
        ok(
          BrowserTestUtils.isVisible(aEvent.detail.dialog._overlay),
          "Overlay is visible"
        );

        // Check that stylesheets were injected
        let expectedStyleSheetURLs =
          aEvent.detail.dialog._injectedStyleSheets.slice(0);
        for (let styleSheet of aEvent.detail.dialog._frame.contentDocument
          .styleSheets) {
          let i = expectedStyleSheetURLs.indexOf(styleSheet.href);
          if (i >= 0) {
            info("found " + styleSheet.href);
            expectedStyleSheetURLs.splice(i, 1);
          }
        }
        is(
          expectedStyleSheetURLs.length,
          0,
          "All expectedStyleSheetURLs should have been found"
        );

        // Wait for the next event tick to make sure the remaining part of the
        // testcase runs after the dialog gets ready for input.
        executeSoon(() => resolve(aEvent.detail.dialog._frame.contentWindow));
      }
    );
  });
}

add_task(async function testHiddenWhenDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.profiles.enabled", false]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  let profilesCategory = doc.getElementById("profilesGroup");
  ok(profilesCategory, "The category exists");
  ok(!BrowserTestUtils.isVisible(profilesCategory), "The category is hidden");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await SpecialPowers.popPrefEnv();
});

add_task(async function testEnabled() {
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  // Verify the profiles section is shown when enabled.
  let profilesCategory = doc.getElementById("profilesGroup");
  ok(SelectableProfileService.isEnabled, "Profiles should be enabled");
  ok(profilesCategory, "The category exists");
  ok(BrowserTestUtils.isVisible(profilesCategory), "The category is visible");

  // Verify the Learn More link exists and points to the right place.
  let learnMore = doc.getElementById("profile-management-learn-more");
  Assert.equal(
    "http://127.0.0.1:8888/support-dummy/profile-management",
    learnMore.href,
    "Learn More link should have expected URL"
  );

  // Verify that clicking the button shows the manage screen in a subdialog.
  let promiseSubDialogLoaded = promiseLoadSubDialog("about:profilemanager");
  let profilesButton = doc.getElementById("manage-profiles");
  profilesButton.click();
  await promiseSubDialogLoaded;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
