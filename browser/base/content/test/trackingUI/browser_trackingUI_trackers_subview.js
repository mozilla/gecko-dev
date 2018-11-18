/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";

const TP_PREF = "privacy.trackingprotection.enabled";

add_task(async function setup() {
  await UrlClassifierTestUtils.addTestTrackers();
});

function openIdentityPopup() {
  let mainView = document.getElementById("identity-popup-mainView");
  let viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  gIdentityHandler._identityBox.click();
  return viewShown;
}

function waitForSecurityChange(blocked) {
  return new Promise(resolve => {
    let webProgressListener = {
      onStateChange: () => {},
      onStatusChange: () => {},
      onLocationChange: () => {},
      onSecurityChange: (webProgress, request, oldState, state) => {
        if ((!blocked && state & Ci.nsIWebProgressListener.STATE_LOADED_TRACKING_CONTENT) ||
            (blocked && state & Ci.nsIWebProgressListener.STATE_BLOCKED_TRACKING_CONTENT)) {
          gBrowser.removeProgressListener(webProgressListener);
          resolve();
        }
      },
      onProgressChange: () => {},
      QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener]),
    };

    gBrowser.addProgressListener(webProgressListener);
  });
}

async function assertSitesListed(blocked) {
  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function(browser) {
    await openIdentityPopup();

    let categoryItem =
      document.getElementById("identity-popup-content-blocking-category-tracking-protection");
    ok(BrowserTestUtils.is_visible(categoryItem), "TP category item is visible");
    let trackersView = document.getElementById("identity-popup-trackersView");
    let viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Trackers view was shown");

    let listItems = document.querySelectorAll(".identity-popup-trackersView-list-item");
    is(listItems.length, 1, "We have 1 tracker in the list");

    let strictInfo = document.getElementById("identity-popup-trackersView-strict-info");
    is(BrowserTestUtils.is_hidden(strictInfo), Services.prefs.getBoolPref(TP_PREF),
      "Strict info is hidden if TP is enabled.");

    let mainView = document.getElementById("identity-popup-mainView");
    viewShown = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
    let backButton = trackersView.querySelector(".subviewbutton-back");
    backButton.click();
    await viewShown;

    ok(true, "Main view was shown");

    let change = waitForSecurityChange(blocked);

    await ContentTask.spawn(browser, {}, function() {
      content.postMessage("more-tracking", "*");
    });

    await change;

    viewShown = BrowserTestUtils.waitForEvent(trackersView, "ViewShown");
    categoryItem.click();
    await viewShown;

    ok(true, "Trackers view was shown");

    listItems = Array.from(document.querySelectorAll(".identity-popup-trackersView-list-item"));
    is(listItems.length, 2, "We have 2 trackers in the list");

    let listItem = listItems.find(item => item.querySelector("label").value == "trackertest.org");
    ok(listItem, "Has an item for trackertest.org");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    is(listItem.classList.contains("allowed"), !blocked,
      "Indicates whether the tracker was blocked or allowed");

    listItem = listItems.find(item => item.querySelector("label").value == "itisatracker.org");
    ok(listItem, "Has an item for itisatracker.org");
    ok(BrowserTestUtils.is_visible(listItem), "List item is visible");
    is(listItem.classList.contains("allowed"), !blocked,
      "Indicates whether the tracker was blocked or allowed");
  });
}

add_task(async function testTrackersSubView() {
  Services.prefs.setBoolPref(TP_PREF, false);
  await assertSitesListed(false);
  Services.prefs.setBoolPref(TP_PREF, true);
  await assertSitesListed(true);
  let uri = Services.io.newURI("https://tracking.example.org");
  Services.perms.add(uri, "trackingprotection", Services.perms.ALLOW_ACTION);
  await assertSitesListed(false);
  Services.perms.remove(uri, "trackingprotection");
  await assertSitesListed(true);
  Services.prefs.clearUserPref(TP_PREF);
});

add_task(function cleanup() {
  Services.prefs.clearUserPref(TP_PREF);
  UrlClassifierTestUtils.cleanupTestTrackers();
});
