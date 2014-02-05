/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci } = require('chrome');
const { Loader } = require('sdk/test/loader');
const timer = require('sdk/timers');
const { getOwnerWindow } = require('sdk/private-browsing/window/utils');
const { windows, onFocus, getMostRecentBrowserWindow } = require('sdk/window/utils');
const { open, focus, close } = require('sdk/window/helpers');
const tabs = require('sdk/tabs');
const { browserWindows } = require('sdk/windows');
const { set: setPref } = require("sdk/preferences/service");
const DEPRECATE_PREF = "devtools.errorconsole.deprecation_warnings";

const base64png = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQImWNgYGBgAA";

// Bug 682681 - tab.title should never be empty
exports.testBug682681_aboutURI = function(assert, done) {
  let url = 'chrome://browser/locale/tabbrowser.properties';
  let stringBundle = Cc["@mozilla.org/intl/stringbundle;1"].
                        getService(Ci.nsIStringBundleService).
                        createBundle(url);
  let emptyTabTitle = stringBundle.GetStringFromName('tabs.emptyTabTitle');

  tabs.on('ready', function onReady(tab) {
    tabs.removeListener('ready', onReady);

    assert.equal(tab.title,
                     emptyTabTitle,
                     "title of about: tab is not blank");

    tab.close(done);
  });

  // open a about: url
  tabs.open({
    url: "about:blank",
    inBackground: true
  });
};

// related to Bug 682681
exports.testTitleForDataURI = function(assert, done) {
  tabs.open({
    url: "data:text/html;charset=utf-8,<title>tab</title>",
    inBackground: true,
    onReady: function(tab) {
      assert.equal(tab.title, "tab", "data: title is not Connecting...");
      tab.close(done);
    }
  });
};

// TEST: 'BrowserWindow' instance creation on tab 'activate' event
// See bug 648244: there was a infinite loop.
exports.testBrowserWindowCreationOnActivate = function(assert, done) {
  let windows = require("sdk/windows").browserWindows;
  let gotActivate = false;

  tabs.once('activate', function onActivate(eventTab) {
    assert.ok(windows.activeWindow, "Is able to fetch activeWindow");
    gotActivate = true;
  });

  open().then(function(window) {
    assert.ok(gotActivate, "Received activate event before openBrowserWindow's callback is called");
    close(window).then(done);
  });
}

// TEST: tab unloader
exports.testAutomaticDestroy = function(assert, done) {
  // Create a second tab instance that we will destroy
  let called = false;

  let loader = Loader(module);
  let tabs2 = loader.require("sdk/tabs");
  tabs2.on('open', function onOpen(tab) {
    called = true;
  });

  loader.unload();

  // Fire a tab event and ensure that the destroyed tab is inactive
  tabs.once('open', function (tab) {
    timer.setTimeout(function () {
      assert.ok(!called, "Unloaded tab module is destroyed and inactive");
      tab.close(done);
    });
  });
  tabs.open("data:text/html;charset=utf-8,foo");
};

exports.testTabPropertiesInNewWindow = function(assert, done) {
  let warning = "DEPRECATED: tab.favicon is deprecated, please use require(\"sdk/places/favicon\").getFavicon instead.\n"
  const { LoaderWithFilteredConsole } = require("sdk/test/loader");
  let loader = LoaderWithFilteredConsole(module, function(type, message) {
    if (type == "error" && message.substring(0, warning.length) == warning)
      return false;
    return true;
  });

  let tabs = loader.require('sdk/tabs');
  let { getOwnerWindow } = loader.require('sdk/private-browsing/window/utils');

  let count = 0;
  function onReadyOrLoad (tab) {
    if (count++) {
      close(getOwnerWindow(tab)).then(done);
    }
  }

  let url = "data:text/html;charset=utf-8,<html><head><title>foo</title></head><body>foo</body></html>";
  tabs.open({
    inNewWindow: true,
    url: url,
    onReady: function(tab) {
      assert.equal(tab.title, "foo", "title of the new tab matches");
      assert.equal(tab.url, url, "URL of the new tab matches");
      assert.ok(tab.favicon, "favicon of the new tab is not empty");
      assert.equal(tab.style, null, "style of the new tab matches");
      assert.equal(tab.index, 0, "index of the new tab matches");
      assert.notEqual(tab.getThumbnail(), null, "thumbnail of the new tab matches");
      assert.notEqual(tab.id, null, "a tab object always has an id property.");

      onReadyOrLoad(tab);
    },
    onLoad: function(tab) {
      assert.equal(tab.title, "foo", "title of the new tab matches");
      assert.equal(tab.url, url, "URL of the new tab matches");
      assert.ok(tab.favicon, "favicon of the new tab is not empty");
      assert.equal(tab.style, null, "style of the new tab matches");
      assert.equal(tab.index, 0, "index of the new tab matches");
      assert.notEqual(tab.getThumbnail(), null, "thumbnail of the new tab matches");
      assert.notEqual(tab.id, null, "a tab object always has an id property.");

      onReadyOrLoad(tab);
    }
  });
};

