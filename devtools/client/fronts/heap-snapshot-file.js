/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  heapSnapshotFileSpec,
} = require("resource://devtools/shared/specs/heap-snapshot-file.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");

class HeapSnapshotFileFront extends FrontClassWithSpec(heapSnapshotFileSpec) {
  // Attribute name from which to retrieve the actorID out of the target actor's form
  formAttributeName = "heapSnapshotFileActor";
}

module.exports = HeapSnapshotFileFront;
registerFront(HeapSnapshotFileFront);
