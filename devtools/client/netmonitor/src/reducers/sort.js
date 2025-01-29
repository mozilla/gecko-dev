/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  SORT_BY,
  RESET_COLUMNS,
} = require("resource://devtools/client/netmonitor/src/constants.js");

function Sort() {
  return {
    // null means: sort by "waterfall", but don't highlight the table header
    type: null,
    ascending: true,
  };
}

function sortReducer(state = new Sort(), action) {
  switch (action.type) {
    case SORT_BY: {
      if (action.sortType != null && action.sortType == state.type) {
        return {
          ...state,
          ascending: !state.ascending,
        };
      }
      if (state.type == action.sortType && state.ascending) {
        return state;
      }
      return {
        ...state,
        type: action.sortType,
        ascending: true,
      };
    }

    case RESET_COLUMNS: {
      if (state.type == null && state.ascending === true) {
        return state;
      }
      return {
        ...state,
        type: null,
        ascending: true,
      };
    }

    default:
      return state;
  }
}

module.exports = {
  Sort,
  sortReducer,
};
