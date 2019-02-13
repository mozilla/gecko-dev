/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const { require } = devtools;
const Editor  = require("devtools/sourceeditor/editor");
const {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

gDevTools.testing = true;
SimpleTest.registerCleanupFunction(() => {
  gDevTools.testing = false;
});

/**
 * Open a new tab at a URL and call a callback on load
 */
function addTab(aURL, aCallback) {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  content.location = aURL;

  let tab = gBrowser.selectedTab;
  let browser = gBrowser.getBrowserForTab(tab);

  function onTabLoad() {
    browser.removeEventListener("load", onTabLoad, true);
    aCallback(browser, tab, browser.contentDocument);
  }

  browser.addEventListener("load", onTabLoad, true);
}

function promiseTab(aURL) {
  return new Promise(resolve =>
    addTab(aURL, resolve));
}

function setup(cb, additionalOpts = {}) {
  cb = cb || function() {};
  let def = promise.defer();
  const opt = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";
  const url = "data:application/vnd.mozilla.xul+xml;charset=UTF-8,<?xml version='1.0'?>" +
    "<?xml-stylesheet href='chrome://global/skin/global.css'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='Editor' width='600' height='500'><box flex='1'/></window>";

  let win = Services.ww.openWindow(null, url, "_blank", opt, null);
  let opts = {
    value: "Hello.",
    lineNumbers: true,
    foldGutter: true,
    gutters: [ "CodeMirror-linenumbers", "breakpoints", "CodeMirror-foldgutter" ]
  }
  for (let o in additionalOpts) {
    opts[o] = additionalOpts[o];
  }

  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);

    waitForFocus(function () {
      let box = win.document.querySelector("box");
      let editor = new Editor(opts);

      editor.appendTo(box)
        .then(() => {
          def.resolve({
            ed: editor,
            win: win,
            edWin: editor.container.contentWindow.wrappedJSObject
          });
          cb(editor, win);
        }, err => ok(false, err.message));
    }, win);
  }, false);

  return def.promise;
}

function ch(exp, act, label) {
  is(exp.line, act.line, label + " (line)");
  is(exp.ch, act.ch, label + " (ch)");
}

function teardown(ed, win) {
  ed.destroy();
  win.close();

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
  finish();
}

/**
 * Some tests may need to import one or more of the test helper scripts.
 * A test helper script is simply a js file that contains common test code that
 * is either not common-enough to be in head.js, or that is located in a separate
 * directory.
 * The script will be loaded synchronously and in the test's scope.
 * @param {String} filePath The file path, relative to the current directory.
 *                 Examples:
 *                 - "helper_attributes_test_runner.js"
 *                 - "../../../commandline/test/helpers.js"
 */
function loadHelperScript(filePath) {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  Services.scriptloader.loadSubScript(testDir + "/" + filePath, this);
}

/**
 * This method returns the portion of the input string `source` up to the
 * [line, ch] location.
 */
function limit(source, [line, ch]) {
  line++;
  let list = source.split("\n");
  if (list.length < line)
    return source;
  if (line == 1)
    return list[0].slice(0, ch);
  return [...list.slice(0, line - 1), list[line - 1].slice(0, ch)].join("\n");
}

function read(url) {
  let scriptableStream = Cc["@mozilla.org/scriptableinputstream;1"]
    .getService(Ci.nsIScriptableInputStream);

  let channel = Services.io.newChannel2(url,
                                        null,
                                        null,
                                        null,      // aLoadingNode
                                        Services.scriptSecurityManager.getSystemPrincipal(),
                                        null,      // aTriggeringPrincipal
                                        Ci.nsILoadInfo.SEC_NORMAL,
                                        Ci.nsIContentPolicy.TYPE_OTHER);
  let input = channel.open();
  scriptableStream.init(input);

  let data = "";
  while (input.available()) {
    data = data.concat(scriptableStream.read(input.available()));
  }
  scriptableStream.close();
  input.close();

  return data;
}

/**
 * This function is called by the CodeMirror test runner to report status
 * messages from the CM tests.
 * @see codemirror.html
 */
function codeMirror_setStatus(statusMsg, type, customMsg) {
  switch (type) {
    case "expected":
    case "ok":
      ok(1, statusMsg);
      break;
    case "error":
    case "fail":
      ok(0, statusMsg);
      break;
    default:
      info(statusMsg);
      break;
  }

  if (customMsg && typeof customMsg == "string" && customMsg != statusMsg) {
    info(customMsg);
  }
}
