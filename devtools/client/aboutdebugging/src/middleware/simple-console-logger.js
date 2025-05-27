/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const {
  PREFERENCES,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");

const lazy = {};
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "shouldLog",
  PREFERENCES.SHOW_REDUX_ACTIONS,
  false
);

const simpleConsoleLogger = store => next => action => {
  if (!lazy.shouldLog) {
    return next(action);
  }

  console.group(`Action: ${action.type}`);
  console.log("Previous state:", store.getState());
  console.log("Dispatched action:", action);

  const result = next(action);

  console.log("Next state:", store.getState());
  console.groupEnd();

  return result;
};

exports.simpleConsoleLogger = simpleConsoleLogger;
