Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished", false);
}

function findChromeWindowByURI(aURI) {
  let windows = Services.wm.getEnumerator(null);
  while (windows.hasMoreElements()) {
    let win = windows.getNext();
    if (win.location.href == aURI)
      return win;
  }
  return null;
}

function updateTabContextMenu(tab) {
  let menu = document.getElementById("tabContextMenu");
  if (!tab)
    tab = gBrowser.selectedTab;
  var evt = new Event("");
  tab.dispatchEvent(evt);
  menu.openPopup(tab, "end_after", 0, 0, true, false, evt);
  is(TabContextMenu.contextTab, tab, "TabContextMenu context is the expected tab");
  menu.hidePopup();
}

function openToolbarCustomizationUI(aCallback, aBrowserWin) {
  if (!aBrowserWin)
    aBrowserWin = window;

  aBrowserWin.gCustomizeMode.enter();

  aBrowserWin.gNavToolbox.addEventListener("customizationready", function UI_loaded() {
    aBrowserWin.gNavToolbox.removeEventListener("customizationready", UI_loaded);
    executeSoon(function() {
      aCallback(aBrowserWin)
    });
  });
}

function closeToolbarCustomizationUI(aCallback, aBrowserWin) {
  aBrowserWin.gNavToolbox.addEventListener("aftercustomization", function unloaded() {
    aBrowserWin.gNavToolbox.removeEventListener("aftercustomization", unloaded);
    executeSoon(aCallback);
  });

  aBrowserWin.gCustomizeMode.exit();
}

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function getTestPlugin(aName) {
  var pluginName = aName || "Test Plug-in";
  var ph = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = ph.getPluginTags();

  // Find the test plugin
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == pluginName)
      return tags[i];
  }
  ok(false, "Unable to find plugin");
  return null;
}

// call this to set the test plugin(s) initially expected enabled state.
// it will automatically be reset to it's previous value after the test
// ends
function setTestPluginEnabledState(newEnabledState, pluginName) {
  var plugin = getTestPlugin(pluginName);
  var oldEnabledState = plugin.enabledState;
  plugin.enabledState = newEnabledState;
  SimpleTest.registerCleanupFunction(function() {
    getTestPlugin(pluginName).enabledState = oldEnabledState;
  });
}

// after a test is done using the plugin doorhanger, we should just clear
// any permissions that may have crept in
function clearAllPluginPermissions() {
  let perms = Services.perms.enumerator;
  while (perms.hasMoreElements()) {
    let perm = perms.getNext();
    if (perm.type.startsWith('plugin')) {
      Services.perms.remove(perm.host, perm.type);
    }
  }
}

function updateBlocklist(aCallback) {
  var blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
                          .getService(Ci.nsITimerCallback);
  var observer = function() {
    Services.obs.removeObserver(observer, "blocklist-updated");
    SimpleTest.executeSoon(aCallback);
  };
  Services.obs.addObserver(observer, "blocklist-updated", false);
  blocklistNotifier.notify(null);
}

var _originalTestBlocklistURL = null;
function setAndUpdateBlocklist(aURL, aCallback) {
  if (!_originalTestBlocklistURL)
    _originalTestBlocklistURL = Services.prefs.getCharPref("extensions.blocklist.url");
  Services.prefs.setCharPref("extensions.blocklist.url", aURL);
  updateBlocklist(aCallback);
}

function resetBlocklist() {
  Services.prefs.setCharPref("extensions.blocklist.url", _originalTestBlocklistURL);
}

function whenNewWindowLoaded(aOptions, aCallback) {
  let win = OpenBrowserWindow(aOptions);
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    aCallback(win);
  }, false);
}

/**
 * Waits for all pending async statements on the default connection, before
 * proceeding with aCallback.
 *
 * @param aCallback
 *        Function to be called when done.
 * @param aScope
 *        Scope for the callback.
 * @param aArguments
 *        Arguments array for the callback.
 *
 * @note The result is achieved by asynchronously executing a query requiring
 *       a write lock.  Since all statements on the same connection are
 *       serialized, the end of this write operation means that all writes are
 *       complete.  Note that WAL makes so that writers don't block readers, but
 *       this is a problem only across different connections.
 */
