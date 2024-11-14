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

  const { Headers } = createFactories(
    require("resource://devtools/client/jsonview/components/Headers.js")
  );
  const { HeadersToolbar } = createFactories(
    require("resource://devtools/client/jsonview/components/HeadersToolbar.js")
  );

  const { div } = dom;

  /**
   * This template represents the 'Headers' panel
   * s responsible for rendering its content.
   */
  class HeadersPanel extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
        data: PropTypes.object,
      };
    }

    constructor(props) {
      super(props);

      this.state = {
        data: {},
      };
    }

    render() {
      const data = this.props.data;

      return div(
        { className: "headersPanelBox tab-panel-inner" },
        HeadersToolbar({ actions: this.props.actions }),
        div({ className: "panelContent" }, Headers({ data }))
      );
    }
  }

  // Exports from this module
  exports.HeadersPanel = HeadersPanel;
});
