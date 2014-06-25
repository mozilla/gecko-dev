/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_MSG = "ContentSearchTest";
const CONTENT_SEARCH_MSG = "ContentSearch";
const TEST_CONTENT_SCRIPT_BASENAME = "contentSearch.js";

var gMsgMan;

add_task(function* GetState() {
  yield addTab();
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "GetState",
  });
  let msg = yield waitForTestMsg("State");
  checkMsg(msg, {
    type: "State",
    data: yield currentStateObj(),
  });
});

add_task(function* SetCurrentEngine() {
  yield addTab();
  let newCurrentEngine = null;
  let oldCurrentEngine = Services.search.currentEngine;
  let engines = Services.search.getVisibleEngines();
  for (let engine of engines) {
    if (engine != oldCurrentEngine) {
      newCurrentEngine = engine;
      break;
    }
  }
  if (!newCurrentEngine) {
    info("Couldn't find a non-selected search engine, " +
         "skipping this part of the test");
    return;
  }
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "SetCurrentEngine",
    data: newCurrentEngine.name,
  });
  let deferred = Promise.defer();
  Services.obs.addObserver(function obs(subj, topic, data) {
    info("Test observed " + data);
    if (data == "engine-current") {
      ok(true, "Test observed engine-current");
      Services.obs.removeObserver(obs, "browser-search-engine-modified", false);
      deferred.resolve();
    }
  }, "browser-search-engine-modified", false);
  let searchPromise = waitForTestMsg("CurrentEngine");
  info("Waiting for test to observe engine-current...");
  yield deferred.promise;
  let msg = yield searchPromise;
  checkMsg(msg, {
    type: "CurrentEngine",
    data: yield currentEngineObj(newCurrentEngine),
  });

  Services.search.currentEngine = oldCurrentEngine;
  let msg = yield waitForTestMsg("CurrentEngine");
  checkMsg(msg, {
    type: "CurrentEngine",
    data: yield currentEngineObj(oldCurrentEngine),
  });
});

add_task(function* ManageEngines() {
  yield addTab();
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "ManageEngines",
  });
  let deferred = Promise.defer();
  let winWatcher = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                   getService(Ci.nsIWindowWatcher);
  winWatcher.registerNotification(function onOpen(subj, topic, data) {
    if (topic == "domwindowopened" && subj instanceof Ci.nsIDOMWindow) {
      subj.addEventListener("load", function onLoad() {
        subj.removeEventListener("load", onLoad);
        if (subj.document.documentURI ==
            "chrome://browser/content/search/engineManager.xul") {
          winWatcher.unregisterNotification(onOpen);
          ok(true, "Observed search manager window open");
          is(subj.opener, window,
             "Search engine manager opener should be this chrome window");
          subj.close();
          deferred.resolve();
        }
      });
    }
  });
  info("Waiting for search engine manager window to open...");
  yield deferred.promise;
});

add_task(function* modifyEngine() {
  yield addTab();
  let engine = Services.search.currentEngine;
  let oldAlias = engine.alias;
  engine.alias = "ContentSearchTest";
  let msg = yield waitForTestMsg("CurrentState");
  checkMsg(msg, {
    type: "CurrentState",
    data: yield currentStateObj(),
  });
  engine.alias = oldAlias;
  msg = yield waitForTestMsg("CurrentState");
  checkMsg(msg, {
    type: "CurrentState",
    data: yield currentStateObj(),
  });
});

add_task(function* search() {
  yield addTab();
  let engine = Services.search.currentEngine;
  let data = {
    engineName: engine.name,
    searchString: "ContentSearchTest",
    whence: "ContentSearchTest",
  };
  gMsgMan.sendAsyncMessage(TEST_MSG, {
    type: "Search",
    data: data,
  });
  let submissionURL =
    engine.getSubmission(data.searchString, "", data.whence).uri.spec;
  let deferred = Promise.defer();
  let listener = {
    onStateChange: function (webProg, req, flags, status) {
      let url = req.originalURI.spec;
      info("onStateChange " + url);
      let docStart = Ci.nsIWebProgressListener.STATE_IS_DOCUMENT |
                     Ci.nsIWebProgressListener.STATE_START;
      if ((flags & docStart) && webProg.isTopLevel && url == submissionURL) {
        gBrowser.removeProgressListener(listener);
        ok(true, "Search URL loaded");
        req.cancel(Components.results.NS_ERROR_FAILURE);
        deferred.resolve();
      }
    }
  };
  gBrowser.addProgressListener(listener);
  info("Waiting for search URL to load: " + submissionURL);
  yield deferred.promise;
});