function waitForAsyncUpdates(aCallback, aScope, aArguments) {
  let scope = aScope || this;
  let args = aArguments || [];
  let db = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                              .DBConnection;
  let begin = db.createAsyncStatement("BEGIN EXCLUSIVE");
  begin.executeAsync();
  begin.finalize();

  let commit = db.createAsyncStatement("COMMIT");
  commit.executeAsync({
    handleResult: function() {},
    handleError: function() {},
    handleCompletion: function(aReason) {
      aCallback.apply(scope, args);
    }
  });
  commit.finalize();
}

/**
 * Asynchronously check a url is visited.

 * @param aURI The URI.
 * @param aExpectedValue The expected value.
 * @return {Promise}
 * @resolves When the check has been added successfully.
 * @rejects JavaScript exception.
 */
function promiseIsURIVisited(aURI, aExpectedValue) {
  let deferred = Promise.defer();
  PlacesUtils.asyncHistory.isURIVisited(aURI, function(aURI, aIsVisited) {
    deferred.resolve(aIsVisited);
  });

  return deferred.promise;
}

function whenNewTabLoaded(aWindow, aCallback) {
  aWindow.BrowserOpenTab();

  let browser = aWindow.gBrowser.selectedBrowser;
  if (browser.contentDocument.readyState === "complete") {
    aCallback();
    return;
  }

  whenTabLoaded(aWindow.gBrowser.selectedTab, aCallback);
}

function whenTabLoaded(aTab, aCallback) {
  let browser = aTab.linkedBrowser;
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    executeSoon(aCallback);
  }, true);
}

function promiseTabLoaded(aTab) {
  let deferred = Promise.defer();
  whenTabLoaded(aTab, deferred.resolve);
  return deferred.promise;
}

function addVisits(aPlaceInfo, aCallback) {
  let places = [];
  if (aPlaceInfo instanceof Ci.nsIURI) {
    places.push({ uri: aPlaceInfo });
  } else if (Array.isArray(aPlaceInfo)) {
    places = places.concat(aPlaceInfo);
  } else {
    places.push(aPlaceInfo);
   }

  // Create mozIVisitInfo for each entry.
  let now = Date.now();
  for (let i = 0; i < places.length; i++) {
    if (!places[i].title) {
      places[i].title = "test visit for " + places[i].uri.spec;
    }
    places[i].visits = [{
      transitionType: places[i].transition === undefined ? Ci.nsINavHistoryService.TRANSITION_LINK
                                                         : places[i].transition,
      visitDate: places[i].visitDate || (now++) * 1000,
      referrerURI: places[i].referrer
    }];
  }

  PlacesUtils.asyncHistory.updatePlaces(
    places,
    {
      handleError: function AAV_handleError() {
        throw("Unexpected error in adding visit.");
      },
      handleResult: function () {},
      handleCompletion: function UP_handleCompletion() {
        if (aCallback)
          aCallback();
      }
    }
  );
}

/**
 * Ensures that the specified URIs are either cleared or not.
 *
 * @param aURIs
 *        Array of page URIs
 * @param aShouldBeCleared
 *        True if each visit to the URI should be cleared, false otherwise
 */
function promiseHistoryClearedState(aURIs, aShouldBeCleared) {
  let deferred = Promise.defer();
  let callbackCount = 0;
  let niceStr = aShouldBeCleared ? "no longer" : "still";
  function callbackDone() {
    if (++callbackCount == aURIs.length)
      deferred.resolve();
  }
  aURIs.forEach(function (aURI) {
    PlacesUtils.asyncHistory.isURIVisited(aURI, function(aURI, aIsVisited) {
      is(aIsVisited, !aShouldBeCleared,
         "history visit " + aURI.spec + " should " + niceStr + " exist");
      callbackDone();
    });
  });

  return deferred.promise;
}

/**
 * Allows waiting for an observer notification once.
 *
 * @param topic
 *        Notification topic to observe.
 *
 * @return {Promise}
 * @resolves The array [subject, data] from the observed notification.
 * @rejects Never.
 */
function promiseTopicObserved(topic)
{
  let deferred = Promise.defer();
  Services.obs.addObserver(function PTO_observe(subject, topic, data) {
    Services.obs.removeObserver(PTO_observe, topic);
    deferred.resolve([subject, data]);
  }, topic, false);
  return deferred.promise;
}

/**
 * Clears history asynchronously.
 *
 * @return {Promise}
 * @resolves When history has been cleared.
 * @rejects Never.
 */
