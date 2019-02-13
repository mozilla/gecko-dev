/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  engines: {
    'Firefox': '*'
  }
};

const windowUtils = require("sdk/deprecated/window-utils");
const timer = require("sdk/timers");
const { Cc, Ci } = require("chrome");
const { Loader } = require("sdk/test/loader");
const { open, getFrames, getWindowTitle, onFocus, windows } = require('sdk/window/utils');
const { close } = require('sdk/window/helpers');
const { fromIterator: toArray } = require('sdk/util/array');

const WM = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);

function makeEmptyWindow(options) {
  options = options || {};
  var xulNs = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  var blankXul = ('<?xml version="1.0"?>' +
                  '<?xml-stylesheet href="chrome://global/skin/" ' +
                  '                 type="text/css"?>' +
                  '<window xmlns="' + xulNs + '" windowtype="test:window">' +
                  '</window>');

  return open("data:application/vnd.mozilla.xul+xml;charset=utf-8," + escape(blankXul), {
    features: {
      chrome: true,
      width: 10,
      height: 10
    }
  });
}

exports.testWindowTracker = function(assert, done) {
  var myWindow = makeEmptyWindow();
  assert.pass('window was created');

  myWindow.addEventListener("load", function onload() {
    myWindow.removeEventListener("load", onload, false);
    assert.pass("test window has opened");

    // test bug 638007 (new is optional), using new
    var wt = new windowUtils.WindowTracker({
      onTrack: window => {
        if (window === myWindow) {
          assert.pass("onTrack() called with our test window");
          close(window);
        }
      },
      onUntrack: window => {
        if (window === myWindow) {
          assert.pass("onUntrack() called with our test window");
          wt.unload();
          timer.setTimeout(done);
        }
      }
    });
  }, false);
};

exports['test window watcher untracker'] = function(assert, done) {
  var myWindow;
  var tracks = 0;
  var unloadCalled = false;

  var delegate = {
    onTrack: function(window) {
      tracks = tracks + 1;
      if (window == myWindow) {
        assert.pass("onTrack() called with our test window");
        timer.setTimeout(function() {
          myWindow.close();
        }, 1);
      }
    },
    onUntrack: function(window) {
      tracks = tracks - 1;
      if (window == myWindow && !unloadCalled) {
        unloadCalled = true;
        timer.setTimeout(function() {
          wt.unload();
        }, 1);
      }
      if (0 > tracks) {
        assert.fail("WindowTracker onUntrack was called more times than onTrack..");
      }
      else if (0 == tracks) {
        timer.setTimeout(function() {
            myWindow = null;
            done();
        }, 1);
      }
    }
  };

  // test bug 638007 (new is optional), not using new
  var wt = windowUtils.WindowTracker(delegate);
  myWindow = makeEmptyWindow();
};

// test that _unregWindow calls _unregLoadingWindow
exports['test window watcher unregs 4 loading wins'] = function(assert, done) {
  var myWindow;
  var finished = false;
  let browserWindow =  WM.getMostRecentWindow("navigator:browser");
  var counter = 0;

  var delegate = {
    onTrack: function(window) {
      var type = window.document.documentElement.getAttribute("windowtype");
      if (type == "test:window")
        assert.fail("onTrack shouldn't have been executed.");
    }
  };
  var wt = new windowUtils.WindowTracker(delegate);

  // make a new window
  myWindow = makeEmptyWindow();

  // make sure that the window hasn't loaded yet
  assert.notEqual(
      myWindow.document.readyState,
      "complete",
      "window hasn't loaded yet.");

  // unload WindowTracker
  wt.unload();

  // make sure that the window still hasn't loaded, which means that the onTrack
  // would have been removed successfully assuming that it doesn't execute.
  assert.notEqual(
      myWindow.document.readyState,
      "complete",
      "window still hasn't loaded yet.");

  // wait for the window to load and then close it. onTrack wouldn't be called
  // until the window loads, so we must let it load before closing it to be
  // certain that onTrack was removed.
  myWindow.addEventListener("load", function() {
    // allow all of the load handles to execute before closing
    myWindow.setTimeout(function() {
      myWindow.addEventListener("unload", function() {
        // once the window unloads test is done
        done();
      }, false);
      myWindow.close();
    }, 0);
  }, false);
}

exports['test window watcher without untracker'] = function(assert, done) {
  let myWindow;
  let wt = new windowUtils.WindowTracker({
    onTrack: function(window) {
      if (window == myWindow) {
        assert.pass("onTrack() called with our test window");

        close(myWindow).then(function() {
          wt.unload();
          done();
        }, assert.fail);
      }
    }
  });

  myWindow = makeEmptyWindow();
};

exports['test active window'] = function(assert, done) {
  let browserWindow = WM.getMostRecentWindow("navigator:browser");
  let continueAfterFocus = function(window) onFocus(window).then(nextTest);

  assert.equal(windowUtils.activeBrowserWindow, browserWindow,
               "Browser window is the active browser window.");


  let testSteps = [
    function() {
      continueAfterFocus(windowUtils.activeWindow = browserWindow);
    },
    function() {
      assert.equal(windowUtils.activeWindow, browserWindow,
                       "Correct active window [1]");
      nextTest();
    },
    function() {
      assert.equal(windowUtils.activeBrowserWindow, browserWindow,
                       "Correct active browser window [2]");
      continueAfterFocus(windowUtils.activeWindow = browserWindow);
    },
    function() {
      assert.equal(windowUtils.activeWindow, browserWindow,
                       "Correct active window [3]");
      nextTest();
    },
    function() {
      assert.equal(windowUtils.activeBrowserWindow, browserWindow,
                       "Correct active browser window [4]");
      done();
    }
  ];

  function nextTest() {
    if (testSteps.length)
      testSteps.shift()();
  }
  nextTest();
};

exports.testWindowIterator = function(assert, done) {
  // make a new window
  let window = makeEmptyWindow();

  // make sure that the window hasn't loaded yet
  assert.notEqual(
      window.document.readyState,
      "complete",
      "window hasn't loaded yet.");

  // this window should only appear in windowIterator() while its loading
  assert.ok(toArray(windowUtils.windowIterator()).indexOf(window) === -1,
            "window isn't in windowIterator()");

  // Then it should be in windowIterator()
  window.addEventListener("load", function onload() {
    window.addEventListener("load", onload, false);
    assert.ok(toArray(windowUtils.windowIterator()).indexOf(window) !== -1,
              "window is now in windowIterator()");

    // Wait for the window unload before ending test
    close(window).then(done);
  }, false);
};

exports.testIgnoreClosingWindow = function(assert, done) {
  assert.equal(windows().length, 1, "Only one window open");

  // make a new window
  let window = makeEmptyWindow();

  assert.equal(windows().length, 2, "Two windows open");

  window.addEventListener("load", function onload() {
    window.addEventListener("load", onload, false);

    assert.equal(windows().length, 2, "Two windows open");

    // Wait for the window unload before ending test
    let checked = false;

    close(window).then(function() {
      assert.ok(checked, 'the test is finished');
    }).then(done, assert.fail)

    assert.equal(windows().length, 1, "Only one window open");
    checked = true;
  }, false);
};

require("sdk/test").run(exports);
