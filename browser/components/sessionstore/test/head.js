/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TAB_STATE_NEEDS_RESTORE = 1;
const TAB_STATE_RESTORING = 2;

const ROOT = getRootDirectory(gTestPath);
const FRAME_SCRIPTS = [
  ROOT + "content.js",
  ROOT + "content-forms.js"
];

let mm = Cc["@mozilla.org/globalmessagemanager;1"]
           .getService(Ci.nsIMessageListenerManager);

for (let script of FRAME_SCRIPTS) {
  mm.loadFrameScript(script, true);
}

mm.addMessageListener("SessionStore:setupSyncHandler", onSetupSyncHandler);

/**
 * This keeps track of all SyncHandlers passed to chrome from frame scripts.
 * We need this to let tests communicate with frame scripts and cause (a)sync
 * flushes.
 */
let SyncHandlers = new WeakMap();
function onSetupSyncHandler(msg) {
  SyncHandlers.set(msg.target, msg.objects.handler);
}

registerCleanupFunction(() => {
  for (let script of FRAME_SCRIPTS) {
    mm.removeDelayedFrameScript(script, true);
  }
  mm.removeMessageListener("SessionStore:setupSyncHandler", onSetupSyncHandler);
});

let tmp = {};
Cu.import("resource://gre/modules/Promise.jsm", tmp);
Cu.import("resource:///modules/sessionstore/SessionStore.jsm", tmp);
let {Promise, SessionStore} = tmp;

let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

// Some tests here assume that all restored tabs are loaded without waiting for
// the user to bring them to the foreground. We ensure this by resetting the
// related preference (see the "firefox.js" defaults file for details).
Services.prefs.setBoolPref("browser.sessionstore.restore_on_demand", false);
registerCleanupFunction(function () {
  Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");
});

// Obtain access to internals
Services.prefs.setBoolPref("browser.sessionstore.debug", true);
registerCleanupFunction(function () {
  Services.prefs.clearUserPref("browser.sessionstore.debug");
});


// This kicks off the search service used on about:home and allows the
// session restore tests to be run standalone without triggering errors.
Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

function provideWindow(aCallback, aURL, aFeatures) {
  function callbackSoon(aWindow) {
    executeSoon(function executeCallbackSoon() {
      aCallback(aWindow);
    });
  }

  let win = openDialog(getBrowserURL(), "", aFeatures || "chrome,all,dialog=no", aURL);
  whenWindowLoaded(win, function onWindowLoaded(aWin) {
    if (!aURL) {
      info("Loaded a blank window.");
      callbackSoon(aWin);
      return;
    }

    aWin.gBrowser.selectedBrowser.addEventListener("load", function selectedBrowserLoadListener() {
      aWin.gBrowser.selectedBrowser.removeEventListener("load", selectedBrowserLoadListener, true);
      callbackSoon(aWin);
    }, true);
  });
}

