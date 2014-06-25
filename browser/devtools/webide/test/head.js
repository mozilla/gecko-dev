/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Task.jsm");

const {Promise: promise} = Cu.import("resource://gre/modules/devtools/deprecated-sync-thenables.js", {});
const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;
const {AppProjects} = require("devtools/app-manager/app-projects");

const TEST_BASE = "chrome://mochitests/content/chrome/browser/devtools/webide/test/";

Services.prefs.setBoolPref("devtools.webide.enabled", true);
Services.prefs.setBoolPref("devtools.webide.enableLocalRuntime", true);

Services.prefs.setCharPref("devtools.webide.addonsURL", TEST_BASE + "addons/simulators.json");
Services.prefs.setCharPref("devtools.webide.simulatorAddonsURL", TEST_BASE + "addons/fxos_#SLASHED_VERSION#_simulator-#OS#.xpi");
Services.prefs.setCharPref("devtools.webide.adbAddonURL", TEST_BASE + "addons/adbhelper-#OS#.xpi");
Services.prefs.setCharPref("devtools.webide.templatesURL", TEST_BASE + "templates.json");


SimpleTest.registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.webide.templatesURL");
  Services.prefs.clearUserPref("devtools.webide.enabled");
  Services.prefs.clearUserPref("devtools.webide.enableLocalRuntime");
  Services.prefs.clearUserPref("devtools.webide.addonsURL");
  Services.prefs.clearUserPref("devtools.webide.simulatorAddonsURL");
  Services.prefs.clearUserPref("devtools.webide.adbAddonURL");
  Services.prefs.clearUserPref("devtools.webide.autoInstallADBHelper", false);
});

function openWebIDE(autoInstallADBHelper) {
  info("opening WebIDE");

  if (!autoInstallADBHelper) {
    Services.prefs.setBoolPref("devtools.webide.autoinstallADBHelper", false);
  }

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
    let projects = AppProjects.store.object.projects;
    for (let i = 0; i < projects.length; i++) {
      yield AppProjects.remove(projects[i].location);
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
