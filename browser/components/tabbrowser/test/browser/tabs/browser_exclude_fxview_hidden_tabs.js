/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

async function openAllTabsPanel(win) {
  const button = win.document.getElementById("alltabs-button");
  const allTabsView = win.document.getElementById("allTabsMenu-allTabsView");
  const allTabsPopupShownPromise = BrowserTestUtils.waitForEvent(
    allTabsView,
    "ViewShown"
  );

  button.click();
  return allTabsPopupShownPromise;
}

async function openHiddenTabsPanel(win) {
  const hiddenTabsButton = win.document.getElementById(
    "allTabsMenu-hiddenTabsButton"
  );

  const hiddenTabsView = win.document.getElementById(
    "allTabsMenu-hiddenTabsView"
  );
  const hiddenTabsPopupShownPromise = BrowserTestUtils.waitForEvent(
    hiddenTabsView,
    "ViewShown"
  );

  hiddenTabsButton.click();
  return hiddenTabsPopupShownPromise;
}

add_task(async function () {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  win.gTabsPanel.init();

  await openAllTabsPanel(win);

  ok(
    win.document.getElementById("allTabsMenu-hiddenTabsButton").hidden,
    "Hidden tabs menu should not be displayed when there are no hidden tabs"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function () {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  win.gTabsPanel.init();

  // Add and hide firefox view.
  win.FirefoxViewHandler.openTab();

  await openAllTabsPanel(win);

  ok(
    win.document.getElementById("allTabsMenu-hiddenTabsButton").hidden,
    "Hidden tabs menu should not be displayed if just FxView is hidden"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function () {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  win.gTabsPanel.init();

  // Add and hide firefox view.
  win.FirefoxViewHandler.openTab();

  const otherTab = await addTabTo(win.gBrowser, "data:text/plain,otherTab");
  win.gBrowser.hideTab(otherTab);

  await openAllTabsPanel(win);

  ok(
    !win.document.getElementById("allTabsMenu-hiddenTabsButton").hidden,
    "Hidden tabs menu should be displayed with other hidden tabs and when FxView is also hidden"
  );

  await openHiddenTabsPanel(win);

  const hiddenTabs = win.document.querySelectorAll(
    "#allTabsMenu-hiddenTabsView-tabs toolbaritem.all-tabs-item"
  );
  Assert.equal(
    hiddenTabs.length,
    1,
    "There should be only 1 tab item as Fx view is filtered out"
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function () {
  const win = await BrowserTestUtils.openNewBrowserWindow();
  win.gTabsPanel.init();

  const otherTab = await addTabTo(win.gBrowser, "data:text/plain,otherTab");
  win.gBrowser.hideTab(otherTab);

  await openAllTabsPanel(win);

  ok(
    !win.document.getElementById("allTabsMenu-hiddenTabsButton").hidden,
    "Hidden tabs menu should be displayed with a single hidden tab"
  );

  await openHiddenTabsPanel(win);

  const hiddenTabs = win.document.querySelectorAll(
    "#allTabsMenu-hiddenTabsView-tabs toolbaritem.all-tabs-item"
  );
  Assert.equal(hiddenTabs.length, 1, "There should be only 1 hidden tab.");

  await BrowserTestUtils.closeWindow(win);
});