// This assumes that tests will at least have some state/entries
function waitForBrowserState(aState, aSetStateCallback) {
  let windows = [window];
  let tabsRestored = 0;
  let expectedTabsRestored = 0;
  let expectedWindows = aState.windows.length;
  let windowsOpen = 1;
  let listening = false;
  let windowObserving = false;
  let restoreHiddenTabs = Services.prefs.getBoolPref(
                          "browser.sessionstore.restore_hidden_tabs");

  aState.windows.forEach(function (winState) {
    winState.tabs.forEach(function (tabState) {
      if (restoreHiddenTabs || !tabState.hidden)
        expectedTabsRestored++;
    });
  });

  // There must be only hidden tabs and restoreHiddenTabs = false. We still
  // expect one of them to be restored because it gets shown automatically.
  if (!expectedTabsRestored)
    expectedTabsRestored = 1;

  function onSSTabRestored(aEvent) {
    if (++tabsRestored == expectedTabsRestored) {
      // Remove the event listener from each window
      windows.forEach(function(win) {
        win.gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, true);
      });
      listening = false;
      info("running " + aSetStateCallback.name);
      executeSoon(aSetStateCallback);
    }
  }

  // Used to add our listener to further windows so we can catch SSTabRestored
  // coming from them when creating a multi-window state.
  function windowObserver(aSubject, aTopic, aData) {
    if (aTopic == "domwindowopened") {
      let newWindow = aSubject.QueryInterface(Ci.nsIDOMWindow);
      newWindow.addEventListener("load", function() {
        newWindow.removeEventListener("load", arguments.callee, false);

        if (++windowsOpen == expectedWindows) {
          Services.ww.unregisterNotification(windowObserver);
          windowObserving = false;
        }

        // Track this window so we can remove the progress listener later
        windows.push(newWindow);
        // Add the progress listener
        newWindow.gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, true);
      }, false);
    }
  }

  // We only want to register the notification if we expect more than 1 window
  if (expectedWindows > 1) {
    registerCleanupFunction(function() {
      if (windowObserving) {
        Services.ww.unregisterNotification(windowObserver);
      }
    });
    windowObserving = true;
    Services.ww.registerNotification(windowObserver);
  }

  registerCleanupFunction(function() {
    if (listening) {
      windows.forEach(function(win) {
        win.gBrowser.tabContainer.removeEventListener("SSTabRestored", onSSTabRestored, true);
      });
    }
  });
  // Add the event listener for this window as well.
  listening = true;
  gBrowser.tabContainer.addEventListener("SSTabRestored", onSSTabRestored, true);

  // Finally, call setBrowserState
  ss.setBrowserState(JSON.stringify(aState));
}

// Doesn't assume that the tab needs to be closed in a cleanup function.
// If that's the case, the test author should handle that in the test.
function waitForTabState(aTab, aState, aCallback) {
  let listening = true;

  function onSSTabRestored() {
    aTab.removeEventListener("SSTabRestored", onSSTabRestored, false);
    listening = false;
    aCallback();
  }

  aTab.addEventListener("SSTabRestored", onSSTabRestored, false);

  registerCleanupFunction(function() {
    if (listening) {
      aTab.removeEventListener("SSTabRestored", onSSTabRestored, false);
    }
  });
  ss.setTabState(aTab, JSON.stringify(aState));
}

/**
 * Wait for a content -> chrome message.
 */
function promiseContentMessage(browser, name) {
  let deferred = Promise.defer();
  let mm = browser.messageManager;

  function removeListener() {
    mm.removeMessageListener(name, listener);
  }

  function listener(msg) {
    removeListener();
    deferred.resolve(msg.data);
  }

  mm.addMessageListener(name, listener);
  registerCleanupFunction(removeListener);
  return deferred.promise;
}

function waitForTopic(aTopic, aTimeout, aCallback) {
  let observing = false;
  function removeObserver() {
    if (!observing)
      return;
    Services.obs.removeObserver(observer, aTopic);
    observing = false;
  }

  let timeout = setTimeout(function () {
    removeObserver();
    aCallback(false);
  }, aTimeout);

  function observer(aSubject, aTopic, aData) {
    removeObserver();
    timeout = clearTimeout(timeout);
    executeSoon(() => aCallback(true));
  }

  registerCleanupFunction(function() {
    removeObserver();
    if (timeout) {
      clearTimeout(timeout);
    }
  });

  observing = true;
  Services.obs.addObserver(observer, aTopic, false);
}

/**
 * Wait until session restore has finished collecting its data and is
 * getting ready to write that data ("sessionstore-state-write").
 *
 * This function is meant to be called immediately after the code
 * that will trigger the saving.
 *
 * Note that this does not wait for the disk write to be complete.
 *
 * @param {function} aCallback If sessionstore-state-write is sent
 * within buffering interval + 100 ms, the callback is passed |true|,
 * otherwise, it is passed |false|.
 */
function waitForSaveState(aCallback) {
  let timeout = 100 +
    Services.prefs.getIntPref("browser.sessionstore.interval");
  return waitForTopic("sessionstore-state-write", timeout, aCallback);
}
function promiseSaveState() {
  let deferred = Promise.defer();
  waitForSaveState(isSuccessful => {
    if (isSuccessful) {
      deferred.resolve();
    } else {
      deferred.reject(new Error("timeout"));
    }});
  return deferred.promise;
}
function forceSaveState() {
  let promise = promiseSaveState();
  const PREF = "browser.sessionstore.interval";
  // Set interval to an arbitrary non-0 duration
  // to ensure that setting it to 0 will notify observers
  Services.prefs.setIntPref(PREF, 1000);
  Services.prefs.setIntPref(PREF, 0);
  return promise.then(
    function onSuccess(x) {
      Services.prefs.clearUserPref(PREF);
      return x;
    },
    function onError(x) {
      Services.prefs.clearUserPref(PREF);
      throw x;
    }
  );
}

