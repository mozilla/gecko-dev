/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {screenshotSpec} = require("devtools/shared/specs/screenshot");
const saveScreenshot = require("devtools/shared/screenshot/save");
const protocol = require("devtools/shared/protocol");

const ScreenshotFront = protocol.FrontClassWithSpec(screenshotSpec, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = form.screenshotActor;
    this.manage(this);
  },

  async captureAndSave(window, args) {
    const screenshot = await this.capture(args);
    return saveScreenshot(window, args, screenshot);
  },
});

exports.ScreenshotFront = ScreenshotFront;