exports.testTabPropertiesInSameWindow = function(assert, done) {
  let warning = "DEPRECATED: tab.favicon is deprecated, please use require(\"sdk/places/favicon\").getFavicon instead.\n"
  const { LoaderWithFilteredConsole } = require("sdk/test/loader");
  let loader = LoaderWithFilteredConsole(module, function(type, message) {
    if (type == "error" && message.substring(0, warning.length) == warning)
      return false;
    return true;
  });

  let tabs = loader.require('sdk/tabs');

  // Get current count of tabs so we know the index of the
  // new tab, bug 893846
  let tabCount = tabs.length;
  let count = 0;
  function onReadyOrLoad (tab) {
    if (count++) {
      tab.close(done);
    }
  }

  let url = "data:text/html;charset=utf-8,<html><head><title>foo</title></head><body>foo</body></html>";
  tabs.open({
    url: url,
    onReady: function(tab) {
      assert.equal(tab.title, "foo", "title of the new tab matches");
      assert.equal(tab.url, url, "URL of the new tab matches");
      assert.ok(tab.favicon, "favicon of the new tab is not empty");
      assert.equal(tab.style, null, "style of the new tab matches");
      assert.equal(tab.index, tabCount, "index of the new tab matches");
      assert.notEqual(tab.getThumbnail(), null, "thumbnail of the new tab matches");
      assert.notEqual(tab.id, null, "a tab object always has an id property.");

      onReadyOrLoad(tab);
    },
    onLoad: function(tab) {
      assert.equal(tab.title, "foo", "title of the new tab matches");
      assert.equal(tab.url, url, "URL of the new tab matches");
      assert.ok(tab.favicon, "favicon of the new tab is not empty");
      assert.equal(tab.style, null, "style of the new tab matches");
      assert.equal(tab.index, tabCount, "index of the new tab matches");
      assert.notEqual(tab.getThumbnail(), null, "thumbnail of the new tab matches");
      assert.notEqual(tab.id, null, "a tab object always has an id property.");

      onReadyOrLoad(tab);
    }
  });
};

// TEST: tab properties
exports.testTabContentTypeAndReload = function(assert, done) {
  open().then(focus).then(function(window) {
    let url = "data:text/html;charset=utf-8,<html><head><title>foo</title></head><body>foo</body></html>";
    let urlXML = "data:text/xml;charset=utf-8,<foo>bar</foo>";
    tabs.open({
      url: url,
      onReady: function(tab) {
        if (tab.url === url) {
          assert.equal(tab.contentType, "text/html");
          tab.url = urlXML;
        }
        else {
          assert.equal(tab.contentType, "text/xml");
          close(window).then(done);
        }
      }
    });
  });
};

// TEST: tabs iterator and length property
exports.testTabsIteratorAndLength = function(assert, done) {
  open(null, { features: { chrome: true, toolbar: true } }).then(focus).then(function(window) {
    let startCount = 0;
    for each (let t in tabs) startCount++;
    assert.equal(startCount, tabs.length, "length property is correct");
    let url = "data:text/html;charset=utf-8,default";

    tabs.open(url);
    tabs.open(url);
    tabs.open({
      url: url,
      onOpen: function(tab) {
        let count = 0;
        for each (let t in tabs) count++;
        assert.equal(count, startCount + 3, "iterated tab count matches");
        assert.equal(startCount + 3, tabs.length, "iterated tab count matches length property");

        close(window).then(done);
      }
    });
  });
};

