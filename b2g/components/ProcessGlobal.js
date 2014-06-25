/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/**
 * This code exists to be a "grab bag" of global code that needs to be
 * loaded per B2G process, but doesn't need to directly interact with
 * web content.
 *
 * (It's written as an XPCOM service because it needs to watch
 * app-startup.)
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

// Preloading the CSP jsm in this process early on.
Cu.import("resource://gre/modules/CSPUtils.jsm");

function debug(msg) {
  log(msg);
}
function log(msg) {
  // This file implements console.log(), so use dump().
  //dump('ProcessGlobal: ' + msg + '\n');
}

function ProcessGlobal() {}
ProcessGlobal.prototype = {
  classID: Components.ID('{1a94c87a-5ece-4d11-91e1-d29c29f21b28}'),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function pg_observe(subject, topic, data) {
    switch (topic) {
    case 'app-startup': {
      Services.obs.addObserver(this, 'console-api-log-event', false);
      let inParent = Cc["@mozilla.org/xre/app-info;1"]
                       .getService(Ci.nsIXULRuntime)
                       .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
      if (inParent) {
        let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                     .getService(Ci.nsIMessageListenerManager);
        ppmm.addMessageListener("getProfD", function(message) {
          return Services.dirsvc.get("ProfD", Ci.nsIFile).path;
        });
      }
      break;
    }
    case 'console-api-log-event': {
      // Pipe `console` log messages to the nsIConsoleService which
      // writes them to logcat on Gonk.
      let message = subject.wrappedJSObject;
      let prefix = ('Content JS ' + message.level.toUpperCase() +
                    ' at ' + message.filename + ':' + message.lineNumber +
                    ' in ' + (message.functionName || 'anonymous') + ': ');
      Services.console.logStringMessage(prefix + Array.join(message.arguments,
                                                            ' '));
      break;
    }
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ProcessGlobal]);
