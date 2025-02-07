/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CLEAR_REQUESTS,
  OPEN_NETWORK_DETAILS,
  OPEN_ACTION_BAR,
  RESIZE_NETWORK_DETAILS,
  ENABLE_PERSISTENT_LOGS,
  DISABLE_BROWSER_CACHE,
  OPEN_STATISTICS,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  RESET_COLUMNS,
  RESPONSE_HEADERS,
  SELECT_DETAILS_PANEL_TAB,
  SELECT_ACTION_BAR_TAB,
  SEND_CUSTOM_REQUEST,
  SELECT_REQUEST,
  TOGGLE_COLUMN,
  WATERFALL_RESIZE,
  PANELS,
  MIN_COLUMN_WIDTH,
  SET_COLUMNS_WIDTH,
  SET_HEADERS_URL_PREVIEW_EXPANDED,
  SET_DEFAULT_RAW_RESPONSE,
} = require("resource://devtools/client/netmonitor/src/constants.js");

const cols = {
  override: true,
  status: true,
  method: true,
  domain: true,
  file: true,
  url: false,
  protocol: false,
  scheme: false,
  remoteip: false,
  initiator: true,
  type: true,
  cookies: false,
  setCookies: false,
  transferred: true,
  contentSize: true,
  priority: false,
  startTime: false,
  endTime: false,
  responseTime: false,
  duration: false,
  latency: false,
  waterfall: true,
};

function Columns() {
  return Object.assign(
    cols,
    RESPONSE_HEADERS.reduce(
      (acc, header) => Object.assign(acc, { [header]: false }),
      {}
    )
  );
}

function ColumnsData() {
  const defaultColumnsData = JSON.parse(
    Services.prefs
      .getDefaultBranch(null)
      .getCharPref("devtools.netmonitor.columnsData")
  );
  return new Map(defaultColumnsData.map(i => [i.name, i]));
}

function UI(initialState = {}) {
  return {
    columns: Columns(),
    columnsData: ColumnsData(),
    detailsPanelSelectedTab: PANELS.HEADERS,
    networkDetailsOpen: false,
    networkDetailsWidth: null,
    networkDetailsHeight: null,
    persistentLogsEnabled: Services.prefs.getBoolPref(
      "devtools.netmonitor.persistlog"
    ),
    defaultRawResponse: Services.prefs.getBoolPref(
      "devtools.netmonitor.ui.default-raw-response",
      false
    ),
    browserCacheDisabled: Services.prefs.getBoolPref("devtools.cache.disabled"),
    slowLimit: Services.prefs.getIntPref("devtools.netmonitor.audits.slow"),
    statisticsOpen: false,
    waterfallWidth: null,
    networkActionOpen: false,
    selectedActionBarTabId: null,
    shouldExpandHeadersUrlPreview: false,
    ...initialState,
  };
}

function resetColumns(state) {
  return {
    ...state,
    columns: Columns(),
    columnsData: ColumnsData(),
  };
}

function resizeWaterfall(state, action) {
  if (state.waterfallWidth == action.width) {
    return state;
  }
  return {
    ...state,
    waterfallWidth: action.width,
  };
}

function openNetworkDetails(state, action) {
  if (state.networkDetailsOpen == action.open) {
    return state;
  }
  return {
    ...state,
    networkDetailsOpen: action.open,
  };
}

function openNetworkAction(state, action) {
  if (state.networkActionOpen == action.open) {
    return state;
  }
  return {
    ...state,
    networkActionOpen: action.open,
  };
}

function resizeNetworkDetails(state, action) {
  if (
    state.networkDetailsWidth == action.width &&
    state.networkDetailsHeight == action.height
  ) {
    return state;
  }
  return {
    ...state,
    networkDetailsWidth: action.width,
    networkDetailsHeight: action.height,
  };
}

function enablePersistentLogs(state, action) {
  if (action.persistentLogsEnabled == action.enabled) {
    return state;
  }
  return {
    ...state,
    persistentLogsEnabled: action.enabled,
  };
}

function disableBrowserCache(state, action) {
  if (state.browserCacheDisabled == action.disabled) {
    return state;
  }
  return {
    ...state,
    browserCacheDisabled: action.disabled,
  };
}

function openStatistics(state, action) {
  if (state.statisticsOpen == action.open) {
    return state;
  }
  return {
    ...state,
    statisticsOpen: action.open,
  };
}

function setDetailsPanelTab(state, action) {
  if (state.detailsPanelSelectedTab == action.id) {
    return state;
  }
  return {
    ...state,
    detailsPanelSelectedTab: action.id,
  };
}

function setActionBarTab(state, action) {
  if (state.selectedActionBarTabId == action.id) {
    return state;
  }
  return {
    ...state,
    selectedActionBarTabId: action.id,
  };
}

function setHeadersUrlPreviewExpanded(state, action) {
  if (state.shouldExpandHeadersUrlPreview == action.expanded) {
    return state;
  }
  return {
    ...state,
    shouldExpandHeadersUrlPreview: action.expanded,
  };
}

function toggleColumn(state, action) {
  const { column } = action;

  if (!state.columns.hasOwnProperty(column)) {
    return state;
  }

  return {
    ...state,
    columns: {
      ...state.columns,
      [column]: !state.columns[column],
    },
  };
}

function setColumnsWidth(state, action) {
  const { widths } = action;
  const columnsData = new Map(state.columnsData);

  widths.forEach(col => {
    let data = columnsData.get(col.name);
    if (!data) {
      data = {
        name: col.name,
        minWidth: MIN_COLUMN_WIDTH,
      };
    }
    columnsData.set(col.name, {
      ...data,
      width: col.width,
    });
  });

  return {
    ...state,
    columnsData,
  };
}

function setDefaultRawResponse(state, action) {
  return {
    ...state,
    defaultRawResponse: action.enabled,
  };
}

function ui(state = UI(), action) {
  switch (action.type) {
    case CLEAR_REQUESTS:
      return openNetworkDetails(state, { open: false });
    case OPEN_NETWORK_DETAILS:
      return openNetworkDetails(state, action);
    case RESIZE_NETWORK_DETAILS:
      return resizeNetworkDetails(state, action);
    case ENABLE_PERSISTENT_LOGS:
      return enablePersistentLogs(state, action);
    case DISABLE_BROWSER_CACHE:
      return disableBrowserCache(state, action);
    case OPEN_STATISTICS:
      return openStatistics(state, action);
    case RESET_COLUMNS:
      return resetColumns(state);
    case REMOVE_SELECTED_CUSTOM_REQUEST:
      return openNetworkDetails(state, { open: true });
    case SEND_CUSTOM_REQUEST:
      return openNetworkDetails(state, { open: false });
    case SELECT_DETAILS_PANEL_TAB:
      return setDetailsPanelTab(state, action);
    case SELECT_ACTION_BAR_TAB:
      return setActionBarTab(state, action);
    case SELECT_REQUEST:
      return openNetworkDetails(state, { open: true });
    case TOGGLE_COLUMN:
      return toggleColumn(state, action);
    case WATERFALL_RESIZE:
      return resizeWaterfall(state, action);
    case SET_COLUMNS_WIDTH:
      return setColumnsWidth(state, action);
    case OPEN_ACTION_BAR:
      return openNetworkAction(state, action);
    case SET_HEADERS_URL_PREVIEW_EXPANDED:
      return setHeadersUrlPreviewExpanded(state, action);
    case SET_DEFAULT_RAW_RESPONSE:
      return setDefaultRawResponse(state, action);
    default:
      return state;
  }
}

module.exports = {
  Columns,
  ColumnsData,
  UI,
  ui,
};