// TEST: tab.url setter
exports.testTabLocation = function(assert, done) {
  open().then(focus).then(function(window) {
    let url1 = "data:text/html;charset=utf-8,foo";
    let url2 = "data:text/html;charset=utf-8,bar";

    tabs.on('ready', function onReady(tab) {
      if (tab.url != url2)
        return;
      tabs.removeListener('ready', onReady);
      assert.pass("tab.load() loaded the correct url");
      close(window).then(done);
    });

    tabs.open({
      url: url1,
      onOpen: function(tab) {
        tab.url = url2
      }
    });
  });
};

// TEST: tab.close()
exports.testTabClose = function(assert, done) {
  let url = "data:text/html;charset=utf-8,foo";

  assert.notEqual(tabs.activeTab.url, url, "tab is not the active tab");
  tabs.on('ready', function onReady(tab) {
    tabs.removeListener('ready', onReady);
    assert.equal(tabs.activeTab.url, tab.url, "tab is now the active tab");
    let secondOnCloseCalled = false;

    // Bug 699450: Multiple calls to tab.close should not throw
    tab.close(function() secondOnCloseCalled = true);
    try {
      tab.close(function () {
        assert.ok(secondOnCloseCalled,
          "The immediate second call to tab.close gots its callback fired");
        assert.notEqual(tabs.activeTab.url, url, "tab is no longer the active tab");

        done();
      });
    }
    catch(e) {
      assert.fail("second call to tab.close() thrown an exception: " + e);
    }
    assert.notEqual(tabs.activeTab.url, url, "tab is no longer the active tab");
  });

  tabs.open(url);
};

// TEST: tab.move()
exports.testTabMove = function(assert, done) {
  open().then(focus).then(function(window) {
    let url = "data:text/html;charset=utf-8,foo";

    tabs.open({
      url: url,
      onOpen: function(tab) {
        assert.equal(tab.index, 1, "tab index before move matches");
        tab.index = 0;
        assert.equal(tab.index, 0, "tab index after move matches");
        close(window).then(done);
      }
    });
  });
};

// TEST: open tab with default options
exports.testOpen = function(assert, done) {
  let url = "data:text/html;charset=utf-8,default";
  tabs.open({
    url: url,
    onReady: function(tab) {
      assert.equal(tab.url, url, "URL of the new tab matches");
      assert.equal(tab.isPinned, false, "The new tab is not pinned");

      tab.close(done);
    }
  });
};

// TEST: opening a pinned tab
exports.testOpenPinned = function(assert, done) {
  let url = "data:text/html;charset=utf-8,default";
  tabs.open({
    url: url,
    isPinned: true,
    onOpen: function(tab) {
      assert.equal(tab.isPinned, true, "The new tab is pinned");
      tab.close(done);
    }
  });
};

// TEST: pin/unpin opened tab
exports.testPinUnpin = function(assert, done) {
  let url = "data:text/html;charset=utf-8,default";
  tabs.open({
    url: url,
    inBackground: true,
    onOpen: function(tab) {
      tab.pin();
      assert.equal(tab.isPinned, true, "The tab was pinned correctly");
      tab.unpin();
      assert.equal(tab.isPinned, false, "The tab was unpinned correctly");
      tab.close(done);
    }
  });
}

// TEST: open tab in background
exports.testInBackground = function(assert, done) {
  let window = getMostRecentBrowserWindow();
  let activeUrl = tabs.activeTab.url;
  let url = "data:text/html;charset=utf-8,background";
  assert.equal(activeWindow, window, "activeWindow matches this window");
  tabs.on('ready', function onReady(tab) {
    tabs.removeListener('ready', onReady);
    assert.equal(tabs.activeTab.url, activeUrl, "URL of active tab has not changed");
    assert.equal(tab.url, url, "URL of the new background tab matches");
    assert.equal(activeWindow, window, "a new window was not opened");
    assert.notEqual(tabs.activeTab.url, url, "URL of active tab is not the new URL");
    tab.close(done);
  });

  tabs.open({
    url: url,
    inBackground: true
  });
}

