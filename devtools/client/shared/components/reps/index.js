/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  MODE,
} = require("resource://devtools/client/shared/components/reps/reps/constants.js");
const {
  REPS,
  getRep,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");

const {
  parseURLEncodedText,
  parseURLParams,
  maybeEscapePropertyName,
  getGripPreviewItems,
} = require("resource://devtools/client/shared/components/reps/reps/rep-utils.js");

module.exports = {
  REPS,
  getRep,
  MODE,
  maybeEscapePropertyName,
  parseURLEncodedText,
  parseURLParams,
  getGripPreviewItems,
};
