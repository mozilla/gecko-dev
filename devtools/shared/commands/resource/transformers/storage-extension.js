/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  TYPES: { EXTENSION_STORAGE },
} = require("resource://devtools/shared/commands/resource/resource-command.js");

const { Front, types } = require("resource://devtools/shared/protocol.js");

module.exports = function ({ resource, watcherFront }) {
  if (!(resource instanceof Front) && watcherFront) {
    // The extension storage actor is instantiated once for the whole toolbox lifecycle,
    // so that it isn't bound to any particular WindowGlobal target and should
    // rather be bound to the Watcher actor.
    resource = types.getType("extensionStorage").read(resource, watcherFront);
    resource.resourceType = EXTENSION_STORAGE;
    resource.resourceId = EXTENSION_STORAGE;
    resource.resourceKey = "extensionStorage";
  }

  return resource;
};