// TEST: open tab in new window
exports.testOpenInNewWindow = function(assert, done) {
  let startWindowCount = windows().length;

  let url = "data:text/html;charset=utf-8,testOpenInNewWindow";
  tabs.open({
    url: url,
    inNewWindow: true,
    onReady: function(tab) {
      let newWindow = getOwnerWindow(tab);
      assert.equal(windows().length, startWindowCount + 1, "a new window was opened");

      onFocus(newWindow).then(function() {
        assert.equal(activeWindow, newWindow, "new window is active");
        assert.equal(tab.url, url, "URL of the new tab matches");
        assert.equal(newWindow.content.location, url, "URL of new tab in new window matches");
        assert.equal(tabs.activeTab.url, url, "URL of activeTab matches");

        close(newWindow).then(done);
      }, assert.fail).then(null, assert.fail);
    }
  });

}

// Test tab.open inNewWindow + onOpen combination
exports.testOpenInNewWindowOnOpen = function(assert, done) {
  let startWindowCount = windows().length;

  let url = "data:text/html;charset=utf-8,newwindow";
  tabs.open({
    url: url,
    inNewWindow: true,
    onOpen: function(tab) {
      let newWindow = getOwnerWindow(tab);

      onFocus(newWindow).then(function() {
        assert.equal(windows().length, startWindowCount + 1, "a new window was opened");
        assert.equal(activeWindow, newWindow, "new window is active");

        close(newWindow).then(done);
      });
    }
  });
};

// TEST: onOpen event handler
exports.testTabsEvent_onOpen = function(assert, done) {
  openBrowserWindow(function(window, browser) {
    let url = "data:text/html;charset=utf-8,1";
    let eventCount = 0;

    // add listener via property assignment
    function listener1(tab) {
      eventCount++;
    };
    tabs.on('open', listener1);

    // add listener via collection add
    tabs.on('open', function listener2(tab) {
      assert.equal(++eventCount, 2, "both listeners notified");
      tabs.removeListener('open', listener1);
      tabs.removeListener('open', listener2);
      close(window).then(done);
    });

    tabs.open(url);
  });
};

// TEST: onClose event handler
exports.testTabsEvent_onClose = function(assert, done) {
  openBrowserWindow(function(window, browser) {
    let url = "data:text/html;charset=utf-8,onclose";
    let eventCount = 0;

    // add listener via property assignment
    function listener1(tab) {
      eventCount++;
    }
    tabs.on('close', listener1);

    // add listener via collection add
    tabs.on('close', function listener2(tab) {
      assert.equal(++eventCount, 2, "both listeners notified");
      tabs.removeListener('close', listener1);
      tabs.removeListener('close', listener2);
      close(window).then(done);
    });

    tabs.on('ready', function onReady(tab) {
      tabs.removeListener('ready', onReady);
      tab.close();
    });

    tabs.open(url);
  });
};

// TEST: onClose event handler when a window is closed
exports.testTabsEvent_onCloseWindow = function(assert, done) {
  let closeCount = 0;
  let individualCloseCount = 0;

  openBrowserWindow(function(window) {
    tabs.on("close", function listener() {
      if (++closeCount == 4) {
        tabs.removeListener("close", listener);
      }
    });

    function endTest() {
      if (++individualCloseCount < 3) {
        return;
      }

      assert.equal(closeCount, 4, "Correct number of close events received");
      assert.equal(individualCloseCount, 3,
                   "Each tab with an attached onClose listener received a close " +
                   "event when the window was closed");

      done();
    }

    // One tab is already open with the window
    let openTabs = 1;
    function testCasePossiblyLoaded() {
      if (++openTabs == 4) {
        window.close();
      }
    }

    tabs.open({
      url: "data:text/html;charset=utf-8,tab2",
      onOpen: testCasePossiblyLoaded,
      onClose: endTest
    });

    tabs.open({
      url: "data:text/html;charset=utf-8,tab3",
      onOpen: testCasePossiblyLoaded,
      onClose: endTest
    });

    tabs.open({
      url: "data:text/html;charset=utf-8,tab4",
      onOpen: testCasePossiblyLoaded,
      onClose: endTest
    });
  });
}

// TEST: onReady event handler
exports.testTabsEvent_onReady = function(assert, done) {
  openBrowserWindow(function(window, browser) {
    let url = "data:text/html;charset=utf-8,onready";
    let eventCount = 0;

    // add listener via property assignment
    function listener1(tab) {
      eventCount++;
    };
    tabs.on('ready', listener1);

    // add listener via collection add
    tabs.on('ready', function listener2(tab) {
      assert.equal(++eventCount, 2, "both listeners notified");
      tabs.removeListener('ready', listener1);
      tabs.removeListener('ready', listener2);
      close(window).then(done);
    });

    tabs.open(url);
  });
};

