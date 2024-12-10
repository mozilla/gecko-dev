/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  contentScriptTargetSpec,
} = require("resource://devtools/shared/specs/targets/content-script.js");
const {
  FrontClassWithSpec,
  registerFront,
} = require("resource://devtools/shared/protocol.js");
const {
  TargetMixin,
} = require("resource://devtools/client/fronts/targets/target-mixin.js");

class ContentScriptTargetFront extends TargetMixin(
  FrontClassWithSpec(contentScriptTargetSpec)
) {
  form(json) {
    this.actorID = json.actor;

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;

    this._title = json.title;
  }
}

exports.ContentScriptTargetFront = ContentScriptTargetFront;
registerFront(exports.ContentScriptTargetFront);
