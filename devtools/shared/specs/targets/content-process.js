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
    // @backward-compat { version 129 } Once Fx129 is release, resource-*-form event won't be used anymore,
    //                                  only the resources-*-array will be still used.
    "resource-available-form": {
      type: "resource-available-form",
      resources: Arg(0, "array:json"),
    },
    "resource-destroyed-form": {
      type: "resource-destroyed-form",
      resources: Arg(0, "array:json"),
    },
    "resource-updated-form": {
      type: "resource-updated-form",
      resources: Arg(0, "array:json"),
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