add_task(function* badImage() {
  yield addTab();
  // If the bad image URI caused an exception to be thrown within ContentSearch,
  // then we'll hang waiting for the CurrentState responses triggered by the new
  // engine.  That's what we're testing, and obviously it shouldn't happen.
  let vals = yield waitForNewEngine("contentSearchBadImage.xml", 1);
  let engine = vals[0];
  let finalCurrentStateMsg = vals[vals.length - 1];
  let expectedCurrentState = yield currentStateObj();
  let expectedEngine =
    expectedCurrentState.engines.find(e => e.name == engine.name);
  ok(!!expectedEngine, "Sanity check: engine should be in expected state");
  ok(expectedEngine.iconBuffer === null,
     "Sanity check: icon array buffer of engine in expected state " +
     "should be null: " + expectedEngine.iconBuffer);
  checkMsg(finalCurrentStateMsg, {
    type: "CurrentState",
    data: expectedCurrentState,
  });
  // Removing the engine triggers a final CurrentState message.  Wait for it so
  // it doesn't trip up subsequent tests.
  Services.search.removeEngine(engine);
  yield waitForTestMsg("CurrentState");
});

function checkMsg(actualMsg, expectedMsgData) {
  SimpleTest.isDeeply(actualMsg.data, expectedMsgData, "Checking message");
}

function waitForMsg(name, type) {
  let deferred = Promise.defer();
  info("Waiting for " + name + " message " + type + "...");
  gMsgMan.addMessageListener(name, function onMsg(msg) {
    info("Received " + name + " message " + msg.data.type + "\n");
    if (msg.data.type == type) {
      gMsgMan.removeMessageListener(name, onMsg);
      deferred.resolve(msg);
    }
  });
  return deferred.promise;
}

function waitForTestMsg(type) {
  return waitForMsg(TEST_MSG, type);
}

function waitForNewEngine(basename, numImages) {
  info("Waiting for engine to be added: " + basename);

  // Wait for the search events triggered by adding the new engine.
  // engine-added engine-loaded
  let expectedSearchEvents = ["CurrentState", "CurrentState"];
  // engine-changed for each of the images
  for (let i = 0; i < numImages; i++) {
    expectedSearchEvents.push("CurrentState");
  }
  let eventPromises = expectedSearchEvents.map(e => waitForTestMsg(e));

  // Wait for addEngine().
  let addDeferred = Promise.defer();
  let url = getRootDirectory(gTestPath) + basename;
  Services.search.addEngine(url, Ci.nsISearchEngine.TYPE_MOZSEARCH, "", false, {
    onSuccess: function (engine) {
      info("Search engine added: " + basename);
      addDeferred.resolve(engine);
    },
    onError: function (errCode) {
      ok(false, "addEngine failed with error code " + errCode);
      addDeferred.reject();
    },
  });

  return Promise.all([addDeferred.promise].concat(eventPromises));
}

function addTab() {
  let deferred = Promise.defer();
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("load", function load() {
    tab.removeEventListener("load", load, true);
    let url = getRootDirectory(gTestPath) + TEST_CONTENT_SCRIPT_BASENAME;
    gMsgMan = tab.linkedBrowser.messageManager;
    gMsgMan.sendAsyncMessage(CONTENT_SEARCH_MSG, {
      type: "AddToWhitelist",
      data: ["about:blank"],
    });
    waitForMsg(CONTENT_SEARCH_MSG, "AddToWhitelistAck").then(() => {
      gMsgMan.loadFrameScript(url, false);
      deferred.resolve();
    });
  }, true);
  registerCleanupFunction(() => gBrowser.removeTab(tab));
  return deferred.promise;
}

function currentStateObj() {
  return Task.spawn(function* () {
    let state = {
      engines: [],
      currentEngine: yield currentEngineObj(),
    };
    for (let engine of Services.search.getVisibleEngines()) {
      let uri = engine.getIconURLBySize(16, 16);
      state.engines.push({
        name: engine.name,
        iconBuffer: yield arrayBufferFromDataURI(uri),
      });
    }
    return state;
  }.bind(this));
}

function currentEngineObj() {
  return Task.spawn(function* () {
    let engine = Services.search.currentEngine;
    let uri1x = engine.getIconURLBySize(65, 26);
    let uri2x = engine.getIconURLBySize(130, 52);
    return {
      name: engine.name,
      logoBuffer: yield arrayBufferFromDataURI(uri1x),
      logo2xBuffer: yield arrayBufferFromDataURI(uri2x),
    };
  }.bind(this));
}

function arrayBufferFromDataURI(uri) {
  if (!uri) {
    return Promise.resolve(null);
  }
  let deferred = Promise.defer();
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
            createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("GET", uri, true);
  xhr.responseType = "arraybuffer";
  xhr.onloadend = () => {
    deferred.resolve(xhr.response);
  };
  try {
    xhr.send();
  }
  catch (err) {
    return Promise.resolve(null);
  }
  return deferred.promise;
}
