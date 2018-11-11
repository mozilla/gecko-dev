/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  fetchNetworkUpdatePacket,
  propertiesEqual,
} = require("../utils/request-utils");
const { RESPONSE_HEADERS } = require("../constants");

// Components
/* global
  RequestListColumnCause,
  RequestListColumnContentSize,
  RequestListColumnCookies,
  RequestListColumnDomain,
  RequestListColumnFile,
  RequestListColumnMethod,
  RequestListColumnProtocol,
  RequestListColumnRemoteIP,
  RequestListColumnResponseHeader,
  RequestListColumnScheme,
  RequestListColumnSetCookies,
  RequestListColumnStatus,
  RequestListColumnTime,
  RequestListColumnTransferredSize,
  RequestListColumnType,
  RequestListColumnWaterfall
*/

loader.lazyGetter(this, "RequestListColumnCause", function() {
  return createFactory(require("./RequestListColumnCause"));
});
loader.lazyGetter(this, "RequestListColumnContentSize", function() {
  return createFactory(require("./RequestListColumnContentSize"));
});
loader.lazyGetter(this, "RequestListColumnCookies", function() {
  return createFactory(require("./RequestListColumnCookies"));
});
loader.lazyGetter(this, "RequestListColumnDomain", function() {
  return createFactory(require("./RequestListColumnDomain"));
});
loader.lazyGetter(this, "RequestListColumnFile", function() {
  return createFactory(require("./RequestListColumnFile"));
});
loader.lazyGetter(this, "RequestListColumnMethod", function() {
  return createFactory(require("./RequestListColumnMethod"));
});
loader.lazyGetter(this, "RequestListColumnProtocol", function() {
  return createFactory(require("./RequestListColumnProtocol"));
});
loader.lazyGetter(this, "RequestListColumnRemoteIP", function() {
  return createFactory(require("./RequestListColumnRemoteIP"));
});
loader.lazyGetter(this, "RequestListColumnResponseHeader", function() {
  return createFactory(require("./RequestListColumnResponseHeader"));
});
loader.lazyGetter(this, "RequestListColumnTime", function() {
  return createFactory(require("./RequestListColumnTime"));
});
loader.lazyGetter(this, "RequestListColumnScheme", function() {
  return createFactory(require("./RequestListColumnScheme"));
});
loader.lazyGetter(this, "RequestListColumnSetCookies", function() {
  return createFactory(require("./RequestListColumnSetCookies"));
});
loader.lazyGetter(this, "RequestListColumnStatus", function() {
  return createFactory(require("./RequestListColumnStatus"));
});
loader.lazyGetter(this, "RequestListColumnTransferredSize", function() {
  return createFactory(require("./RequestListColumnTransferredSize"));
});
loader.lazyGetter(this, "RequestListColumnType", function() {
  return createFactory(require("./RequestListColumnType"));
});
loader.lazyGetter(this, "RequestListColumnWaterfall", function() {
  return createFactory(require("./RequestListColumnWaterfall"));
});

/**
 * Used by shouldComponentUpdate: compare two items, and compare only properties
 * relevant for rendering the RequestListItem. Other properties (like request and
 * response headers, cookies, bodies) are ignored. These are very useful for the
 * network details, but not here.
 */
const UPDATED_REQ_ITEM_PROPS = [
  "mimeType",
  "eventTimings",
  "securityState",
  "status",
  "statusText",
  "fromCache",
  "fromServiceWorker",
  "method",
  "url",
  "remoteAddress",
  "cause",
  "contentSize",
  "transferredSize",
  "startedMillis",
  "totalTime",
  "requestCookies",
  "requestHeaders",
  "responseCookies",
  "responseHeaders",
];

const UPDATED_REQ_PROPS = [
  "firstRequestStartedMillis",
  "index",
  "isSelected",
  "requestFilterTypes",
  "waterfallWidth",
];

/**
 * Render one row in the request list.
 */
