/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file exposes the Redux reducers of the box model, grid and grid highlighter
// settings.

exports.animations = require("devtools/client/inspector/animation/reducers/animations");
exports.boxModel = require("devtools/client/inspector/boxmodel/reducers/box-model");
exports.changes = require("devtools/client/inspector/changes/reducers/changes");
exports.classList = require("devtools/client/inspector/rules/reducers/class-list");
exports.compatibility = require("devtools/client/inspector/compatibility/reducers/compatibility");
exports.extensionsSidebar = require("devtools/client/inspector/extensions/reducers/sidebar");
exports.flexbox = require("devtools/client/inspector/flexbox/reducers/flexbox");
exports.fontOptions = require("devtools/client/inspector/fonts/reducers/font-options");
exports.fontData = require("devtools/client/inspector/fonts/reducers/fonts");
exports.fontEditor = require("devtools/client/inspector/fonts/reducers/font-editor");
exports.grids = require("devtools/client/inspector/grids/reducers/grids");
exports.highlighterSettings = require("devtools/client/inspector/grids/reducers/highlighter-settings");
exports.pseudoClasses = require("devtools/client/inspector/rules/reducers/pseudo-classes");
exports.rules = require("devtools/client/inspector/rules/reducers/rules");

// This ObjectInspector reducer is needed for the Extension Sidebar.
const {
  default: objectInspector,
} = require("devtools/client/debugger/packages/devtools-reps/src/object-inspector/reducer");
exports.objectInspector = objectInspector;
