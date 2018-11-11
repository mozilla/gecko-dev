/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  FILTER_BAR_TOGGLE,
  INITIALIZE,
  MESSAGES_CLEAR,
  PERSIST_TOGGLE,
  REVERSE_SEARCH_INPUT_TOGGLE,
  SELECT_NETWORK_MESSAGE_TAB,
  SHOW_OBJECT_IN_SIDEBAR,
  SIDEBAR_CLOSE,
  SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE,
  TIMESTAMPS_TOGGLE,
} = require("devtools/client/webconsole/constants");

const {
  PANELS,
} = require("devtools/client/netmonitor/src/constants");

const UiState = (overrides) => Object.freeze(Object.assign({
  filterBarVisible: false,
  initialized: false,
  networkMessageActiveTabId: PANELS.HEADERS,
  persistLogs: false,
  sidebarVisible: false,
  timestampsVisible: true,
  gripInSidebar: null,
  closeButtonVisible: false,
  reverseSearchInputVisible: false,
}, overrides));

function ui(state = UiState(), action) {
  switch (action.type) {
    case FILTER_BAR_TOGGLE:
      return Object.assign({}, state, {filterBarVisible: !state.filterBarVisible});
    case PERSIST_TOGGLE:
      return Object.assign({}, state, {persistLogs: !state.persistLogs});
    case TIMESTAMPS_TOGGLE:
      return Object.assign({}, state, {timestampsVisible: action.visible});
    case SELECT_NETWORK_MESSAGE_TAB:
      return Object.assign({}, state, {networkMessageActiveTabId: action.id});
    case SIDEBAR_CLOSE:
      return Object.assign({}, state, {
        sidebarVisible: false,
        gripInSidebar: null,
      });
    case INITIALIZE:
      return Object.assign({}, state, {initialized: true});
    case MESSAGES_CLEAR:
      return Object.assign({}, state, {sidebarVisible: false, gripInSidebar: null});
    case SHOW_OBJECT_IN_SIDEBAR:
      if (action.grip === state.gripInSidebar) {
        return state;
      }
      return Object.assign({}, state, {sidebarVisible: true, gripInSidebar: action.grip});
    case SPLIT_CONSOLE_CLOSE_BUTTON_TOGGLE:
      return Object.assign({}, state, {closeButtonVisible: action.shouldDisplayButton});
    case REVERSE_SEARCH_INPUT_TOGGLE:
      return {...state, reverseSearchInputVisible: !state.reverseSearchInputVisible};
  }

  return state;
}

module.exports = {
  UiState,
  ui,
};
