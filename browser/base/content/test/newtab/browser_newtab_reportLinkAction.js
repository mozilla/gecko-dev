/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PRELOAD_PREF = "browser.newtab.preload";

gDirectorySource = "data:application/json," + JSON.stringify({
  "directory": [{
    url: "http://example.com/organic",
    type: "organic"
  }, {
    url: "http://localhost/sponsored",
    type: "sponsored"
  }]
});

function runTests() {
  Services.prefs.setBoolPref(PRELOAD_PREF, false);

  let originalReportSitesAction  = DirectoryLinksProvider.reportSitesAction;
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PRELOAD_PREF);
    DirectoryLinksProvider.reportSitesAction = originalReportSitesAction;
  });

  let expected = {};
  DirectoryLinksProvider.reportSitesAction = function(sites, action, siteIndex) {
    let {link} = sites[siteIndex];
    is(link.type, expected.type, "got expected type");
    is(action, expected.action, "got expected action");
    is(NewTabUtils.pinnedLinks.isPinned(link), expected.pinned, "got expected pinned");
    executeSoon(TestRunner.next);
  }

  // Test that the last visible site (index 1) is reported
  expected.type = "sponsored";
  expected.action = "view";
  expected.pinned = false;
  addNewTabPageTab();

  // Wait for addNewTabPageTab and reportSitesAction
  yield null;
  yield null;

  whenPagesUpdated();
  // Click the pin button on the link in the 1th tile spot
  let siteNode = getCell(1).node.querySelector(".newtab-site");
  let pinButton = siteNode.querySelector(".newtab-control-pin");
  expected.action = "pin";
  // tiles become "history" when pinned
  expected.type = "history";
  expected.pinned = true;
  EventUtils.synthesizeMouseAtCenter(pinButton, {}, getContentWindow());

  // Wait for whenPagesUpdated and reportSitesAction
  yield null;
  yield null;

  // Unpin that link
  expected.action = "unpin";
  expected.pinned = false;
  whenPagesUpdated();
  // need to reget siteNode for it could have been re-rendered after pin
  siteNode = getCell(1).node.querySelector(".newtab-site");
  pinButton = siteNode.querySelector(".newtab-control-pin");
  EventUtils.synthesizeMouseAtCenter(pinButton, {}, getContentWindow());

  // Wait for whenPagesUpdated and reportSitesAction
  yield null;
  yield null;

  // Block the site in the 0th tile spot
  let blockedSite = getCell(0).node.querySelector(".newtab-site");
  let blockButton = blockedSite.querySelector(".newtab-control-block");
  expected.type = "organic";
  expected.action = "block";
  expected.pinned = false;
  whenPagesUpdated();
  EventUtils.synthesizeMouseAtCenter(blockButton, {}, getContentWindow());

  // Wait for whenPagesUpdated and reportSitesAction
  yield null;
  yield null;

  // Click the 1th link now in the 0th tile spot
  expected.type = "history";
  expected.action = "click";
  EventUtils.synthesizeMouseAtCenter(siteNode, {}, getContentWindow());

  // Wait for reportSitesAction
  yield null;
}
