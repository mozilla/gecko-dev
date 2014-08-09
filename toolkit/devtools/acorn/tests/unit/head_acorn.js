"use strict";
const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

// We also need a valid nsIXulAppInfo service as Webapps.jsm is querying it
Cu.import("resource://testing-common/AppInfo.jsm");
updateAppInfo();

const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const { require } = devtools;


function isObject(value) {
  return typeof value === "object" && value !== null;
}

function intersect(a, b) {
  const seen = new Set(a);
  return b.filter(value => seen.has(value));
}

function checkEquivalentASTs(expected, actual, prop = []) {
  do_print("Checking: " + prop.join(" "));

  if (!isObject(expected)) {
    return void do_check_eq(expected, actual);
  }

  do_check_true(isObject(actual));

  if (Array.isArray(expected)) {
    do_check_true(Array.isArray(actual));
    do_check_eq(expected.length, actual.length);
    for (let i = 0; i < expected.length; i++) {
      checkEquivalentASTs(expected[i], actual[i], prop.concat(i));
    }
  } else {
    // We must intersect the keys since acorn and Reflect have different
    // extraneous properties on their AST nodes.
    const keys = intersect(Object.keys(expected), Object.keys(actual));
    for (let key of keys) {
      checkEquivalentASTs(expected[key], actual[key], prop.concat(key));
    }
  }
}


// Register a console listener, so console messages don't just disappear
// into the ether.
let errorCount = 0;
let listener = {
  observe: function (aMessage) {
    errorCount++;
    try {
      // If we've been given an nsIScriptError, then we can print out
      // something nicely formatted, for tools like Emacs to pick up.
      var scriptError = aMessage.QueryInterface(Ci.nsIScriptError);
      dump(aMessage.sourceName + ":" + aMessage.lineNumber + ": " +
           scriptErrorFlagsToKind(aMessage.flags) + ": " +
           aMessage.errorMessage + "\n");
      var string = aMessage.errorMessage;
    } catch (x) {
      // Be a little paranoid with message, as the whole goal here is to lose
      // no information.
      try {
        var string = "" + aMessage.message;
      } catch (x) {
        var string = "<error converting error message to string>";
      }
    }

    do_throw("head_acorn.js got console message: " + string + "\n");
  }
};

let consoleService = Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService);
consoleService.registerListener(listener);
