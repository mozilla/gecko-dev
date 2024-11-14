/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports) {
  const {
    Component,
  } = require("resource://devtools/client/shared/vendor/react.js");
  const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
  const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
  const {
    createFactories,
  } = require("resource://devtools/client/shared/react-utils.js");
  const { TextToolbar } = createFactories(
    require("resource://devtools/client/jsonview/components/TextToolbar.js")
  );
  const { LiveText } = createFactories(
    require("resource://devtools/client/jsonview/components/LiveText.js")
  );
  const { div } = dom;

  /**
   * This template represents the 'Raw Data' panel displaying
   * JSON as a text received from the server.
   */
  class TextPanel extends Component {
    static get propTypes() {
      return {
        isValidJson: PropTypes.bool,
        actions: PropTypes.object,
        errorMessage: PropTypes.string,
        data: PropTypes.instanceOf(Text),
      };
    }

    constructor(props) {
      super(props);
      this.state = {};
    }

    render() {
      return div(
        { className: "textPanelBox tab-panel-inner" },
        TextToolbar({
          actions: this.props.actions,
          isValidJson: this.props.isValidJson,
        }),
        this.props.errorMessage
          ? div({ className: "jsonParseError" }, this.props.errorMessage)
          : null,
        div({ className: "panelContent" }, LiveText({ data: this.props.data }))
      );
    }
  }

  // Exports from this module
  exports.TextPanel = TextPanel;
});
