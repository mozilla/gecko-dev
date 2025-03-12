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

  ok(
    document.hasAttribute("fxadisabled"),
    "fxadisabled attribute should exist"
  );
}

// Given a window, check if hamburger menu
// buttons that aren't relevant to taskbar tabs
// are hidden
async function checkHamburgerMenu(win) {
  win.document.getElementById("PanelUI-menu-button").click();

  // Set up a MutationObserver to await for the hamburger menu
  // DOM element and CSS to be loaded & applied.
  // The observer itself verifies that the "new tab" button is hidden
  await new Promise(resolve => {
    const observer = new MutationObserver(() => {
      const newTabButton = win.document.querySelector(
        "#appMenu-new-tab-button2"
      );
      if (
        newTabButton &&
        win.getComputedStyle(newTabButton).display == "none"
      ) {
        observer.disconnect();
        resolve();
      }
    });

    observer.observe(win.document, { childList: true, subtree: true });
  });

  is(
    win.getComputedStyle(
      win.document.querySelector("#appMenu-new-window-button2")
    ).display,
    "none",
    "New window button in hamburger menu should not be visible"
  );

  is(
    win.getComputedStyle(
      win.document.querySelector("#appMenu-new-private-window-button2")
    ).display,
    "none",
    "New private window button in hamburger menu should not be visible"
  );

  is(
    win.getComputedStyle(
      win.document.querySelector("#appMenu-bookmarks-button")
    ).display,
    "none",
    "Bookmarks button in hamburger menu should not be visible"
  );
}

add_task(async function testWindowChrome() {
  let win = await openTaskbarTabWindow();

  checkWindowChrome(win);
  await checkHamburgerMenu(win);

  await BrowserTestUtils.closeWindow(win);
});
