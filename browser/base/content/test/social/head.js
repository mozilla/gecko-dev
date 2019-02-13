/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");

function waitForCondition(condition, nextTest, errorMsg, numTries = 30) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= numTries) {
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


function promiseObserverNotified(aTopic) {
  let deferred = Promise.defer();
  Services.obs.addObserver(function onNotification(aSubject, aTopic, aData) {
    Services.obs.removeObserver(onNotification, aTopic);
      deferred.resolve({subject: aSubject, data: aData});
    }, aTopic, false);
  return deferred.promise;
}

// Check that a specified (string) URL hasn't been "remembered" (ie, is not
// in history, will not appear in about:newtab or auto-complete, etc.)
function promiseSocialUrlNotRemembered(url) {
  let deferred = Promise.defer();
  let uri = Services.io.newURI(url, null, null);
  PlacesUtils.asyncHistory.isURIVisited(uri, function(aURI, aIsVisited) {
    ok(!aIsVisited, "social URL " + url + " should not be in global history");
    deferred.resolve();
  });
  return deferred.promise;
}

let gURLsNotRemembered = [];


function checkProviderPrefsEmpty(isError) {
  let MANIFEST_PREFS = Services.prefs.getBranch("social.manifest.");
  let prefs = MANIFEST_PREFS.getChildList("", []);
  let c = 0;
  for (let pref of prefs) {
    if (MANIFEST_PREFS.prefHasUserValue(pref)) {
      info("provider [" + pref + "] manifest left installed from previous test");
      c++;
    }
  }
  is(c, 0, "all provider prefs uninstalled from previous test");
  is(Social.providers.length, 0, "all providers uninstalled from previous test " + Social.providers.length);
}

function defaultFinishChecks() {
  checkProviderPrefsEmpty(true);
  finish();
}

function runSocialTestWithProvider(manifest, callback, finishcallback) {

  let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

  let manifests = Array.isArray(manifest) ? manifest : [manifest];

  // Check that none of the provider's content ends up in history.
  function finishCleanUp() {
    ok(!SocialSidebar.provider, "no provider in sidebar");
    SessionStore.setWindowValue(window, "socialSidebar", "");
    for (let i = 0; i < manifests.length; i++) {
      let m = manifests[i];
      for (let what of ['sidebarURL', 'workerURL', 'iconURL', 'shareURL', 'markURL']) {
        if (m[what]) {
          yield promiseSocialUrlNotRemembered(m[what]);
        }
      };
    }
    for (let i = 0; i < gURLsNotRemembered.length; i++) {
      yield promiseSocialUrlNotRemembered(gURLsNotRemembered[i]);
    }
    gURLsNotRemembered = [];
  }

  info("runSocialTestWithProvider: " + manifests.toSource());

  let finishCount = 0;
  function finishIfDone(callFinish) {
    finishCount++;
    if (finishCount == manifests.length)
      Task.spawn(finishCleanUp).then(finishcallback || defaultFinishChecks);
  }
  function removeAddedProviders(cleanup) {
    manifests.forEach(function (m) {
      // If we're "cleaning up", don't call finish when done.
      let callback = cleanup ? function () {} : finishIfDone;
      // Similarly, if we're cleaning up, catch exceptions from removeProvider
      let removeProvider = SocialService.disableProvider.bind(SocialService);
      if (cleanup) {
        removeProvider = function (origin, cb) {
          try {
            SocialService.disableProvider(origin, cb);
          } catch (ex) {
            // Ignore "provider doesn't exist" errors.
            if (ex.message.indexOf("SocialService.disableProvider: no provider with origin") == 0)
              return;
            info("Failed to clean up provider " + origin + ": " + ex);
          }
        }
      }
      removeProvider(m.origin, callback);
    });
  }
  function finishSocialTest(cleanup) {
    removeAddedProviders(cleanup);
  }

  let providersAdded = 0;
  let firstProvider;

  manifests.forEach(function (m) {
    SocialService.addProvider(m, function(provider) {

      providersAdded++;
      info("runSocialTestWithProvider: provider added");

      // we want to set the first specified provider as the UI's provider
      if (provider.origin == manifests[0].origin) {
        firstProvider = provider;
      }

      // If we've added all the providers we need, call the callback to start
      // the tests (and give it a callback it can call to finish them)
      if (providersAdded == manifests.length) {
        registerCleanupFunction(function () {
          finishSocialTest(true);
        });
        waitForCondition(function() provider.enabled,
                         function() {
          info("provider has been enabled");
          callback(finishSocialTest);
        }, "providers added and enabled");
      }
    });
  });
}

function runSocialTests(tests, cbPreTest, cbPostTest, cbFinish) {
  let testIter = (function*() {
    for (let name in tests) {
      if (tests.hasOwnProperty(name)) {
        yield [name, tests[name]];
      }
    }
  })();
  let providersAtStart = Social.providers.length;
  info("runSocialTests: start test run with " + providersAtStart + " providers");
  window.focus();


  if (cbPreTest === undefined) {
    cbPreTest = function(cb) {cb()};
  }
  if (cbPostTest === undefined) {
    cbPostTest = function(cb) {cb()};
  }

  function runNextTest() {
    let result = testIter.next();
    if (result.done) {
      // out of items:
      (cbFinish || defaultFinishChecks)();
      is(providersAtStart, Social.providers.length,
         "runSocialTests: finish test run with " + Social.providers.length + " providers");
      return;
    }
    let [name, func] = result.value;
    // We run on a timeout as the frameworker also makes use of timeouts, so
    // this helps keep the debug messages sane.
    executeSoon(function() {
      function cleanupAndRunNextTest() {
        info("sub-test " + name + " complete");
        cbPostTest(runNextTest);
      }
      cbPreTest(function() {
        info("pre-test: starting with " + Social.providers.length + " providers");
        info("sub-test " + name + " starting");
        try {
          func.call(tests, cleanupAndRunNextTest);
        } catch (ex) {
          ok(false, "sub-test " + name + " failed: " + ex.toString() +"\n"+ex.stack);
          cleanupAndRunNextTest();
        }
      })
    });
  }
  runNextTest();
}

// A fairly large hammer which checks all aspects of the SocialUI for
// internal consistency.
function checkSocialUI(win) {
  let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;
  win = win || window;
  let doc = win.document;
  let enabled = win.SocialUI.enabled;
  let active = Social.providers.length > 0 && !win.SocialUI._chromeless &&
               !PrivateBrowsingUtils.isWindowPrivate(win);
  let sidebarEnabled = win.SocialSidebar.provider ? enabled : false;

  // if we have enabled providers, we should also have instances of those
  // providers
  if (SocialService.hasEnabledProviders) {
    ok(Social.providers.length > 0, "providers are enabled");
  } else {
    is(Social.providers.length, 0, "providers are not enabled");
  }

  // some local helpers to avoid log-spew for the many checks made here.
  let numGoodTests = 0, numTests = 0;
  function _ok(what, msg) {
    numTests++;
    if (!ok)
      ok(what, msg)
    else
      ++numGoodTests;
  }
  function _is(a, b, msg) {
    numTests++;
    if (a != b)
      is(a, b, msg)
    else
      ++numGoodTests;
  }
  function isbool(a, b, msg) {
    _is(!!a, !!b, msg);
  }
  isbool(win.SocialSidebar.canShow, sidebarEnabled, "social sidebar active?");

  let contextMenus = [
    {
      type: "link",
      id: "context-marklinkMenu",
      label: "social.marklinkMenu.label"
    },
    {
      type: "page",
      id: "context-markpageMenu",
      label: "social.markpageMenu.label"
    }
  ];

  for (let c of contextMenus) {
    let leMenu = document.getElementById(c.id);
    let parent, menus;
    let markProviders = SocialMarks.getProviders();
    if (markProviders.length > SocialMarks.MENU_LIMIT) {
      // menus should be in a submenu, not in the top level of the context menu
      parent = leMenu.firstChild;
      menus = document.getElementsByClassName("context-mark" + c.type);
      _is(menus.length, 0, "menu's are not in main context menu\n");
      menus = parent.childNodes;
      _is(menus.length, markProviders.length, c.id + " menu exists for each mark provider");
    } else {
      // menus should be in the top level of the context menu, not in a submenu
      parent = leMenu.parentNode;
      menus = document.getElementsByClassName("context-mark" + c.type);
      _is(menus.length, markProviders.length, c.id + " menu exists for each mark provider");
      menus = leMenu.firstChild.childNodes;
      _is(menus.length, 0, "menu's are not in context submenu\n");
    }
    for (let m of menus)
      _is(m.parentNode, parent, "menu has correct parent");
  }

  // and for good measure, check all the social commands.
  isbool(!doc.getElementById("Social:ToggleSidebar").hidden, sidebarEnabled, "Social:ToggleSidebar visible?");
  isbool(!doc.getElementById("Social:ToggleNotifications").hidden, enabled, "Social:ToggleNotifications visible?");

  // and report on overall success of failure of the various checks here.
  is(numGoodTests, numTests, "The Social UI tests succeeded.")
}

function waitForNotification(topic, cb) {
  function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);
    cb();
  }
  Services.obs.addObserver(observer, topic, false);
}

