/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createElement,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  getOverriddenUrl,
} = require("resource://devtools/client/netmonitor/src/selectors/index.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");

class RequestListColumnOverrideContent extends Component {
  static get propTypes() {
    return {
      isOverridden: PropTypes.bool.isRequired,
      item: PropTypes.object.isRequired,
      overriddenUrl: PropTypes.string,
    };
  }

  render() {
    const { isOverridden, overriddenUrl } = this.props;

    return dom.td({
      className:
        "requests-list-column requests-list-override" +
        (isOverridden ? " request-override-enabled" : ""),
      title: isOverridden
        ? L10N.getFormatStr("netmonitor.override.enabled", overriddenUrl)
        : L10N.getStr("netmonitor.override.disabled"),
    });
  }
}

const RequestListColumnOverrideInner = connect(
  (state, props) => {
    const overriddenUrl = getOverriddenUrl(state, props.item.urlDetails?.url);
    return {
      isOverridden: !!overriddenUrl,
      overriddenUrl,
    };
  },
  {},
  undefined,
  { storeKey: "toolbox-store" }
)(RequestListColumnOverrideContent);

class RequestListColumnOverride extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  render() {
    return createElement(RequestListColumnOverrideInner, {
      item: this.props.item,
    });
  }
}

module.exports = RequestListColumnOverride;
