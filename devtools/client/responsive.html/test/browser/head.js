/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from ../../../shared/test/shared-redux-head.js */
/* import-globals-from ../../../inspector/test/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this);
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-redux-head.js",
  this);

// Import helpers registering the test-actor in remote targets
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/test-actor-registry.js",
  this);

// Import helpers for the inspector that are also shared with others
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this);

const { _loadPreferredDevices } = require("devtools/client/responsive.html/actions/devices");
const { getStr } = require("devtools/client/responsive.html/utils/l10n");
const { getTopLevelWindow } = require("devtools/client/responsive.html/utils/window");
const { addDevice, removeDevice, removeLocalDevices } = require("devtools/client/shared/devices");
const { KeyCodes } = require("devtools/client/shared/keycodes");
const asyncStorage = require("devtools/shared/async-storage");

loader.lazyRequireGetter(this, "ResponsiveUIManager", "devtools/client/responsive.html/manager", true);

const E10S_MULTI_ENABLED = Services.prefs.getIntPref("dom.ipc.processCount") > 1;
const TEST_URI_ROOT = "http://example.com/browser/devtools/client/responsive.html/test/browser/";
const RELOAD_CONDITION_PREF_PREFIX = "devtools.responsive.reloadConditions.";
const DEFAULT_UA = Cc["@mozilla.org/network/protocol;1?name=http"]
  .getService(Ci.nsIHttpProtocolHandler)
  .userAgent;

SimpleTest.requestCompleteLog();
SimpleTest.waitForExplicitFinish();

// Toggling the RDM UI involves several docShell swap operations, which are somewhat slow
// on debug builds. Usually we are just barely over the limit, so a blanket factor of 2
// should be enough.
requestLongerTimeout(2);

Services.prefs.setCharPref("devtools.devices.url", TEST_URI_ROOT + "devices.json");
// The appearance of this notification causes intermittent behavior in some tests that
// send mouse events, since it causes the content to shift when it appears.
Services.prefs.setBoolPref("devtools.responsive.reloadNotification.enabled", false);
// Don't show the setting onboarding tooltip in the test suites.
Services.prefs.setBoolPref("devtools.responsive.show-setting-tooltip", false);
Services.prefs.setBoolPref("devtools.responsive.showUserAgentInput", true);

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("devtools.devices.url");
  Services.prefs.clearUserPref("devtools.responsive.reloadNotification.enabled");
  Services.prefs.clearUserPref("devtools.responsive.html.displayedDeviceList");
  Services.prefs.clearUserPref("devtools.responsive.reloadConditions.touchSimulation");
  Services.prefs.clearUserPref("devtools.responsive.reloadConditions.userAgent");
  Services.prefs.clearUserPref("devtools.responsive.show-setting-tooltip");
  Services.prefs.clearUserPref("devtools.responsive.showUserAgentInput");
  Services.prefs.clearUserPref("devtools.responsive.touchSimulation.enabled");
  Services.prefs.clearUserPref("devtools.responsive.userAgent");
  Services.prefs.clearUserPref("devtools.responsive.viewport.height");
  Services.prefs.clearUserPref("devtools.responsive.viewport.pixelRatio");
  Services.prefs.clearUserPref("devtools.responsive.viewport.width");
  await asyncStorage.removeItem("devtools.devices.url_cache");
  await asyncStorage.removeItem("devtools.responsive.deviceState");
  await removeLocalDevices();
});

/**
 * Open responsive design mode for the given tab.
 */
var openRDM = async function(tab) {
  info("Opening responsive design mode");
  const manager = ResponsiveUIManager;
  const ui = await manager.openIfNeeded(tab.ownerGlobal, tab, { trigger: "test" });
  info("Responsive design mode opened");
  return { ui, manager };
};

/**
 * Close responsive design mode for the given tab.
 */
var closeRDM = async function(tab, options) {
  info("Closing responsive design mode");
  const manager = ResponsiveUIManager;
  await manager.closeIfNeeded(tab.ownerGlobal, tab, options);
  info("Responsive design mode closed");
};

/**
 * Adds a new test task that adds a tab with the given URL, opens responsive
 * design mode, runs the given generator, closes responsive design mode, and
 * removes the tab.
 *
 * Example usage:
 *
 *   addRDMTask(TEST_URL, async function ({ ui, manager }) {
 *     // Your tests go here...
 *   });
 */
function addRDMTask(url, task) {
  add_task(async function() {
    const tab = await addTab(url);
    const results = await openRDM(tab);

    try {
      await task(results);
    } catch (err) {
      ok(false, "Got an error: " + DevToolsUtils.safeErrorString(err));
    }

    await closeRDM(tab);
    await removeTab(tab);
  });
}