class RequestListItem extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      columns: PropTypes.object.isRequired,
      item: PropTypes.object.isRequired,
      index: PropTypes.number.isRequired,
      isSelected: PropTypes.bool.isRequired,
      firstRequestStartedMillis: PropTypes.number.isRequired,
      fromCache: PropTypes.bool,
      onCauseBadgeMouseDown: PropTypes.func.isRequired,
      onContextMenu: PropTypes.func.isRequired,
      onFocusedNodeChange: PropTypes.func,
      onMouseDown: PropTypes.func.isRequired,
      onSecurityIconMouseDown: PropTypes.func.isRequired,
      onWaterfallMouseDown: PropTypes.func.isRequired,
      requestFilterTypes: PropTypes.object.isRequired,
      waterfallWidth: PropTypes.number,
    };
  }

  componentDidMount() {
    if (this.props.isSelected) {
      this.refs.listItem.focus();
    }

    const { connector, item, requestFilterTypes } = this.props;
    // Filtering XHR & WS require to lazily fetch requestHeaders & responseHeaders
    if (requestFilterTypes.xhr || requestFilterTypes.ws) {
      fetchNetworkUpdatePacket(connector.requestData, item, [
        "requestHeaders",
        "responseHeaders",
      ]);
    }
  }

  componentWillReceiveProps(nextProps) {
    const { connector, item, requestFilterTypes } = nextProps;
    // Filtering XHR & WS require to lazily fetch requestHeaders & responseHeaders
    if (requestFilterTypes.xhr || requestFilterTypes.ws) {
      fetchNetworkUpdatePacket(connector.requestData, item, [
        "requestHeaders",
        "responseHeaders",
      ]);
    }
  }

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_REQ_ITEM_PROPS, this.props.item, nextProps.item) ||
      !propertiesEqual(UPDATED_REQ_PROPS, this.props, nextProps) ||
      this.props.columns !== nextProps.columns;
  }

  componentDidUpdate(prevProps) {
    if (!prevProps.isSelected && this.props.isSelected) {
      this.refs.listItem.focus();
      if (this.props.onFocusedNodeChange) {
        this.props.onFocusedNodeChange();
      }
    }
  }

  render() {
    const {
      connector,
      columns,
      item,
      index,
      isSelected,
      firstRequestStartedMillis,
      fromCache,
      onContextMenu,
      onMouseDown,
      onCauseBadgeMouseDown,
      onSecurityIconMouseDown,
      onWaterfallMouseDown,
    } = this.props;

    const classList = ["request-list-item", index % 2 ? "odd" : "even"];
    isSelected && classList.push("selected");
    fromCache && classList.push("fromCache");

    return (
      div({
        ref: "listItem",
        className: classList.join(" "),
        "data-id": item.id,
        tabIndex: 0,
        onContextMenu,
        onMouseDown,
      },
        columns.status && RequestListColumnStatus({ item }),
        columns.method && RequestListColumnMethod({ item }),
        columns.domain && RequestListColumnDomain({
            item,
            onSecurityIconMouseDown,
        }),
        columns.file && RequestListColumnFile({ item }),
        columns.protocol && RequestListColumnProtocol({ item }),
        columns.scheme && RequestListColumnScheme({ item }),
        columns.remoteip && RequestListColumnRemoteIP({ item }),
        columns.cause && RequestListColumnCause({
          item,
          onCauseBadgeMouseDown,
        }),
        columns.type && RequestListColumnType({ item }),
        columns.cookies && RequestListColumnCookies({ connector, item }),
        columns.setCookies && RequestListColumnSetCookies({ connector, item }),
        columns.transferred && RequestListColumnTransferredSize({ item }),
        columns.contentSize && RequestListColumnContentSize({ item }),
        columns.startTime && RequestListColumnTime({
          connector,
          item,
          firstRequestStartedMillis,
          type: "start",
        }),
        columns.endTime && RequestListColumnTime({
          connector,
          item,
          firstRequestStartedMillis,
          type: "end",
        }),
        columns.responseTime && RequestListColumnTime({
          connector,
          item,
          firstRequestStartedMillis,
          type: "response",
        }),
        columns.duration && RequestListColumnTime({
          connector,
          item,
          firstRequestStartedMillis,
          type: "duration",
        }),
        columns.latency && RequestListColumnTime({
          connector,
          item,
          firstRequestStartedMillis,
          type: "latency",
        }),
        ...RESPONSE_HEADERS.filter(header => columns[header]).map(
          header => RequestListColumnResponseHeader({
            connector,
            item,
            header,
          }),
        ),
        columns.waterfall && RequestListColumnWaterfall({
          connector,
          firstRequestStartedMillis,
          item,
          onWaterfallMouseDown,
        }),
      )
    );
  }
}

module.exports = RequestListItem;
