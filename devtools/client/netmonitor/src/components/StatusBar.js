/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  Component,
} = require("resource://devtools/client/shared/vendor/react.mjs");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.mjs");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);
const Actions = require("resource://devtools/client/netmonitor/src/actions/index.js");
const {
  getDisplayedRequestsSummary,
  getDisplayedTimingMarker,
} = require("resource://devtools/client/netmonitor/src/selectors/index.js");
const {
  getFormattedSize,
  getFormattedTime,
} = require("resource://devtools/client/netmonitor/src/utils/format-utils.js");
const {
  propertiesEqual,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

const { button, div } = dom;

const UPDATED_SUMMARY_PROPS = ["count", "contentSize", "transferredSize", "ms"];

const UPDATED_TIMING_PROPS = ["DOMContentLoaded", "load"];

/**
 * Status Bar component
 * Displays the summary of total size and transferred size by all requests
 * Also displays different timing markers
 */
class StatusBar extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      openStatistics: PropTypes.func.isRequired,
      summary: PropTypes.object.isRequired,
      timingMarkers: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    const { summary, timingMarkers } = this.props;
    return (
      !propertiesEqual(UPDATED_SUMMARY_PROPS, summary, nextProps.summary) ||
      !propertiesEqual(
        UPDATED_TIMING_PROPS,
        timingMarkers,
        nextProps.timingMarkers
      )
    );
  }

  render() {
    const { openStatistics, summary, timingMarkers, connector } = this.props;
    const { count, contentSize, transferredSize, ms } = summary;
    const { DOMContentLoaded, load } = timingMarkers;
    const { isBrowserToolbox } = connector.getToolbox();

    return div(
      { className: "devtools-toolbar devtools-toolbar-bottom" },
      !isBrowserToolbox
        ? Localized(
            {
              id: "network-menu-summary-tooltip-perf",
              attrs: { title: true },
            },
            button({
              className: "devtools-button requests-list-network-summary-button",
              onClick: openStatistics,
            })
          )
        : null,
      Localized(
        {
          id: "network-menu-summary-tooltip-requests-count",
          attrs: { title: true },
        },
        div(
          {
            className: "status-bar-label requests-list-network-summary-count",
          },
          Localized({
            id: "network-menu-summary-requests-count",
            $requestCount: count,
          })
        )
      ),
      count !== 0 &&
        Localized(
          {
            id: "network-menu-summary-tooltip-transferred",
            attrs: { title: true },
          },
          div(
            {
              className:
                "status-bar-label requests-list-network-summary-transfer",
            },
            Localized({
              id: "network-menu-summary-transferred",
              $formattedContentSize: getFormattedSize(contentSize),
              $formattedTransferredSize: getFormattedSize(transferredSize),
            })
          )
        ),
      count !== 0 &&
        Localized(
          {
            id: "network-menu-summary-tooltip-finish",
            attrs: { title: true },
          },
          div(
            {
              className:
                "status-bar-label requests-list-network-summary-finish",
            },
            Localized({
              id: "network-menu-summary-finish",
              $formattedTime: getFormattedTime(ms),
            })
          )
        ),
      DOMContentLoaded > -1 &&
        Localized(
          {
            id: "network-menu-summary-tooltip-domcontentloaded",
            attrs: { title: true },
          },
          div(
            {
              className: "status-bar-label dom-content-loaded",
            },
            `DOMContentLoaded: ${getFormattedTime(DOMContentLoaded)}`
          )
        ),
      load > -1 &&
        Localized(
          {
            id: "network-menu-summary-tooltip-load",
            attrs: { title: true },
          },
          div(
            {
              className: "status-bar-label load",
            },
            `load: ${getFormattedTime(load)}`
          )
        )
    );
  }
}

module.exports = connect(
  state => ({
    summary: getDisplayedRequestsSummary(state),
    timingMarkers: {
      DOMContentLoaded: getDisplayedTimingMarker(
        state,
        "firstDocumentDOMContentLoadedTimestamp"
      ),
      load: getDisplayedTimingMarker(state, "firstDocumentLoadTimestamp"),
    },
  }),
  (dispatch, props) => ({
    openStatistics: () =>
      dispatch(Actions.openStatistics(props.connector, true)),
  })
)(StatusBar);
