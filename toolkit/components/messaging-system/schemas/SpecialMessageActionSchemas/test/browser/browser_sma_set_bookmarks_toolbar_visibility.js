/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BOOKMARKS_TOOLBAR_VISIBILITY_PREF =
  "browser.toolbars.bookmarks.visibility";

add_setup(async function () {
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref(BOOKMARKS_TOOLBAR_VISIBILITY_PREF);
  });
});

async function setAndCheckBookmarksToolbarVisibility(
  visibility,
  expectedToolbarCollapsed,
  url = "about:blank"
) {
  const action = {
    type: "SET_BOOKMARKS_TOOLBAR_VISIBILITY",
    data: {
      visibility,
    },
  };
  await SMATestUtils.validateAction(action);
  await SpecialMessageActions.handleAction(action, gBrowser);

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: url,
    waitForLoad: false,
  });

  Assert.equal(
    window.document.getElementById("PersonalToolbar").collapsed,
    expectedToolbarCollapsed,
    `The bookmarks toolbar should be ${
      expectedToolbarCollapsed ? "collapsed" : "visible"
    } when visibility is set to ${
      visibility === "newtab" ? `${visibility} when on '${url}'` : visibility
    }
    }'`
  );

  Assert.equal(
    Services.prefs.getStringPref(BOOKMARKS_TOOLBAR_VISIBILITY_PREF, ""),
    visibility,
    `'browser.toolbars.bookmarks.visibility' pref should be set to '${visibility}'`
  );

  BrowserTestUtils.removeTab(tab);
}

add_task(async function test_BOOKMARKS_TOOLBAR_ALWAYS_VISIBILITY() {
  await setAndCheckBookmarksToolbarVisibility("always", false);
});

add_task(async function test_BOOKMARKS_TOOLBAR_NEVER_VISIBILITY() {
  await setAndCheckBookmarksToolbarVisibility("never", true);
});

add_task(async function test_BOOKMARKS_TOOLBAR_NEWTAB_VISIBILITY_NON_NEWTAB() {
  await setAndCheckBookmarksToolbarVisibility("newtab", true);
});

add_task(async function test_BOOKMARKS_TOOLBAR_NEWTAB_VISIBILITY_ON_NEWTAB() {
  await setAndCheckBookmarksToolbarVisibility("newtab", false, "about:newtab");
});
