/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  types,
  Arg,
  Option,
  RetVal,
  generateActorSpec,
} = require("resource://devtools/shared/protocol.js");

types.addDictType("contentProcessTarget.workers", {
  error: "nullable:string",
  workers: "nullable:array:workerDescriptor",
});

const contentProcessTargetSpec = generateActorSpec({
  typeName: "contentProcessTarget",

  methods: {
    listWorkers: {
      request: {},
      response: RetVal("contentProcessTarget.workers"),
    },

    pauseMatchingServiceWorkers: {
      request: {
        origin: Option(0, "string"),
      },
      response: {},
    },
  },

  events: {
    workerListChanged: {
      type: "workerListChanged",
    },

    "resources-available-array": {
      type: "resources-available-array",
      array: Arg(0, "array:json"),
    },
    "resources-destroyed-array": {
      type: "resources-destroyed-array",
      array: Arg(0, "array:json"),
    },
    "resources-updated-array": {
      type: "resources-updated-array",
      array: Arg(0, "array:json"),
    },
  },
});

exports.contentProcessTargetSpec = contentProcessTargetSpec;