function whenBrowserLoaded(aBrowser, aCallback = next, ignoreSubFrames = true) {
  aBrowser.addEventListener("load", function onLoad(event) {
    if (!ignoreSubFrames || event.target == aBrowser.contentDocument) {
      aBrowser.removeEventListener("load", onLoad, true);
      executeSoon(aCallback);
    }
  }, true);
}
function promiseBrowserLoaded(aBrowser, ignoreSubFrames = true) {
  let deferred = Promise.defer();
  whenBrowserLoaded(aBrowser, deferred.resolve, ignoreSubFrames);
  return deferred.promise;
}
function whenBrowserUnloaded(aBrowser, aContainer, aCallback = next) {
  aBrowser.addEventListener("unload", function onUnload() {
    aBrowser.removeEventListener("unload", onUnload, true);
    executeSoon(aCallback);
  }, true);
}
function promiseBrowserUnloaded(aBrowser, aContainer) {
  let deferred = Promise.defer();
  whenBrowserUnloaded(aBrowser, aContainer, deferred.resolve);
  return deferred.promise;
}

function whenWindowLoaded(aWindow, aCallback = next) {
  aWindow.addEventListener("load", function windowLoadListener() {
    aWindow.removeEventListener("load", windowLoadListener, false);
    executeSoon(function executeWhenWindowLoaded() {
      aCallback(aWindow);
    });
  }, false);
}

function whenTabRestored(aTab, aCallback = next) {
  aTab.addEventListener("SSTabRestored", function onRestored(aEvent) {
    aTab.removeEventListener("SSTabRestored", onRestored, true);
    executeSoon(function executeWhenTabRestored() {
      aCallback();
    });
  }, true);
}

var gUniqueCounter = 0;
function r() {
  return Date.now() + "-" + (++gUniqueCounter);
}

function BrowserWindowIterator() {
  let windowsEnum = Services.wm.getEnumerator("navigator:browser");
  while (windowsEnum.hasMoreElements()) {
    let currentWindow = windowsEnum.getNext();
    if (!currentWindow.closed) {
      yield currentWindow;
    }
  }
}

let gWebProgressListener = {
  _callback: null,

  setCallback: function (aCallback) {
    if (!this._callback) {
      window.gBrowser.addTabsProgressListener(this);
    }
    this._callback = aCallback;
  },

  unsetCallback: function () {
    if (this._callback) {
      this._callback = null;
      window.gBrowser.removeTabsProgressListener(this);
    }
  },

  onStateChange: function (aBrowser, aWebProgress, aRequest,
                           aStateFlags, aStatus) {
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) {
      this._callback(aBrowser);
    }
  }
};

registerCleanupFunction(function () {
  gWebProgressListener.unsetCallback();
});

let gProgressListener = {
  _callback: null,

  setCallback: function (callback) {
    Services.obs.addObserver(this, "sessionstore-debug-tab-restored", false);
    this._callback = callback;
  },

  unsetCallback: function () {
    if (this._callback) {
      this._callback = null;
    Services.obs.removeObserver(this, "sessionstore-debug-tab-restored");
    }
  },

  observe: function (browser, topic, data) {
    gProgressListener.onRestored(browser);
  },

  onRestored: function (browser) {
    if (browser.__SS_restoreState == TAB_STATE_RESTORING) {
      let args = [browser].concat(gProgressListener._countTabs());
      gProgressListener._callback.apply(gProgressListener, args);
    }
  },

  _countTabs: function () {
    let needsRestore = 0, isRestoring = 0, wasRestored = 0;

    for (let win in BrowserWindowIterator()) {
      for (let i = 0; i < win.gBrowser.tabs.length; i++) {
        let browser = win.gBrowser.tabs[i].linkedBrowser;
        if (!browser.__SS_restoreState)
          wasRestored++;
        else if (browser.__SS_restoreState == TAB_STATE_RESTORING)
          isRestoring++;
        else if (browser.__SS_restoreState == TAB_STATE_NEEDS_RESTORE)
          needsRestore++;
      }
    }
    return [needsRestore, isRestoring, wasRestored];
  }
};

