/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental",
  "engines": {
    "Firefox": "> 28"
  }
};

const { Toolbar } = require("./toolbar/model");
require("./toolbar/view");

exports.Toolbar = Toolbar;
