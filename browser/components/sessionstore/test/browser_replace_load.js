"use strict";

const STATE = {
  entries: [{url: "about:robots"}, {url: "about:mozilla"}],
  selected: 2
};

/**
 * Bug 1100223. Calling browser.loadURI() while a tab is loading causes
 * sessionstore to override the desired target URL. This test ensures that
 * calling loadURI() on a pending tab causes the tab to no longer be marked
 * as pending and correctly finish the instructed load while keeping the
 * restored history around.
 */
add_task(function* () {
  yield testSwitchToTab("about:mozilla#fooobar", {ignoreFragment: true});
  yield testSwitchToTab("about:mozilla?foo=bar", {replaceQueryString: true});
});

let testSwitchToTab = Task.async(function* (url, options) {
  // Create a background tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // The tab shouldn't be restored right away.
  Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", true);

  // Prepare the tab state.
  let promise = promiseTabRestoring(tab);
  ss.setTabState(tab, JSON.stringify(STATE));
  ok(tab.hasAttribute("pending"), "tab is pending");
  yield promise;

  // Switch-to-tab with a similar URI.
  switchToTabHavingURI(url, false, options);
  ok(!tab.hasAttribute("pending"), "tab is no longer pending");

  // Wait until the tab is restored.
  yield promiseTabRestored(tab);
  is(browser.currentURI.spec, url, "correct URL loaded");

  // Check that we didn't lose any history entries.
  let count = yield ContentTask.spawn(browser, null, function* () {
    let Ci = Components.interfaces;
    let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    let history = webNavigation.sessionHistory.QueryInterface(Ci.nsISHistoryInternal);
    return history && history.count;
  });
  is(count, 3, "three history entries");

  // Cleanup.
  gBrowser.removeTab(tab);
});
