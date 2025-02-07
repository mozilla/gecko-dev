/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  td,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  getFormattedIPAndPort,
} = require("resource://devtools/client/netmonitor/src/utils/format-utils.js");
const {
  propertiesEqual,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");
const SecurityState = createFactory(
  require("resource://devtools/client/netmonitor/src/components/SecurityState.js")
);
const {
  getOverriddenUrl,
} = require("resource://devtools/client/netmonitor/src/selectors/index.js");
const { truncateString } = require("resource://devtools/shared/string.js");
const {
  MAX_UI_STRING_LENGTH,
} = require("resource://devtools/client/netmonitor/src/constants.js");

const UPDATED_FILE_PROPS = ["remoteAddress", "securityState", "urlDetails"];

class RequestListColumnUrl extends Component {
  static get propTypes() {
    return {
      isOverridden: PropTypes.bool.isRequired,
      item: PropTypes.object.isRequired,
      onSecurityIconMouseDown: PropTypes.func.isRequired,
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
      isOverridden,
      item: { urlDetails },
      overriddenUrl,
    } = this.props;

    const { item, onSecurityIconMouseDown } = this.props;

    const {
      remoteAddress,
      remotePort,
      urlDetails: { isLocal },
    } = item;

    // deals with returning whole url
    const originalURL = urlDetails.url;
    const decodedFileURL = urlDetails.unicodeUrl;
    const ORIGINAL_FILE_URL = L10N.getFormatStr(
      "netRequest.originalFileURL.tooltip",
      originalURL
    );
    const DECODED_FILE_URL = L10N.getFormatStr(
      "netRequest.decodedFileURL.tooltip",
      decodedFileURL
    );
    const urlToolTip =
      originalURL === decodedFileURL
        ? originalURL
        : ORIGINAL_FILE_URL + "\n\n" + DECODED_FILE_URL;

    // Build extra content for the title if this is a remote address.
    const remoteAddressTitle = remoteAddress
      ? ` (${getFormattedIPAndPort(remoteAddress, remotePort)})`
      : "";

    // Build extra content for the title if the request is overridden.
    const overrideTitle = isOverridden ? ` → ${overriddenUrl}` : "";

    return td(
      {
        className: "requests-list-column requests-list-url",
        title:
          truncateString(urlToolTip, MAX_UI_STRING_LENGTH) +
          remoteAddressTitle +
          overrideTitle,
      },
      SecurityState({ item, onSecurityIconMouseDown, isLocal }),
      truncateString(originalURL, MAX_UI_STRING_LENGTH)
    );
  }
}

module.exports = connect(
  (state, props) => {
    const overriddenUrl = getOverriddenUrl(state, props.item.urlDetails.url);
    return {
      isOverridden: !!overriddenUrl,
      overriddenUrl,
    };
  },
  {},
  undefined,
  { storeKey: "toolbox-store" }
)(RequestListColumnUrl);