// TEST: onActivate event handler
exports.testTabsEvent_onActivate = function(assert, done) {
  openBrowserWindow(function(window, browser) {
    let url = "data:text/html;charset=utf-8,onactivate";
    let eventCount = 0;

    // add listener via property assignment
    function listener1(tab) {
      eventCount++;
    };
    tabs.on('activate', listener1);

    // add listener via collection add
    tabs.on('activate', function listener2(tab) {
      assert.equal(++eventCount, 2, "both listeners notified");
      tabs.removeListener('activate', listener1);
      tabs.removeListener('activate', listener2);
      close(window).then(done);
    });

    tabs.open(url);
  });
};

// onDeactivate event handler
exports.testTabsEvent_onDeactivate = function(assert, done) {
  openBrowserWindow(function(window, browser) {
    let url = "data:text/html;charset=utf-8,ondeactivate";
    let eventCount = 0;

    // add listener via property assignment
    function listener1(tab) {
      eventCount++;
    };
    tabs.on('deactivate', listener1);

    // add listener via collection add
    tabs.on('deactivate', function listener2(tab) {
      assert.equal(++eventCount, 2, "both listeners notified");
      tabs.removeListener('deactivate', listener1);
      tabs.removeListener('deactivate', listener2);
      close(window).then(done);
    });

    tabs.on('open', function onOpen(tab) {
      tabs.removeListener('open', onOpen);
      tabs.open("data:text/html;charset=utf-8,foo");
    });

    tabs.open(url);
  });
};

// pinning
exports.testTabsEvent_pinning = function(assert, done) {
  openBrowserWindow(function(window, browser) {
    let url = "data:text/html;charset=utf-8,1";

    tabs.on('open', function onOpen(tab) {
      tabs.removeListener('open', onOpen);
      tab.pin();
    });

    tabs.on('pinned', function onPinned(tab) {
      tabs.removeListener('pinned', onPinned);
      assert.ok(tab.isPinned, "notified tab is pinned");
      tab.unpin();
    });

    tabs.on('unpinned', function onUnpinned(tab) {
      tabs.removeListener('unpinned', onUnpinned);
      assert.ok(!tab.isPinned, "notified tab is not pinned");
      close(window).then(done);
    });

    tabs.open(url);
  });
};

// TEST: per-tab event handlers
exports.testPerTabEvents = function(assert, done) {
  openBrowserWindow(function(window, browser) {
    let eventCount = 0;

    tabs.open({
      url: "data:text/html;charset=utf-8,foo",
      onOpen: function(tab) {
        // add listener via property assignment
        function listener1() {
          eventCount++;
        };
        tab.on('ready', listener1);

        // add listener via collection add
        tab.on('ready', function listener2() {
          assert.equal(eventCount, 1, "both listeners notified");
          tab.removeListener('ready', listener1);
          tab.removeListener('ready', listener2);
          close(window).then(done);
        });
      }
    });
  });
};

exports.testAttachOnOpen = function (assert, done) {
  // Take care that attach has to be called on tab ready and not on tab open.
  openBrowserWindow(function(window, browser) {
    tabs.open({
      url: "data:text/html;charset=utf-8,foobar",
      onOpen: function (tab) {
        let worker = tab.attach({
          contentScript: 'self.postMessage(document.location.href); ',
          onMessage: function (msg) {
            assert.equal(msg, "about:blank",
              "Worker document url is about:blank on open");
            worker.destroy();
            close(window).then(done);
          }
        });
      }
    });

  });
}

