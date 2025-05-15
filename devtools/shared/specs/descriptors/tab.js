/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  generateActorSpec,
  Option,
  Arg,
  RetVal,
} = require("resource://devtools/shared/protocol.js");

const tabDescriptorSpec = generateActorSpec({
  typeName: "tabDescriptor",

  methods: {
    getTarget: {
      request: {},
      response: {
        frame: RetVal("json"),
      },
    },
    getFavicon: {
      request: {},
      response: {
        favicon: RetVal("string"),
      },
    },
    getWatcher: {
      request: {
        isServerTargetSwitchingEnabled: Option(0, "boolean"),
        isPopupDebuggingEnabled: Option(0, "boolean"),
      },
      response: RetVal("watcher"),
    },
    // Added in Firefox 140
    navigateTo: {
      request: {
        url: Arg(0, "string"),
        waitForLoad: Arg(1, "boolean"),
      },
      response: {},
    },
    // Added in Firefox 140
    goBack: {
      request: {},
      response: {},
    },
    // Added in Firefox 140
    goForward: {
      request: {},
      response: {},
    },
    reloadDescriptor: {
      request: {
        bypassCache: Option(0, "boolean"),
      },
      response: {},
    },
  },

  events: {
    "descriptor-destroyed": {
      type: "descriptor-destroyed",
    },
  },
});

exports.tabDescriptorSpec = tabDescriptorSpec;
