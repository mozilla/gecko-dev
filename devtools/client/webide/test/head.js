/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const { FileUtils } = require("resource://gre/modules/FileUtils.jsm");
const { gDevTools } = require("devtools/client/framework/devtools");
const Services = require("Services");
const { AppProjects } = require("devtools/client/webide/modules/app-projects");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { DebuggerServer } = require("devtools/server/main");

var TEST_BASE;
if (window.location === AppConstants.BROWSER_CHROME_URL) {
  TEST_BASE = "chrome://mochitests/content/browser/devtools/client/webide/test/";
} else {
  TEST_BASE = "chrome://mochitests/content/chrome/devtools/client/webide/test/";
}

Services.prefs.setBoolPref("devtools.webide.enabled", true);
Services.prefs.setBoolPref("devtools.webide.enableLocalRuntime", true);

Services.prefs.setCharPref("devtools.remote.adb.extensionURL", TEST_BASE + "addons/adb-extension-#OS#.xpi");
Services.prefs.setCharPref("devtools.webide.templatesURL", TEST_BASE + "templates.json");
Services.prefs.setCharPref("devtools.devices.url", TEST_BASE + "browser_devices.json");

var registerCleanupFunction = registerCleanupFunction ||
                              SimpleTest.registerCleanupFunction;
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.webide.enabled");
  Services.prefs.clearUserPref("devtools.webide.enableLocalRuntime");
  Services.prefs.clearUserPref("devtools.webide.autoinstallADBExtension");
  Services.prefs.clearUserPref("devtools.webide.busyTimeout");
  Services.prefs.clearUserPref("devtools.webide.lastSelectedProject");
  Services.prefs.clearUserPref("devtools.webide.lastConnectedRuntime");
});

var openWebIDE = async function(autoInstallAddons) {
  info("opening WebIDE");

  Services.prefs.setBoolPref("devtools.webide.autoinstallADBExtension", !!autoInstallAddons);

  const win = Services.ww.openWindow(null, "chrome://webide/content/", "webide",
                                   "chrome,centerscreen,resizable", null);

  await new Promise(resolve => {
    win.addEventListener("load", function() {
      SimpleTest.requestCompleteLog();
      SimpleTest.executeSoon(resolve);
    }, {once: true});
  });

  info("WebIDE open");

  return win;
};

function closeWebIDE(win) {
  info("Closing WebIDE");

  return new Promise(resolve => {
    win.addEventListener("unload", function() {
      info("WebIDE closed");
      SimpleTest.executeSoon(resolve);
    }, {once: true});

    win.close();
  });
}

function removeAllProjects() {
  return (async function() {
    await AppProjects.load();
    // use a new array so we're not iterating over the same
    // underlying array that's being modified by AppProjects
    const projects = AppProjects.projects.map(p => p.location);
    for (let i = 0; i < projects.length; i++) {
      await AppProjects.remove(projects[i]);
    }
  })();
}

function nextTick() {
  return new Promise(resolve => {
    SimpleTest.executeSoon(resolve);
  });
}

function waitForUpdate(win, update) {
  info("Wait: " + update);
  return new Promise(resolve => {
    win.AppManager.on("app-manager-update", function onUpdate(what) {
      info("Got: " + what);
      if (what !== update) {
        return;
      }
      win.AppManager.off("app-manager-update", onUpdate);
      resolve(win.UI._updatePromise);
    });
  });
}

function waitForTime(time) {
  return new Promise(resolve => {
    setTimeout(resolve, time);
  });
}

function documentIsLoaded(doc) {
  return new Promise(resolve => {
    if (doc.readyState == "complete") {
      resolve();
    } else {
      doc.addEventListener("readystatechange", function onChange() {
        if (doc.readyState == "complete") {
          doc.removeEventListener("readystatechange", onChange);
          resolve();
        }
      });
    }
  });
}

function lazyIframeIsLoaded(iframe) {
  return new Promise(resolve => {
    iframe.addEventListener("load", function() {
      resolve(nextTick());
    }, {capture: true, once: true});
  });
}

function addTab(aUrl, aWindow) {
  info("Adding tab: " + aUrl);

  return new Promise(resolve => {
    const targetWindow = aWindow || window;
    const targetBrowser = targetWindow.gBrowser;

    targetWindow.focus();
    const tab = targetBrowser.selectedTab = targetBrowser.addTab(aUrl);
    const linkedBrowser = tab.linkedBrowser;

    BrowserTestUtils.browserLoaded(linkedBrowser).then(function() {
      info("Tab added and finished loading: " + aUrl);
      resolve(tab);
    });
  });
}

function removeTab(aTab, aWindow) {
  info("Removing tab.");

  return new Promise(resolve => {
    const targetWindow = aWindow || window;
    const targetBrowser = targetWindow.gBrowser;
    const tabContainer = targetBrowser.tabContainer;

    tabContainer.addEventListener("TabClose", function(aEvent) {
      info("Tab removed and finished closing.");
      resolve();
    }, {once: true});

    targetBrowser.removeTab(aTab);
  });
}

function getRuntimeDocument(win) {
  return win.document.querySelector("#runtime-listing-panel-details").contentDocument;
}

function getProjectDocument(win) {
  return win.document.querySelector("#project-listing-panel-details").contentDocument;
}

function getRuntimeWindow(win) {
  return win.document.querySelector("#runtime-listing-panel-details").contentWindow;
}

function getProjectWindow(win) {
  return win.document.querySelector("#project-listing-panel-details").contentWindow;
}

function getAddonsDocument(win) {
  return win.document.querySelector("#deck-panel-addons").contentDocument;
}

function connectToLocalRuntime(win) {
  info("Loading local runtime.");

  const runtimePanel = getRuntimeDocument(win);

  const panelNode = runtimePanel.querySelector("#runtime-panel");
  const items = panelNode.querySelectorAll(".runtime-panel-item-other");
  is(items.length, 2, "Found 2 custom runtime buttons");

  const updated = waitForUpdate(win, "runtime-global-actors");
  items[1].click();
  return updated;
}

function handleError(aError) {
  ok(false, "Got an error: " + aError.message + "\n" + aError.stack);
  finish();
}

function waitForConnectionChange(expectedState, count = 1) {
  return new Promise(resolve => {
    const onConnectionChange = state => {
      if (state != expectedState) {
        return;
      }
      if (--count != 0) {
        return;
      }
      DebuggerServer.off("connectionchange", onConnectionChange);
      resolve();
    };
    DebuggerServer.on("connectionchange", onConnectionChange);
  });
}