exports.testAttachOnMultipleDocuments = function (assert, done) {
  // Example of attach that process multiple tab documents
  openBrowserWindow(function(window, browser) {
    let firstLocation = "data:text/html;charset=utf-8,foobar";
    let secondLocation = "data:text/html;charset=utf-8,bar";
    let thirdLocation = "data:text/html;charset=utf-8,fox";
    let onReadyCount = 0;
    let worker1 = null;
    let worker2 = null;
    let detachEventCount = 0;

    tabs.open({
      url: firstLocation,
      onReady: function (tab) {
        onReadyCount++;
        if (onReadyCount == 1) {
          worker1 = tab.attach({
            contentScript: 'self.on("message", ' +
                           '  function () self.postMessage(document.location.href)' +
                           ');',
            onMessage: function (msg) {
              assert.equal(msg, firstLocation,
                               "Worker url is equal to the 1st document");
              tab.url = secondLocation;
            },
            onDetach: function () {
              detachEventCount++;
              assert.pass("Got worker1 detach event");
              assert.throws(function () {
                  worker1.postMessage("ex-1");
                },
                /Couldn't find the worker/,
                "postMessage throw because worker1 is destroyed");
              checkEnd();
            }
          });
          worker1.postMessage("new-doc-1");
        }
        else if (onReadyCount == 2) {

          worker2 = tab.attach({
            contentScript: 'self.on("message", ' +
                           '  function () self.postMessage(document.location.href)' +
                           ');',
            onMessage: function (msg) {
              assert.equal(msg, secondLocation,
                               "Worker url is equal to the 2nd document");
              tab.url = thirdLocation;
            },
            onDetach: function () {
              detachEventCount++;
              assert.pass("Got worker2 detach event");
              assert.throws(function () {
                  worker2.postMessage("ex-2");
                },
                /Couldn't find the worker/,
                "postMessage throw because worker2 is destroyed");
              checkEnd();
            }
          });
          worker2.postMessage("new-doc-2");
        }
        else if (onReadyCount == 3) {
          tab.close();
        }
      }
    });

    function checkEnd() {
      if (detachEventCount != 2)
        return;

      assert.pass("Got all detach events");

      close(window).then(done);
    }

  });
}


exports.testAttachWrappers = function (assert, done) {
  // Check that content script has access to wrapped values by default
  openBrowserWindow(function(window, browser) {
    let document = "data:text/html;charset=utf-8,<script>var globalJSVar = true; " +
                   "                       document.getElementById = 3;</script>";
    let count = 0;

    tabs.open({
      url: document,
      onReady: function (tab) {
        let worker = tab.attach({
          contentScript: 'try {' +
                         '  self.postMessage(!("globalJSVar" in window));' +
                         '  self.postMessage(typeof window.globalJSVar == "undefined");' +
                         '} catch(e) {' +
                         '  self.postMessage(e.message);' +
                         '}',
          onMessage: function (msg) {
            assert.equal(msg, true, "Worker has wrapped objects ("+count+")");
            if (count++ == 1)
              close(window).then(done);
          }
        });
      }
    });

  });
}

/*
// We do not offer unwrapped access to DOM since bug 601295 landed
// See 660780 to track progress of unwrap feature
exports.testAttachUnwrapped = function (assert, done) {
  // Check that content script has access to unwrapped values through unsafeWindow
  openBrowserWindow(function(window, browser) {
    let document = "data:text/html;charset=utf-8,<script>var globalJSVar=true;</script>";
    let count = 0;

    tabs.open({
      url: document,
      onReady: function (tab) {
        let worker = tab.attach({
          contentScript: 'try {' +
                         '  self.postMessage(unsafeWindow.globalJSVar);' +
                         '} catch(e) {' +
                         '  self.postMessage(e.message);' +
                         '}',
          onMessage: function (msg) {
            assert.equal(msg, true, "Worker has access to javascript content globals ("+count+")");
            close(window).then(done);
          }
        });
      }
    });

  });
}
*/

exports['test window focus changes active tab'] = function(assert, done) {
  let url1 = "data:text/html;charset=utf-8," + encodeURIComponent("test window focus changes active tab</br><h1>Window #1");

  let win1 = openBrowserWindow(function() {
    assert.pass("window 1 is open");

    let win2 = openBrowserWindow(function() {
      assert.pass("window 2 is open");

      focus(win2).then(function() {
        tabs.on("activate", function onActivate(tab) {
          tabs.removeListener("activate", onActivate);
          assert.pass("activate was called on windows focus change.");
          assert.equal(tab.url, url1, 'the activated tab url is correct');

          close(win2).then(function() {
            assert.pass('window 2 was closed');
            return close(win1);
          }).then(done);
        });

        win1.focus();
      });
    }, "data:text/html;charset=utf-8,test window focus changes active tab</br><h1>Window #2");
  }, url1);
};