// blocklist testing
function updateBlocklist(aCallback) {
  var blocklistNotifier = Cc["@mozilla.org/extensions/blocklist;1"]
                          .getService(Ci.nsITimerCallback);
  var observer = function() {
    Services.obs.removeObserver(observer, "blocklist-updated");
    if (aCallback)
      executeSoon(aCallback);
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

function resetBlocklist(aCallback) {
  // XXX - this has "forked" from the head.js helpers in our parent directory :(
  // But let's reuse their blockNoPlugins.xml.  Later, we should arrange to
  // use their head.js helpers directly
  let noBlockedURL = "http://example.com/browser/browser/base/content/test/plugins/blockNoPlugins.xml";
  setAndUpdateBlocklist(noBlockedURL, function() {
    Services.prefs.setCharPref("extensions.blocklist.url", _originalTestBlocklistURL);
    if (aCallback)
      aCallback();
  });
}

function setManifestPref(name, manifest) {
  let string = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
  string.data = JSON.stringify(manifest);
  Services.prefs.setComplexValue(name, Ci.nsISupportsString, string);
}

function getManifestPrefname(aManifest) {
  // is same as the generated name in SocialServiceInternal.getManifestPrefname
  let originUri = Services.io.newURI(aManifest.origin, null, null);
  return "social.manifest." + originUri.hostPort.replace('.','-');
}

function setBuiltinManifestPref(name, manifest) {
  // we set this as a default pref, it must not be a user pref
  manifest.builtin = true;
  let string = Cc["@mozilla.org/supports-string;1"].
               createInstance(Ci.nsISupportsString);
  string.data = JSON.stringify(manifest);
  Services.prefs.getDefaultBranch(null).setComplexValue(name, Ci.nsISupportsString, string);
  // verify this is set on the default branch
  let stored = Services.prefs.getComplexValue(name, Ci.nsISupportsString).data;
  is(stored, string.data, "manifest '"+name+"' stored in default prefs");
  // don't dirty our manifest, we'll need it without this flag later
  delete manifest.builtin;
  // verify we DO NOT have a user-level pref
  ok(!Services.prefs.prefHasUserValue(name), "manifest '"+name+"' is not in user-prefs");
}

function resetBuiltinManifestPref(name) {
  Services.prefs.getDefaultBranch(null).deleteBranch(name);
  is(Services.prefs.getDefaultBranch(null).getPrefType(name),
     Services.prefs.PREF_INVALID, "default manifest removed");
}

function addTab(url, callback) {
  let tab = gBrowser.selectedTab = gBrowser.addTab(url, {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    executeSoon(function() {callback(tab)});
  }, true);
}

function selectBrowserTab(tab, callback) {
  if (gBrowser.selectedTab == tab) {
    executeSoon(function() {callback(tab)});
    return;
  }
  gBrowser.tabContainer.addEventListener("TabSelect", function onTabSelect() {
    gBrowser.tabContainer.removeEventListener("TabSelect", onTabSelect, false);
    is(gBrowser.selectedTab, tab, "browser tab is selected");
    executeSoon(function() {callback(tab)});
  });
  gBrowser.selectedTab = tab;
}

function ensureEventFired(elem, event) {
  let deferred = Promise.defer();
  elem.addEventListener(event, function handler() {
    elem.removeEventListener(event, handler, true);
    deferred.resolve()
  }, true);
  return deferred.promise;
}

function loadIntoTab(tab, url, callback) {
  tab.linkedBrowser.addEventListener("load", function tabLoad(event) {
    tab.linkedBrowser.removeEventListener("load", tabLoad, true);
    executeSoon(function() {callback(tab)});
  }, true);
  tab.linkedBrowser.loadURI(url);
}

function ensureBrowserTabClosed(tab) {
  let promise = ensureEventFired(gBrowser.tabContainer, "TabClose");
  gBrowser.removeTab(tab);
  return promise;
}

function ensureFrameLoaded(frame) {
  let deferred = Promise.defer();
  if (frame.contentDocument && frame.contentDocument.readyState == "complete") {
    deferred.resolve();
  } else {
    frame.addEventListener("load", function handler() {
      frame.removeEventListener("load", handler, true);
      deferred.resolve()
    }, true);
  }
  return deferred.promise;
}

// chat test help functions

// And lots of helpers for the resize tests.
function get3ChatsForCollapsing(mode, cb) {
  // We make one chat, then measure its size.  We then resize the browser to
  // ensure a second can be created fully visible but a third can not - then
  // create the other 2.  first will will be collapsed, second fully visible
  // and the third also visible and the "selected" one.
  // To make our life easier we don't go via the worker and ports so we get
  // more control over creation *and* to make the code much simpler.  We
  // assume the worker/port stuff is individually tested above.
  let chatbar = getChatBar();
  let chatWidth = undefined;
  let num = 0;
  is(chatbar.childNodes.length, 0, "chatbar starting empty");
  is(chatbar.menupopup.childNodes.length, 0, "popup starting empty");

  makeChat(mode, "first chat", function() {
    // got the first one.
    checkPopup();
    ok(chatbar.menupopup.parentNode.collapsed, "menu selection isn't visible");
    // we kinda cheat here and get the width of the first chat, assuming
    // that all future chats will have the same width when open.
    chatWidth = chatbar.calcTotalWidthOf(chatbar.selectedChat);
    let desired = chatWidth * 2.5;
    resizeWindowToChatAreaWidth(desired, function(sizedOk) {
      ok(sizedOk, "can't do any tests without this width");
      checkPopup();
      makeChat(mode, "second chat", function() {
        is(chatbar.childNodes.length, 2, "now have 2 chats");
        checkPopup();
        // and create the third.
        makeChat(mode, "third chat", function() {
          is(chatbar.childNodes.length, 3, "now have 3 chats");
          checkPopup();
          // XXX - this is a hacky implementation detail around the order of
          // the chats.  Ideally things would be a little more sane wrt the
          // other in which the children were created.
          let second = chatbar.childNodes[2];
          let first = chatbar.childNodes[1];
          let third = chatbar.childNodes[0];
          is(first.collapsed, true, "first collapsed state as promised");
          is(second.collapsed, false, "second collapsed state as promised");
          is(third.collapsed, false, "third collapsed state as promised");
          is(chatbar.selectedChat, third, "third is selected as promised")
          info("have 3 chats for collapse testing - starting actual test...");
          cb(first, second, third);
        }, mode);
      }, mode);
    });
  }, mode);
}

function makeChat(mode, uniqueid, cb) {
  info("making a chat window '" + uniqueid +"'");
  let provider = SocialSidebar.provider;
  let chatUrl = provider.origin + "/browser/browser/base/content/test/social/social_chat.html";
  // chatURL is not a part of the provider class, but is added by tests if we
  // want to use a specific url (different than above) for testing
  if (provider.chatURL) {
    chatUrl = provider.chatURL;
  }
  // Note that we use promiseChatLoaded instead of the callback to ensure the
  // content has started loading.
  let chatbox = getChatBar().openChat(provider.origin, provider.name,
                                      chatUrl + "?id=" + uniqueid, mode);
  chatbox.promiseChatLoaded.then(
    () => {
    info("chat window has opened");
    chatbox.contentDocument.title = uniqueid;
    cb();
  });
}

function checkPopup() {
  // popup only showing if any collapsed popup children.
  let chatbar = getChatBar();
  let numCollapsed = 0;
  for (let chat of chatbar.childNodes) {
    if (chat.collapsed) {
      numCollapsed += 1;
      // and it have a menuitem weakmap
      is(chatbar.menuitemMap.get(chat).nodeName, "menuitem", "collapsed chat has a menu item");
    } else {
      ok(!chatbar.menuitemMap.has(chat), "open chat has no menu item");
    }
  }
  is(chatbar.menupopup.parentNode.collapsed, numCollapsed == 0, "popup matches child collapsed state");
  is(chatbar.menupopup.childNodes.length, numCollapsed, "popup has correct count of children");
  // todo - check each individual elt is what we expect?
}
// Resize the main window so the chat area's boxObject is |desired| wide.
// Does a callback passing |true| if the window is now big enough or false
// if we couldn't resize large enough to satisfy the test requirement.
function resizeWindowToChatAreaWidth(desired, cb, count = 0) {
  let current = getChatBar().getBoundingClientRect().width;
  let delta = desired - current;
  info(count + ": resizing window so chat area is " + desired + " wide, currently it is "
       + current + ".  Screen avail is " + window.screen.availWidth
       + ", current outer width is " + window.outerWidth);

  // WTF?  Sometimes we will get fractional values due to the - err - magic
  // of DevPointsPerCSSPixel etc, so we allow a couple of pixels difference.
  let widthDeltaCloseEnough = function(d) {
    return Math.abs(d) < 2;
  }

  // attempting to resize by (0,0), unsurprisingly, doesn't cause a resize
  // event - so just callback saying all is well.
  if (widthDeltaCloseEnough(delta)) {
    info(count + ": skipping this as screen width is close enough");
    executeSoon(function() {
      cb(true);
    });
    return;
  }
  // On lo-res screens we may already be maxed out but still smaller than the
  // requested size, so asking to resize up also will not cause a resize event.
  // So just callback now saying the test must be skipped.
  if (window.screen.availWidth - window.outerWidth < delta) {
    info(count + ": skipping this as screen available width is less than necessary");
    executeSoon(function() {
      cb(false);
    });
    return;
  }
  function resize_handler(event) {
    // we did resize - but did we get far enough to be able to continue?
    let newSize = getChatBar().getBoundingClientRect().width;
    let sizedOk = widthDeltaCloseEnough(newSize - desired);
    if (!sizedOk)
      return;
    window.removeEventListener("resize", resize_handler, true);
    info(count + ": resized window width is " + newSize);
    executeSoon(function() {
      cb(sizedOk);
    });
  }
  // Otherwise we request resize and expect a resize event
  window.addEventListener("resize", resize_handler, true);
  window.resizeBy(delta, 0);
}

function resizeAndCheckWidths(first, second, third, checks, cb) {
  if (checks.length == 0) {
    cb(); // nothing more to check!
    return;
  }
  let count = checks.length;
  let [width, numExpectedVisible, why] = checks.shift();
  info("<< Check " + count + ": " + why);
  info(count + ": " + "resizing window to " + width + ", expect " + numExpectedVisible + " visible items");
  resizeWindowToChatAreaWidth(width, function(sizedOk) {
    checkPopup();
    ok(sizedOk, count+": window resized correctly");
    function collapsedObserver(r, m) {
      if ([first, second, third].filter(function(item) !item.collapsed).length == numExpectedVisible) {
        if (m) {
          m.disconnect();
        }
        ok(true, count + ": " + "correct number of chats visible");
        info(">> Check " + count);
        executeSoon(function() {
          resizeAndCheckWidths(first, second, third, checks, cb);
        });
      }
    }
    let m = new MutationObserver(collapsedObserver);
    m.observe(first, {attributes: true });
    m.observe(second, {attributes: true });
    m.observe(third, {attributes: true });
    // and just in case we are already at the right size, explicitly call the
    // observer.
    collapsedObserver(undefined, m);
  }, count);
}

function getChatBar() {
  let cb = document.getElementById("pinnedchats");
  cb.hidden = false;
  return cb;
}

function getPopupWidth() {
  let chatbar = getChatBar();
  let popup = chatbar.menupopup;
  ok(!popup.parentNode.collapsed, "asking for popup width when it is visible");
  let cs = document.defaultView.getComputedStyle(popup.parentNode);
  let margins = parseInt(cs.marginLeft) + parseInt(cs.marginRight);
  return popup.parentNode.getBoundingClientRect().width + margins;
}

function promiseCloseChat(chat) {
  let deferred = Promise.defer();
  let parent = chat.parentNode;

  let observer = new MutationObserver(function onMutatations(mutations) {
    for (let mutation of mutations) {
      for (let i = 0; i < mutation.removedNodes.length; i++) {
        let node = mutation.removedNodes.item(i);
        if (node != chat) {
          continue;
        }
        observer.disconnect();
        deferred.resolve();
      }
    }
  });
  observer.observe(parent, {childList: true});
  chat.close();
  return deferred.promise;
}

function closeAllChats() {
  let chatbar = getChatBar();
  while (chatbar.selectedChat) {
    yield promiseCloseChat(chatbar.selectedChat);
  }
}


// Support for going on and offline.
// (via browser/base/content/test/browser_bookmark_titles.js)
let origProxyType = Services.prefs.getIntPref('network.proxy.type');

function toggleOfflineStatus(goOffline) {
  // Bug 968887 fix.  when going on/offline, wait for notification before continuing
  let deferred = Promise.defer();
  if (!goOffline) {
    Services.prefs.setIntPref('network.proxy.type', origProxyType);
  }
  if (goOffline != Services.io.offline) {
    info("initial offline state " + Services.io.offline);
    let expect = !Services.io.offline;
    Services.obs.addObserver(function offlineChange(subject, topic, data) {
      Services.obs.removeObserver(offlineChange, "network:offline-status-changed");
      info("offline state changed to " + Services.io.offline);
      is(expect, Services.io.offline, "network:offline-status-changed successful toggle");
      deferred.resolve();
    }, "network:offline-status-changed", false);
    BrowserOffline.toggleOfflineStatus();
  } else {
    deferred.resolve();
  }
  if (goOffline) {
    Services.prefs.setIntPref('network.proxy.type', 0);
    // LOAD_FLAGS_BYPASS_CACHE isn't good enough. So clear the cache.
    Services.cache2.clear();
  }
  return deferred.promise;
}

function goOffline() {
  // Simulate a network outage with offline mode. (Localhost is still
  // accessible in offline mode, so disable the test proxy as well.)
  return toggleOfflineStatus(true);
}

function goOnline(callback) {
  return toggleOfflineStatus(false);
}
