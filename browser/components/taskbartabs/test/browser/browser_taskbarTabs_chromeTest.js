/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
});

// Given a window, check if it meets all requirements
// of the taskbar tab chrome UI
function checkWindowChrome(win) {
  let document = win.document.documentElement;

  ok(
    document.hasAttribute("taskbartab"),
    "The window HTML should have a taskbartab attribute"
  );

  ok(win.gURLBar.readOnly, "The URL bar should be read-only");

  ok(
    win.document.getElementById("TabsToolbar").collapsed,
    "The tab bar should be collapsed"
  );

  is(
    document.getAttribute("chromehidden"),
    "menubar directories extrachrome ",
    "The correct chrome hidden attributes should be populated"
  );

  ok(!win.menubar.visible, "menubar barprop should not be visible");
  ok(!win.personalbar.visible, "personalbar barprop should not be visible");

  is(
    document.getAttribute("pocketdisabled"),
    "true",
    "Pocket button should be disabled"
  );

  let starButton = win.document.querySelector("#star-button-box");
  is(
    win.getComputedStyle(starButton).display,
    "none",
    "Bookmark button should not be visible"
  );

  ok(
    !document.hasAttribute("fxatoolbarmenu"),
    "Firefox accounts menu should not be displayed"
  );
}

// Given a window, check if hamburger menu
// buttons that aren't relevant to taskbar tabs
// are hidden
function checkHamburgerMenu(win) {
  let document = win.document.documentElement;

  is(
    document.getAttribute("fxadisabled"),
    "true",
    "fxadisabled attribute should be true"
  );
}

add_task(async function testWindowChrome() {
  let extraOptions = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
    Ci.nsIWritablePropertyBag2
  );
  extraOptions.setPropertyAsBool("taskbartab", true);

  let args = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  args.appendElement(null);
  args.appendElement(extraOptions);
  args.appendElement(null);

  // Simulate opening a taskbar tab window
  let win = Services.ww.openWindow(
    null,
    AppConstants.BROWSER_CHROME_URL,
    "_blank",
    "chrome,dialog=no,titlebar,close,toolbar,location,personalbar=no,status,menubar=no,resizable,minimizable,scrollbars",
    args
  );
  await new Promise(resolve => {
    win.addEventListener("load", resolve, { once: true });
  });
  await win.delayedStartupPromise;

  checkWindowChrome(win);
  checkHamburgerMenu(win);

  await BrowserTestUtils.closeWindow(win);
});