function promiseClearHistory() {
  let promise = promiseTopicObserved(PlacesUtils.TOPIC_EXPIRATION_FINISHED);
  PlacesUtils.bhistory.removeAllPages();
  return promise;
}

/**
 * Waits for the next top-level document load in the current browser.  The URI
 * of the document is compared against aExpectedURL.  The load is then stopped
 * before it actually starts.
 *
 * @param aExpectedURL
 *        The URL of the document that is expected to load.
 * @return promise
 */
function waitForDocLoadAndStopIt(aExpectedURL) {
  let deferred = Promise.defer();
  let progressListener = {
    onStateChange: function (webProgress, req, flags, status) {
      info("waitForDocLoadAndStopIt: onStateChange: " + req.name);
      let docStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                     Ci.nsIWebProgressListener.STATE_START;
      if ((flags & docStart) && webProgress.isTopLevel) {
        info("waitForDocLoadAndStopIt: Document start: " +
             req.QueryInterface(Ci.nsIChannel).URI.spec);
        is(req.originalURI.spec, aExpectedURL,
           "waitForDocLoadAndStopIt: The expected URL was loaded");
        req.cancel(Components.results.NS_ERROR_FAILURE);
        gBrowser.removeProgressListener(progressListener);
        deferred.resolve();
      }
    },
  };
  gBrowser.addProgressListener(progressListener);
  info("waitForDocLoadAndStopIt: Waiting for URL: " + aExpectedURL);
  return deferred.promise;
}

let FullZoomHelper = {

  selectTabAndWaitForLocationChange: function selectTabAndWaitForLocationChange(tab) {
    if (!tab)
      throw new Error("tab must be given.");
    if (gBrowser.selectedTab == tab)
      return Promise.resolve();
    gBrowser.selectedTab = tab;
    return this.waitForLocationChange();
  },

  removeTabAndWaitForLocationChange: function removeTabAndWaitForLocationChange(tab) {
    tab = tab || gBrowser.selectedTab;
    let selected = gBrowser.selectedTab == tab;
    gBrowser.removeTab(tab);
    if (selected)
      return this.waitForLocationChange();
    return Promise.resolve();
  },

  waitForLocationChange: function waitForLocationChange() {
    let deferred = Promise.defer();
    Services.obs.addObserver(function obs(subj, topic, data) {
      Services.obs.removeObserver(obs, topic);
      deferred.resolve();
    }, "browser-fullZoom:location-change", false);
    return deferred.promise;
  },

  load: function load(tab, url) {
    let deferred = Promise.defer();
    let didLoad = false;
    let didZoom = false;

    tab.linkedBrowser.addEventListener("load", function (event) {
      event.currentTarget.removeEventListener("load", arguments.callee, true);
      didLoad = true;
      if (didZoom)
        deferred.resolve();
    }, true);

    this.waitForLocationChange().then(function () {
      didZoom = true;
      if (didLoad)
        deferred.resolve();
    });

    tab.linkedBrowser.loadURI(url);

    return deferred.promise;
  },

  zoomTest: function zoomTest(tab, val, msg) {
    is(ZoomManager.getZoomForBrowser(tab.linkedBrowser), val, msg);
  },

  enlarge: function enlarge() {
    let deferred = Promise.defer();
    FullZoom.enlarge(function () deferred.resolve());
    return deferred.promise;
  },

  reduce: function reduce() {
    let deferred = Promise.defer();
    FullZoom.reduce(function () deferred.resolve());
    return deferred.promise;
  },

  reset: function reset() {
    let deferred = Promise.defer();
    FullZoom.reset(function () deferred.resolve());
    return deferred.promise;
  },

  BACK: 0,
  FORWARD: 1,
  navigate: function navigate(direction) {
    let deferred = Promise.defer();
    let didPs = false;
    let didZoom = false;

    gBrowser.addEventListener("pageshow", function (event) {
      gBrowser.removeEventListener("pageshow", arguments.callee, true);
      didPs = true;
      if (didZoom)
        deferred.resolve();
    }, true);

    if (direction == this.BACK)
      gBrowser.goBack();
    else if (direction == this.FORWARD)
      gBrowser.goForward();

    this.waitForLocationChange().then(function () {
      didZoom = true;
      if (didPs)
        deferred.resolve();
    });
    return deferred.promise;
  },

  failAndContinue: function failAndContinue(func) {
    return function (err) {
      ok(false, err);
      func();
    };
  },
};