exports['test ready event on new window tab'] = function(assert, done) {
  let uri = encodeURI("data:text/html;charset=utf-8,Waiting for ready event!");

  require("sdk/tabs").on("ready", function onReady(tab) {
    if (tab.url === uri) {
      require("sdk/tabs").removeListener("ready", onReady);
      assert.pass("ready event was emitted");
      close(window).then(done);
    }
  });

  let window = openBrowserWindow(function(){}, uri);
};

exports['test unique tab ids'] = function(assert, done) {
  var windows = require('sdk/windows').browserWindows;
  var { all, defer } = require('sdk/core/promise');

  function openWindow() {
    // console.log('in openWindow');
    let deferred = defer();
    let win = windows.open({
      url: "data:text/html;charset=utf-8,<html>foo</html>",
    });

    win.on('open', function(window) {
      assert.ok(window.tabs.length);
      assert.ok(window.tabs.activeTab);
      assert.ok(window.tabs.activeTab.id);
      deferred.resolve({
        id: window.tabs.activeTab.id,
        win: win
      });
    });

    return deferred.promise;
  }

  var one = openWindow(), two = openWindow();
  all([one, two]).then(function(results) {
    assert.notEqual(results[0].id, results[1].id, "tab Ids should not be equal.");
    results[0].win.close();
    results[1].win.close();
    done();
  });
}

// related to Bug 671305
exports.testOnLoadEventWithDOM = function(assert, done) {
  let count = 0;
  let title = 'testOnLoadEventWithDOM';

  // open a about: url
  tabs.open({
    url: 'data:text/html;charset=utf-8,<title>' + title + '</title>',
    inBackground: true,
    onLoad: function(tab) {
      assert.equal(tab.title, title, 'tab passed in as arg, load called');

      if (++count > 1) {
        assert.pass('onLoad event called on reload');
        tab.close(done);
      }
      else {
        assert.pass('first onLoad event occured');
        tab.reload();
      }
    }
  });
};

// related to Bug 671305
exports.testOnLoadEventWithImage = function(assert, done) {
  let count = 0;

  tabs.open({
    url: base64png,
    inBackground: true,
    onLoad: function(tab) {
      if (++count > 1) {
        assert.pass('onLoad event called on reload with image');
        tab.close(done);
      }
      else {
        assert.pass('first onLoad event occured');
        tab.reload();
      }
    }
  });
};

exports.testFaviconGetterDeprecation = function (assert, done) {
  setPref(DEPRECATE_PREF, true);
  const { LoaderWithHookedConsole } = require("sdk/test/loader");
  let { loader, messages } = LoaderWithHookedConsole(module);
  let tabs = loader.require('sdk/tabs');

  tabs.open({
    url: 'data:text/html;charset=utf-8,',
    onOpen: function (tab) {
      let favicon = tab.favicon;
      assert.ok(messages.length === 1, 'only one error is dispatched');
      assert.ok(messages[0].type, 'error', 'the console message is an error');

      let msg = messages[0].msg;
      assert.ok(msg.indexOf('tab.favicon is deprecated') !== -1,
        'message contains the given message');
      tab.close(done);
      loader.unload();
    }
  });
}

/******************* helpers *********************/

// Helper for getting the active window
this.__defineGetter__("activeWindow", function activeWindow() {
  return Cc["@mozilla.org/appshell/window-mediator;1"].
         getService(Ci.nsIWindowMediator).
         getMostRecentWindow("navigator:browser");
});

// Utility function to open a new browser window.
function openBrowserWindow(callback, url) {
  let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  let urlString = Cc["@mozilla.org/supports-string;1"].
                  createInstance(Ci.nsISupportsString);
  urlString.data = url;
  let window = ww.openWindow(null, "chrome://browser/content/browser.xul",
                             "_blank", "chrome,all,dialog=no", urlString);

  if (callback) {
    window.addEventListener("load", function onLoad(event) {
      if (event.target && event.target.defaultView == window) {
        window.removeEventListener("load", onLoad, true);
        let browsers = window.document.getElementsByTagName("tabbrowser");
        try {
          timer.setTimeout(function () {
            callback(window, browsers[0]);
          }, 10);
        }
        catch (e) {
          console.exception(e);
        }
      }
    }, true);
  }

  return window;
}

require('sdk/test').run(exports);
