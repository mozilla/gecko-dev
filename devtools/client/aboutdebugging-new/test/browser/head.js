/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env browser */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from debug-target-pane_collapsibilities_head.js */

"use strict";

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);

// Load the shared Redux helpers into this compartment.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this);

// Load collapsibilities helpers
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "debug-target-pane_collapsibilities_head.js", this);

// Make sure the ADB addon is removed and ADB is stopped when the test ends.
registerCleanupFunction(async function() {
  try {
    const { adbAddon } = require("devtools/shared/adb/adb-addon");
    await adbAddon.uninstall();
  } catch (e) {
    // Will throw if the addon is already uninstalled, ignore exceptions here.
  }
  const { ADB } = require("devtools/shared/adb/adb");
  await ADB.kill();
});

/**
 * Enable the new about:debugging panel.
 */
async function enableNewAboutDebugging() {
  await pushPref("devtools.aboutdebugging.new-enabled", true);
  await pushPref("devtools.aboutdebugging.network", true);
}

async function openAboutDebugging(page, win) {
  await enableNewAboutDebugging();

  info("opening about:debugging");
  const tab = await addTab("about:debugging", { window: win });
  const browser = tab.linkedBrowser;
  const document = browser.contentDocument;
  const window = browser.contentWindow;
  const { AboutDebugging } = window;

  await Promise.all([
    waitForDispatch(AboutDebugging.store, "REQUEST_EXTENSIONS_SUCCESS"),
    waitForDispatch(AboutDebugging.store, "REQUEST_TABS_SUCCESS"),
    waitForDispatch(AboutDebugging.store, "REQUEST_WORKERS_SUCCESS"),
  ]);

  return { tab, document, window };
}

/**
 * Wait for all client requests to settle, meaning here that no new request has been
 * dispatched after the provided delay.
 */
async function waitForRequestsToSettle(store, delay = 500) {
  let hasSettled = false;

  // After each iteration of this while loop, we check is the timerPromise had the time
  // to resolve or if we captured a REQUEST_*_SUCCESS action before.
  while (!hasSettled) {
    let timer;

    // This timer will be executed only if no REQUEST_*_SUCCESS action is dispatched
    // during the delay. We consider that when no request are received for some time, it
    // means there are no ongoing requests anymore.
    const timerPromise = new Promise(resolve => {
      timer = setTimeout(() => {
        hasSettled = true;
        resolve();
      }, delay);
    });

    // Wait either for a REQUEST_*_SUCCESS to be dispatched, or for the timer to resolve.
    await Promise.race([
      waitForDispatch(store, "REQUEST_EXTENSIONS_SUCCESS"),
      waitForDispatch(store, "REQUEST_TABS_SUCCESS"),
      waitForDispatch(store, "REQUEST_WORKERS_SUCCESS"),
      timerPromise,
    ]);

    // Clear the timer to avoid setting hasSettled to true accidently unless timerPromise
    // was the first to resolve.
    clearTimeout(timer);
  }
}

function waitForDispatch(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => action.type === type,
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}

/**
 * Navigate to the Connect page. Resolves when the Connect page is rendered.
 */
async function selectConnectPage(doc) {
  const sidebarItems = doc.querySelectorAll(".js-sidebar-item");
  const connectSidebarItem = [...sidebarItems].find(element => {
    return element.textContent === "Connect";
  });
  ok(connectSidebarItem, "Sidebar contains a Connect item");
  const connectLink = connectSidebarItem.querySelector(".js-sidebar-link");
  ok(connectLink, "Sidebar contains a Connect link");

  info("Click on the Connect link in the sidebar");
  connectLink.click();

  info("Wait until Connect page is displayed");
  await waitUntil(() => doc.querySelector(".js-connect-page"));
}

function findDebugTargetByText(text, document) {
  const targets = [...document.querySelectorAll(".js-debug-target-item")];
  return targets.find(target => target.textContent.includes(text));
}

function findSidebarItemByText(text, document) {
  const sidebarItems = document.querySelectorAll(".js-sidebar-item");
  return [...sidebarItems].find(element => {
    return element.textContent.includes(text);
  });
}

async function connectToRuntime(deviceName, appName, document) {
  info(`Wait until the sidebar item for ${deviceName} appears`);
  await waitUntil(() => findSidebarItemByText(deviceName, document));
  const sidebarItem = findSidebarItemByText(deviceName, document);
  const connectButton = sidebarItem.querySelector(".js-connect-button");
  ok(connectButton, `Connect button is displayed for the runtime ${deviceName}`);

  info("Click on the connect button and wait until it disappears");
  connectButton.click();
  await waitUntil(() => !sidebarItem.querySelector(".js-connect-button"));

  sidebarItem.querySelector(".js-sidebar-link").click();

  await waitUntil(() => {
    const runtimeInfo = document.querySelector(".js-runtime-info");
    return runtimeInfo.textContent.includes(appName);
  });
}
