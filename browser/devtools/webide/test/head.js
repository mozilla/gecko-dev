/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
const {require} = devtools;
const promise = require("promise");
const {AppProjects} = require("devtools/app-manager/app-projects");
gDevTools.testing = true;

let TEST_BASE;
if (window.location === "chrome://browser/content/browser.xul") {
  TEST_BASE = "chrome://mochitests/content/browser/browser/devtools/webide/test/";
} else {
  TEST_BASE = "chrome://mochitests/content/chrome/browser/devtools/webide/test/";
}

Services.prefs.setBoolPref("devtools.webide.enabled", true);
Services.prefs.setBoolPref("devtools.webide.enableLocalRuntime", true);
Services.prefs.setBoolPref("devtools.webide.enableRuntimeConfiguration", true);

Services.prefs.setCharPref("devtools.webide.addonsURL", TEST_BASE + "addons/simulators.json");
Services.prefs.setCharPref("devtools.webide.simulatorAddonsURL", TEST_BASE + "addons/fxos_#SLASHED_VERSION#_simulator-#OS#.xpi");
Services.prefs.setCharPref("devtools.webide.adbAddonURL", TEST_BASE + "addons/adbhelper-#OS#.xpi");
Services.prefs.setCharPref("devtools.webide.adaptersAddonURL", TEST_BASE + "addons/fxdt-adapters-#OS#.xpi");
Services.prefs.setCharPref("devtools.webide.templatesURL", TEST_BASE + "templates.json");
Services.prefs.setCharPref("devtools.devices.url", TEST_BASE + "browser_devices.json");

let registerCleanupFunction = registerCleanupFunction ||
                              SimpleTest.registerCleanupFunction;
registerCleanupFunction(() => {
  gDevTools.testing = false;
  Services.prefs.clearUserPref("devtools.webide.enabled");
  Services.prefs.clearUserPref("devtools.webide.enableLocalRuntime");
  Services.prefs.clearUserPref("devtools.webide.autoinstallADBHelper");
  Services.prefs.clearUserPref("devtools.webide.autoinstallFxdtAdapters");
  Services.prefs.clearUserPref("devtools.webide.sidebars");
  Services.prefs.clearUserPref("devtools.webide.busyTimeout");
  Services.prefs.clearUserPref("devtools.webide.lastSelectedProject");
  Services.prefs.clearUserPref("devtools.webide.lastConnectedRuntime");
});

function openWebIDE(autoInstallAddons) {
  info("opening WebIDE");

  Services.prefs.setBoolPref("devtools.webide.autoinstallADBHelper", !!autoInstallAddons);
  Services.prefs.setBoolPref("devtools.webide.autoinstallFxdtAdapters", !!autoInstallAddons);

  let deferred = promise.defer();

  let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].getService(Ci.nsIWindowWatcher);
  let win = ww.openWindow(null, "chrome://webide/content/", "webide", "chrome,centerscreen,resizable", null);

  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad);
    info("WebIDE open");
    SimpleTest.requestCompleteLog();
    SimpleTest.executeSoon(() => {
      deferred.resolve(win);
    });
  });

  return deferred.promise;
}

function closeWebIDE(win) {
  info("Closing WebIDE");

  let deferred = promise.defer();

  Services.prefs.clearUserPref("devtools.webide.widget.enabled");

  win.addEventListener("unload", function onUnload() {
    win.removeEventListener("unload", onUnload);
    info("WebIDE closed");
    SimpleTest.executeSoon(() => {
      deferred.resolve();
    });
  });

  win.close();

  return deferred.promise;
}

function removeAllProjects() {
  return Task.spawn(function* () {
    yield AppProjects.load();
    // use a new array so we're not iterating over the same
    // underlying array that's being modified by AppProjects
    let projects = AppProjects.store.object.projects.map(p => p.location);
    for (let i = 0; i < projects.length; i++) {
      yield AppProjects.remove(projects[i]);
    }
  });
}

function nextTick() {
  let deferred = promise.defer();
  SimpleTest.executeSoon(() => {
    deferred.resolve();
  });

  return deferred.promise;
}

function waitForUpdate(win, update) {
  info("Wait: " + update);
  let deferred = promise.defer();
  win.AppManager.on("app-manager-update", function onUpdate(e, what) {
    info("Got: " + what);
    if (what !== update) {
      return;
    }
    win.AppManager.off("app-manager-update", onUpdate);
    deferred.resolve(win.UI._updatePromise);
  });
  return deferred.promise;
}

function waitForTime(time) {
  let deferred = promise.defer();
  setTimeout(() => {
    deferred.resolve();
  }, time);
  return deferred.promise;
}

function documentIsLoaded(doc) {
  let deferred = promise.defer();
  if (doc.readyState == "complete") {
    deferred.resolve();
  } else {
    doc.addEventListener("readystatechange", function onChange() {
      if (doc.readyState == "complete") {
        doc.removeEventListener("readystatechange", onChange);
        deferred.resolve();
      }
    });
  }
  return deferred.promise;
}

function lazyIframeIsLoaded(iframe) {
  let deferred = promise.defer();
  iframe.addEventListener("load", function onLoad() {
    iframe.removeEventListener("load", onLoad, true);
    deferred.resolve(nextTick());
  }, true);
  return deferred.promise;
}

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  let deferred = promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;

  targetWindow.focus();
  let tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);
  let linkedBrowser = tab.linkedBrowser;

  linkedBrowser.addEventListener("load", function onLoad() {
    linkedBrowser.removeEventListener("load", onLoad, true);
    info("Tab added and finished loading: " + aUrl);
    deferred.resolve(tab);
  }, true);

  return deferred.promise;
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  let deferred = promise.defer();
  let targetWindow = aWindow || window;
  let targetBrowser = targetWindow.gBrowser;
  let tabContainer = targetBrowser.tabContainer;

  tabContainer.addEventListener("TabClose", function onClose(aEvent) {
    tabContainer.removeEventListener("TabClose", onClose, false);
    info("Tab removed and finished closing.");
    deferred.resolve();
  }, false);

  targetBrowser.removeTab(aTab);
  return deferred.promise;
}

function getRuntimeDocument(win) {
  return win.document.querySelector("#runtime-listing-panel-details").contentDocument;
}

function getProjectDocument(win) {
  return win.document.querySelector("#project-listing-panel-details").contentDocument;
}

function connectToLocalRuntime(aWindow) {
  info("Loading local runtime.");

  let panelNode;
  let runtimePanel;

  // If we are currently toggled to the sidebar panel display in WebIDE then
  // the runtime panel is in an iframe.
  if (aWindow.runtimeList.sidebarsEnabled) {
    runtimePanel = getRuntimeDocument(aWindow);
  } else {
    runtimePanel = aWindow.document;
  }

  panelNode = runtimePanel.querySelector("#runtime-panel");
  let items = panelNode.querySelectorAll(".runtime-panel-item-other");
  is(items.length, 2, "Found 2 custom runtime buttons");

  let updated = waitForUpdate(aWindow, "runtime-global-actors");
  items[1].click();
  return updated;
}

function handleError(aError) {
  ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  finish();
}