registerCleanupFunction(function () {
  gProgressListener.unsetCallback();
});

// Close everything but our primary window. We can't use waitForFocus()
// because apparently it's buggy. See bug 599253.
function closeAllButPrimaryWindow() {
  for (let win in BrowserWindowIterator()) {
    if (win != window) {
      win.close();
    }
  }
}

/**
 * When opening a new window it is not sufficient to wait for its load event.
 * We need to use whenDelayedStartupFinshed() here as the browser window's
 * delayedStartup() routine is executed one tick after the window's load event
 * has been dispatched. browser-delayed-startup-finished might be deferred even
 * further if parts of the window's initialization process take more time than
 * expected (e.g. reading a big session state from disk).
 */
function whenNewWindowLoaded(aOptions, aCallback) {
  let win = OpenBrowserWindow(aOptions);
  whenDelayedStartupFinished(win, () => aCallback(win));
  return win;
}
function promiseNewWindowLoaded(aOptions) {
  let deferred = Promise.defer();
  whenNewWindowLoaded(aOptions, deferred.resolve);
  return deferred.promise;
}

/**
 * Chrome windows aren't closed synchronously. Provide a helper method to close
 * a window and wait until we received the "domwindowclosed" notification for it.
 */
function promiseWindowClosed(win) {
  let deferred = Promise.defer();

  Services.obs.addObserver(function obs(subject, topic) {
    if (subject == win) {
      Services.obs.removeObserver(obs, topic);
      deferred.resolve();
    }
  }, "domwindowclosed", false);

  win.close();
  return deferred.promise;
}

/**
 * This waits for the browser-delayed-startup-finished notification of a given
 * window. It indicates that the windows has loaded completely and is ready to
 * be used for testing.
 */
function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished", false);
}

/**
 * The test runner that controls the execution flow of our tests.
 */
let TestRunner = {
  _iter: null,

  /**
   * Holds the browser state from before we started so
   * that we can restore it after all tests ran.
   */
  backupState: {},

  /**
   * Starts the test runner.
   */
  run: function () {
    waitForExplicitFinish();

    SessionStore.promiseInitialized.then(() => {
      this.backupState = JSON.parse(ss.getBrowserState());
      this._iter = runTests();
      this.next();
    });
  },

  /**
   * Runs the next available test or finishes if there's no test left.
   */
  next: function () {
    try {
      TestRunner._iter.next();
    } catch (e if e instanceof StopIteration) {
      TestRunner.finish();
    }
  },

  /**
   * Finishes all tests and cleans up.
   */
  finish: function () {
    closeAllButPrimaryWindow();
    waitForBrowserState(this.backupState, finish);
  }
};

function next() {
  TestRunner.next();
}

function promiseTabRestored(tab) {
  let deferred = Promise.defer();

  tab.addEventListener("SSTabRestored", function onRestored() {
    tab.removeEventListener("SSTabRestored", onRestored);
    deferred.resolve();
  });

  return deferred.promise;
}

function sendMessage(browser, name, data = {}) {
  browser.messageManager.sendAsyncMessage(name, data);
  return promiseContentMessage(browser, name);
}

// This creates list of functions that we will map to their corresponding
// ss-test:* messages names. Those will be sent to the frame script and
// be used to read and modify form data.
const FORM_HELPERS = [
  "getTextContent",
  "getInputValue", "setInputValue",
  "getInputChecked", "setInputChecked",
  "getSelectedIndex", "setSelectedIndex",
  "getMultipleSelected", "setMultipleSelected",
  "getFileNameArray", "setFileNameArray",
];

for (let name of FORM_HELPERS) {
  let msg = "ss-test:" + name;
  this[name] = (browser, data) => sendMessage(browser, msg, data);
}