function spawnViewportTask(ui, args, task) {
  return ContentTask.spawn(ui.getViewportBrowser(), args, task);
}

function waitForFrameLoad(ui, targetURL) {
  return spawnViewportTask(ui, { targetURL }, async function(args) {
    if ((content.document.readyState == "complete" ||
         content.document.readyState == "interactive") &&
        content.location.href == args.targetURL) {
      return;
    }
    await ContentTaskUtils.waitForEvent(this, "DOMContentLoaded");
  });
}

function waitForViewportResizeTo(ui, width, height) {
  return new Promise(async function(resolve) {
    const isSizeMatching = data => data.width == width && data.height == height;

    // If the viewport has already the expected size, we resolve the promise immediately.
    const size = ui.getViewportSize();
    if (isSizeMatching(size)) {
      info(`Viewport already resized to ${width} x ${height}`);
      resolve();
      return;
    }

    // Otherwise, we'll listen to the content's resize event, the viewport's resize event,
    // and the browser's load end; since a racing condition can happen, where the
    // content's listener is added after the resize, because the content's document was
    // reloaded; therefore the test would hang forever. See bug 1302879.
    const browser = ui.getViewportBrowser();

    const onResize = data => {
      if (!isSizeMatching(data)) {
        return;
      }
      ui.off("viewport-resize", onResize);
      ui.off("content-resize", onResize);
      browser.removeEventListener("mozbrowserloadend", onBrowserLoadEnd);
      info(`Got content-resize or viewport-resize to ${width} x ${height}`);
      resolve();
    };

    const onBrowserLoadEnd = async function() {
      const data = ui.getViewportSize(ui);
      onResize(data);
    };

    info(`Waiting for content-resize or viewport-resize to ${width} x ${height}`);
    // Depending on whether or not the viewport is overridden, we'll either get a
    // viewport-resize event or a content-resize event.
    ui.on("viewport-resize", onResize);
    ui.on("content-resize", onResize);
    browser.addEventListener("mozbrowserloadend",
      onBrowserLoadEnd, { once: true });
  });
}

var setViewportSize = async function(ui, manager, width, height) {
  const size = ui.getViewportSize();
  info(`Current size: ${size.width} x ${size.height}, ` +
       `set to: ${width} x ${height}`);
  if (size.width != width || size.height != height) {
    const resized = waitForViewportResizeTo(ui, width, height);
    ui.setViewportSize({ width, height });
    await resized;
  }
};

function getViewportDevicePixelRatio(ui) {
  return ContentTask.spawn(ui.getViewportBrowser(), {}, async function() {
    return content.devicePixelRatio;
  });
}

function getElRect(selector, win) {
  const el = win.document.querySelector(selector);
  return el.getBoundingClientRect();
}

/**
 * Drag an element identified by 'selector' by [x,y] amount. Returns
 * the rect of the dragged element as it was before drag.
 */
function dragElementBy(selector, x, y, win) {
  const { Simulate } = win.require("devtools/client/shared/vendor/react-dom-test-utils");
  const rect = getElRect(selector, win);
  const startPoint = {
    clientX: Math.floor(rect.left + rect.width / 2),
    clientY: Math.floor(rect.top + rect.height / 2),
  };
  const endPoint = [ startPoint.clientX + x, startPoint.clientY + y ];

  const elem = win.document.querySelector(selector);

  // mousedown is a React listener, need to use its testing tools to avoid races
  Simulate.mouseDown(elem, startPoint);

  // mousemove and mouseup are regular DOM listeners
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mousemove" }, win);
  EventUtils.synthesizeMouseAtPoint(...endPoint, { type: "mouseup" }, win);

  return rect;
}

async function testViewportResize(ui, selector, moveBy,
                             expectedViewportSize, expectedHandleMove) {
  const win = ui.toolWindow;
  const resized = waitForViewportResizeTo(ui, ...expectedViewportSize);
  const startRect = dragElementBy(selector, ...moveBy, win);
  await resized;

  const endRect = getElRect(selector, win);
  is(endRect.left - startRect.left, expectedHandleMove[0],
    `The x move of ${selector} is as expected`);
  is(endRect.top - startRect.top, expectedHandleMove[1],
    `The y move of ${selector} is as expected`);
}

async function openDeviceModal(ui) {
  const { document, store } = ui.toolWindow;

  info("Opening device modal through device selector.");
  const onModalOpen = waitUntilState(store, state => state.devices.isModalOpen);
  await selectMenuItem(ui, "#device-selector", getStr("responsive.editDeviceList2"));
  await onModalOpen;

  const modal = document.getElementById("device-modal-wrapper");
  ok(modal.classList.contains("opened") && !modal.classList.contains("closed"),
    "The device modal is displayed.");
}

