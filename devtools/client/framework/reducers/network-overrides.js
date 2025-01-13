/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

exports.reducer = networkOverridesReducer;
function networkOverridesReducer(
  state = {
    mutableOverrides: {},
  },
  action
) {
  switch (action.type) {
    case "SET_NETWORK_OVERRIDE": {
      state.mutableOverrides[action.url] = action.path;
      return { ...state };
    }
    case "REMOVE_NETWORK_OVERRIDE": {
      delete state.mutableOverrides[action.url];
      return { ...state };
    }
    default:
      return state;
  }
}
