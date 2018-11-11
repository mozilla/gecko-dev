/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { L10N } = require("../utils/l10n");
const {
  fetchNetworkUpdatePacket,
  propertiesEqual,
} = require("../utils/request-utils");

// List of properties of the timing info we want to create boxes for
const { TIMING_KEYS } = require("../constants");

const { div } = dom;

const UPDATED_WATERFALL_PROPS = [
  "eventTimings",
  "fromCache",
  "fromServiceWorker",
  "totalTime",
];

class RequestListColumnWaterfall extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      firstRequestStartedMillis: PropTypes.number.isRequired,
      item: PropTypes.object.isRequired,
      onWaterfallMouseDown: PropTypes.func.isRequired,
    };
  }

  componentDidMount() {
    const { connector, item } = this.props;
    fetchNetworkUpdatePacket(connector.requestData, item, ["eventTimings"]);
  }

  componentWillReceiveProps(nextProps) {
    const { connector, item } = nextProps;
    fetchNetworkUpdatePacket(connector.requestData, item, ["eventTimings"]);
  }

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_WATERFALL_PROPS, this.props.item, nextProps.item) ||
      this.props.firstRequestStartedMillis !== nextProps.firstRequestStartedMillis;
  }

  timingTooltip() {
    const { eventTimings, fromCache, fromServiceWorker, totalTime } = this.props.item;
    const tooltip = [];

    if (fromCache || fromServiceWorker) {
      return tooltip;
    }

    if (eventTimings) {
      for (const key of TIMING_KEYS) {
        const width = eventTimings.timings[key];

        if (width > 0) {
          tooltip.push(L10N.getFormatStr("netmonitor.waterfall.tooltip." + key, width));
        }
      }
    }

    if (typeof totalTime === "number") {
      tooltip.push(L10N.getFormatStr("netmonitor.waterfall.tooltip.total", totalTime));
    }

    return tooltip.join(L10N.getStr("netmonitor.waterfall.tooltip.separator"));
  }

  timingBoxes() {
    const { eventTimings, fromCache, fromServiceWorker, totalTime } = this.props.item;
    const boxes = [];

    if (fromCache || fromServiceWorker) {
      return boxes;
    }

    if (eventTimings) {
      // Add a set of boxes representing timing information.
      for (const key of TIMING_KEYS) {
        const width = eventTimings.timings[key];

        // Don't render anything if it surely won't be visible.
        // One millisecond == one unscaled pixel.
        if (width > 0) {
          boxes.push(
            div({
              key,
              className: `requests-list-timings-box ${key}`,
              style: { width },
            })
          );
        }
      }
    }

    if (typeof totalTime === "number") {
      const title = L10N.getFormatStr("networkMenu.totalMS2", totalTime);
      boxes.push(
        div({
          key: "total",
          className: "requests-list-timings-total",
          title,
        }, title)
      );
    }

    return boxes;
  }

  render() {
    const {
      firstRequestStartedMillis,
      item,
      onWaterfallMouseDown,
    } = this.props;

    return (
      div({
        className: "requests-list-column requests-list-waterfall",
        onMouseOver: ({ target }) => {
          if (!target.title) {
            target.title = this.timingTooltip();
          }
        },
      },
        div({
          className: "requests-list-timings",
          style: {
            paddingInlineStart: `${item.startedMillis - firstRequestStartedMillis}px`,
          },
          onMouseDown: onWaterfallMouseDown,
        },
          this.timingBoxes(),
        )
      )
    );
  }
}

module.exports = RequestListColumnWaterfall;