async function selectMenuItem({ toolWindow }, selector, value) {
  const { document } = toolWindow;

  const button = document.querySelector(selector);
  isnot(button, null, `Selector "${selector}" should match an existing element.`);

  info(`Selecting ${value} in ${selector}.`);

  await testMenuItems(toolWindow, button, items => {
    const menuItem = items.find(item => item.getAttribute("label") === value);
    isnot(menuItem, undefined, `Value "${value}" should match an existing menu item.`);
    menuItem.click();
  });
}

/**
 * Runs the menu items from the button's context menu against a test function.
 *
 * @param  {Window} toolWindow
 *         A window reference.
 * @param  {Element} button
 *         The button that will show a context menu when clicked.
 * @param  {Function} testFn
 *         A test function that will be ran with the found menu item in the context menu
 *         as an argument.
 */
function testMenuItems(toolWindow, button, testFn) {
  // The context menu appears only in the top level window, which is different from
  // the inner toolWindow.
  const win = getTopLevelWindow(toolWindow);

  return new Promise(resolve => {
    win.document.addEventListener("popupshown", () => {
      const popup = win.document.querySelector("menupopup[menu-api=\"true\"]");
      const menuItems = [...popup.children];

      testFn(menuItems);

      popup.hidePopup();
      resolve();
    }, { once: true });

    button.click();
  });
}

const selectDevice = (ui, value) => Promise.all([
  once(ui, "device-changed"),
  selectMenuItem(ui, "#device-selector", value),
]);

const selectDevicePixelRatio = (ui, value) =>
  selectMenuItem(ui, "#device-pixel-ratio-menu", `DPR: ${value}`);

const selectNetworkThrottling = (ui, value) => Promise.all([
  once(ui, "network-throttling-changed"),
  selectMenuItem(ui, "#network-throttling-menu", value),
]);

function getSessionHistory(browser) {
  return ContentTask.spawn(browser, {}, async function() {
    /* eslint-disable no-undef */
    const { SessionHistory } =
      ChromeUtils.import("resource://gre/modules/sessionstore/SessionHistory.jsm", {});
    return SessionHistory.collect(docShell);
    /* eslint-enable no-undef */
  });
}

function getContentSize(ui) {
  return spawnViewportTask(ui, {}, () => ({
    width: content.screen.width,
    height: content.screen.height,
  }));
}

async function waitForPageShow(browser) {
  const tab = gBrowser.getTabForBrowser(browser);
  const ui = ResponsiveUIManager.getResponsiveUIForTab(tab);
  if (ui) {
    browser = ui.getViewportBrowser();
  }
  info("Waiting for pageshow from " + (ui ? "responsive" : "regular") + " browser");
  // Need to wait an extra tick after pageshow to ensure everyone is up-to-date,
  // hence the waitForTick.
  await BrowserTestUtils.waitForContentEvent(browser, "pageshow");
  return waitForTick();
}

function waitForViewportLoad(ui) {
  return BrowserTestUtils.waitForContentEvent(ui.getViewportBrowser(), "load", true);
}

function load(browser, url) {
  const loaded = BrowserTestUtils.browserLoaded(browser, false, url);
  BrowserTestUtils.loadURI(browser, url);
  return loaded;
}

function back(browser) {
  const shown = waitForPageShow(browser);
  browser.goBack();
  return shown;
}

function forward(browser) {
  const shown = waitForPageShow(browser);
  browser.goForward();
  return shown;
}

function addDeviceForTest(device) {
  info(`Adding Test Device "${device.name}" to the list.`);
  addDevice(device);

  registerCleanupFunction(() => {
    // Note that assertions in cleanup functions are not displayed unless they failed.
    ok(removeDevice(device), `Removed Test Device "${device.name}" from the list.`);
  });
}

async function waitForClientClose(ui) {
  info("Waiting for RDM debugger client to close");
  await ui.client.addOneTimeListener("closed");
  info("RDM's debugger client is now closed");
}

async function testDevicePixelRatio(ui, expected) {
  const dppx = await getViewportDevicePixelRatio(ui);
  is(dppx, expected, `devicePixelRatio should be set to ${expected}`);
}

async function testTouchEventsOverride(ui, expected) {
  const { document } = ui.toolWindow;
  const touchButton = document.getElementById("touch-simulation-button");

  const flag = await ui.emulationFront.getTouchEventsOverride();
  is(flag === Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_ENABLED, expected,
    `Touch events override should be ${expected ? "enabled" : "disabled"}`);
  is(touchButton.classList.contains("checked"), expected,
    `Touch simulation button should be ${expected ? "" : "in"}active.`);
}

