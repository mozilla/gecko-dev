/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  propertiesEqual,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");
const { truncateString } = require("resource://devtools/shared/string.js");
const {
  MAX_UI_STRING_LENGTH,
} = require("resource://devtools/client/netmonitor/src/constants.js");
const {
  getOverriddenUrl,
} = require("resource://devtools/client/netmonitor/src/selectors/index.js");

const UPDATED_FILE_PROPS = ["urlDetails", "waitingTime"];

class RequestListColumnPath extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      onWaterfallMouseDown: PropTypes.func,
      isOverridden: PropTypes.bool.isRequired,
      overriddenUrl: PropTypes.string,
    };
  }

  shouldComponentUpdate(nextProps) {
    return (
      !propertiesEqual(UPDATED_FILE_PROPS, this.props.item, nextProps.item) ||
      nextProps.overriddenUrl !== this.props.overriddenUrl
    );
  }

  render() {
    const {
      item: { urlDetails },
      isOverridden,
      overriddenUrl,
    } = this.props;

    const originalFileURL = urlDetails.url;
    const decodedFileURL = urlDetails.unicodeUrl;
    const ORIGINAL_FILE_URL = L10N.getFormatStr(
      "netRequest.originalFileURL.tooltip",
      originalFileURL
    );
    const DECODED_FILE_URL = L10N.getFormatStr(
      "netRequest.decodedFileURL.tooltip",
      decodedFileURL
    );
    const requestedPath = urlDetails.path;
    const fileToolTip =
      originalFileURL === decodedFileURL
        ? originalFileURL
        : ORIGINAL_FILE_URL + "\n\n" + DECODED_FILE_URL;

    // Build extra content for the title if the request is overridden.
    const overrideTitle = isOverridden ? ` → ${overriddenUrl}` : "";

    return dom.td(
      {
        className: "requests-list-column requests-list-path",
        title:
          truncateString(fileToolTip, MAX_UI_STRING_LENGTH) + overrideTitle,
      },
      dom.div({}, truncateString(requestedPath, MAX_UI_STRING_LENGTH))
    );
  }
}

module.exports = connect(
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
)(RequestListColumnPath);
