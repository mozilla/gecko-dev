"use strict";
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});

this.sourceMap = require("source-map");
this.acorn = require("acorn/acorn");
this.prettyFast = require("devtools/shared/pretty-fast/pretty-fast");
const { console } = Cu.import("resource://gre/modules/Console.jsm", {});

// Register a console listener, so console messages don't just disappear
// into the ether.
var errorCount = 0;
var listener = {
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

    // Ignored until they are fixed in bug 1242968.
    if (string.includes("JavaScript Warning")) {
      return;
    }

    do_throw("head_pretty-fast.js got console message: " + string + "\n");
  }
};

var consoleService = Cc["@mozilla.org/consoleservice;1"].getService(Ci.nsIConsoleService);
consoleService.registerListener(listener);