function testViewportDeviceMenuLabel(ui, expected) {
  info("Test viewport's device select label");

  const label = ui.toolWindow.document.querySelector("#device-selector .title");
  is(label.textContent, expected, `Device Select value should be: ${expected}`);
}

async function toggleTouchSimulation(ui) {
  const { document } = ui.toolWindow;
  const touchButton = document.getElementById("touch-simulation-button");
  const changed = once(ui, "touch-simulation-changed");
  const loaded = waitForViewportLoad(ui);
  touchButton.click();
  await Promise.all([ changed, loaded ]);
}

async function testUserAgent(ui, expected) {
  const { document } = ui.toolWindow;
  const userAgentInput = document.getElementById("user-agent-input");

  if (expected === DEFAULT_UA) {
    is(userAgentInput.value, "", "UA input should be empty");
  } else {
    is(userAgentInput.value, expected, `UA input should be set to ${expected}`);
  }

  await testUserAgentFromBrowser(ui.getViewportBrowser(), expected);
}

async function testUserAgentFromBrowser(browser, expected) {
  const ua = await ContentTask.spawn(browser, {}, async function() {
    return content.navigator.userAgent;
  });
  is(ua, expected, `UA should be set to ${expected}`);
}

function testViewportDimensions(ui, w, h) {
  const viewport = ui.toolWindow.document.querySelector(".viewport-content");

  is(ui.toolWindow.getComputedStyle(viewport).getPropertyValue("width"),
     `${w}px`, `Viewport should have width of ${w}px`);
  is(ui.toolWindow.getComputedStyle(viewport).getPropertyValue("height"),
     `${h}px`, `Viewport should have height of ${h}px`);
}

async function changeUserAgentInput(ui, value) {
  const { Simulate } =
    ui.toolWindow.require("devtools/client/shared/vendor/react-dom-test-utils");
  const { document, store } = ui.toolWindow;

  const userAgentInput = document.getElementById("user-agent-input");
  userAgentInput.value = value;
  Simulate.change(userAgentInput);

  const userAgentChanged = waitUntilState(store, state => state.ui.userAgent === value);
  const changed = once(ui, "user-agent-changed");
  const loaded = waitForViewportLoad(ui);
  Simulate.keyUp(userAgentInput, { keyCode: KeyCodes.DOM_VK_RETURN });
  await Promise.all([ changed, loaded, userAgentChanged ]);
}

/**
 * Assuming the device modal is open and the device adder form is shown, this helper
 * function adds `device` via the form, saves it, and waits for it to appear in the store.
 */
function addDeviceInModal(ui, device) {
  const { Simulate } =
    ui.toolWindow.require("devtools/client/shared/vendor/react-dom-test-utils");
  const { document, store } = ui.toolWindow;

  const nameInput = document.querySelector("#device-adder-name input");
  const [ widthInput, heightInput ] =
    document.querySelectorAll("#device-adder-size input");
  const pixelRatioInput = document.querySelector("#device-adder-pixel-ratio input");
  const userAgentInput = document.querySelector("#device-adder-user-agent input");
  const touchInput = document.querySelector("#device-adder-touch input");

  nameInput.value = device.name;
  Simulate.change(nameInput);
  widthInput.value = device.width;
  Simulate.change(widthInput);
  Simulate.blur(widthInput);
  heightInput.value = device.height;
  Simulate.change(heightInput);
  Simulate.blur(heightInput);
  pixelRatioInput.value = device.pixelRatio;
  Simulate.change(pixelRatioInput);
  userAgentInput.value = device.userAgent;
  Simulate.change(userAgentInput);
  touchInput.checked = device.touch;
  Simulate.change(touchInput);

  const existingCustomDevices = store.getState().devices.custom.length;
  const adderSave = document.querySelector("#device-adder-save");
  const saved = waitUntilState(store, state =>
    state.devices.custom.length == existingCustomDevices + 1
  );
  Simulate.click(adderSave);
  return saved;
}

function reloadOnUAChange(enabled) {
  const pref = RELOAD_CONDITION_PREF_PREFIX + "userAgent";
  Services.prefs.setBoolPref(pref, enabled);
}

function reloadOnTouchChange(enabled) {
  const pref = RELOAD_CONDITION_PREF_PREFIX + "touchSimulation";
  Services.prefs.setBoolPref(pref, enabled);
}

function rotateViewport(ui) {
  const { document } = ui.toolWindow;
  const rotateButton = document.getElementById("rotate-button");
  rotateButton.click();
}
