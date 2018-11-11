/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test records which services, JS components and JS modules are loaded
 * when creating a new content process.
 *
 * If you made changes that cause this test to fail, it's likely because you
 * are loading more JS code during content process startup.
 *
 * If your code isn't strictly required to show a page, consider loading it
 * lazily. If you can't, consider delaying its load until after we have started
 * handling user events.
 */

"use strict";

/* Set this to true only for debugging purpose; it makes the output noisy. */
const kDumpAllStacks = false;

const whitelist = {
  components: new Set([
    "ContentProcessSingleton.js",
    "extension-process-script.js",
  ]),
  modules: new Set([
    "chrome://mochikit/content/ShutdownLeaksCollector.jsm",
    "resource://specialpowers/specialpowers.js",
    "resource://specialpowers/specialpowersAPI.js",

    // General utilities
    "resource://gre/modules/AppConstants.jsm",
    "resource://gre/modules/AsyncShutdown.jsm",
    "resource://gre/modules/DeferredTask.jsm",
    "resource://gre/modules/PromiseUtils.jsm",
    "resource://gre/modules/Services.jsm", // bug 1464542
    "resource://gre/modules/Timer.jsm",
    "resource://gre/modules/XPCOMUtils.jsm",

    // Logging related
    "resource://gre/modules/Log.jsm",

    // Session store
    "resource:///modules/sessionstore/ContentSessionStore.jsm",
    "resource://gre/modules/sessionstore/SessionHistory.jsm",

    // Forms and passwords
    "resource://formautofill/FormAutofill.jsm",
    "resource://formautofill/FormAutofillContent.jsm",

    // Browser front-end
    "resource:///actors/AboutReaderChild.jsm",
    "resource:///actors/BrowserTabChild.jsm",
    "resource:///modules/ContentMetaHandler.jsm",
    "resource:///actors/LinkHandlerChild.jsm",
    "resource:///actors/PageStyleChild.jsm",
    "resource://gre/modules/ActorChild.jsm",
    "resource://gre/modules/ActorManagerChild.jsm",
    "resource://gre/modules/E10SUtils.jsm",
    "resource://gre/modules/Readerable.jsm",
    "resource://gre/modules/WebProgressChild.jsm",

    // Pocket
    "chrome://pocket/content/AboutPocket.jsm",

    // Telemetry
    "resource://gre/modules/TelemetryController.jsm", // bug 1470339
    "resource://gre/modules/MemoryTelemetry.jsm", // bug 1481812
    "resource://gre/modules/TelemetryUtils.jsm", // bug 1470339

    // Extensions
    "resource://gre/modules/ExtensionUtils.jsm",
    "resource://gre/modules/MessageChannel.jsm",
  ]),
};

// Items on this list are allowed to be loaded but not
// required, as opposed to items in the main whitelist,
// which are all required.
const intermittently_loaded_whitelist = {
  components: new Set([
    "nsAsyncShutdown.js",
  ]),
  modules: new Set([
    "resource://gre/modules/sessionstore/Utils.jsm",
  ]),
};

const blacklist = {
  services: new Set([
    "@mozilla.org/base/telemetry-startup;1",
    "@mozilla.org/embedcomp/default-tooltiptextprovider;1",
    "@mozilla.org/push/Service;1",
  ]),
};

add_task(async function() {
  SimpleTest.requestCompleteLog();

  let tab = await BrowserTestUtils.openNewForegroundTab({gBrowser,
                                                         forceNewProcess: true});

  let mm = gBrowser.selectedBrowser.messageManager;
  let promise = BrowserTestUtils.waitForMessage(mm, "Test:LoadedScripts");

  // Load a custom frame script to avoid using ContentTask which loads Task.jsm
  mm.loadFrameScript("data:text/javascript,(" + function() {
    /* eslint-env mozilla/frame-script */
    const Cm = Components.manager;
    Cm.QueryInterface(Ci.nsIServiceManager);
    ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
    let collectStacks = AppConstants.NIGHTLY_BUILD || AppConstants.DEBUG;
    let components = {};
    for (let component of Cu.loadedComponents) {
      /* Keep only the file name for components, as the path is an absolute file
         URL rather than a resource:// URL like for modules. */
      components[component.replace(/.*\//, "")] =
        collectStacks ? Cu.getComponentLoadStack(component) : "";
    }
    let modules = {};
    for (let module of Cu.loadedModules) {
      modules[module] = collectStacks ? Cu.getModuleImportStack(module) : "";
    }
    let services = {};
    for (let contractID of Object.keys(Cc)) {
      try {
        if (Cm.isServiceInstantiatedByContractID(contractID, Ci.nsISupports)) {
          services[contractID] = "";
        }
      } catch (e) {}
    }
    sendAsyncMessage("Test:LoadedScripts", {components, modules, services});
  } + ")()", false);

  let loadedInfo = await promise;
  let loadedList = {};

  for (let scriptType in whitelist) {
    loadedList[scriptType] = Object.keys(loadedInfo[scriptType]).filter(c => {
      if (!whitelist[scriptType].has(c))
        return true;
      whitelist[scriptType].delete(c);
      return false;
    });

    loadedList[scriptType] = loadedList[scriptType].filter(c => {
      return !intermittently_loaded_whitelist[scriptType].has(c);
    });

    is(loadedList[scriptType].length, 0,
       `should have no unexpected ${scriptType} loaded on content process startup`);

    for (let script of loadedList[scriptType]) {
      ok(false, `Unexpected ${scriptType} loaded during content process startup: ${script}`);
      info(`Stack that loaded ${script}:\n`);
      info(loadedInfo[scriptType][script]);
    }

    is(whitelist[scriptType].size, 0,
       `all ${scriptType} whitelist entries should have been used`);

    for (let script of whitelist[scriptType]) {
      ok(false, `${scriptType} is whitelisted for content process startup but wasn't used: ${script}`);
    }

    if (kDumpAllStacks) {
      info(`Stacks for all loaded ${scriptType}:`);
      for (let file in loadedInfo[scriptType]) {
        if (loadedInfo[scriptType][file]) {
          info(`${file}\n------------------------------------\n` + loadedInfo[scriptType][file] + "\n");
        }
      }
    }
  }

  for (let scriptType in blacklist) {
    for (let script of blacklist[scriptType]) {
      let loaded = script in loadedInfo[scriptType];
      if (loaded) {
        ok(false, `Unexpected ${scriptType} loaded during content process startup: ${script}`);
        if (loadedInfo[scriptType][script]) {
          info(`Stack that loaded ${script}:\n`);
          info(loadedInfo[scriptType][script]);
        }
      }
    }
  }

  BrowserTestUtils.removeTab(tab);
});
