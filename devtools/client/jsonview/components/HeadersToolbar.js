/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports) {
  const {
    Component,
  } = require("resource://devtools/client/shared/vendor/react.js");
  const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
  const {
    createFactories,
  } = require("resource://devtools/client/shared/react-utils.js");

  const { Toolbar, ToolbarButton } = createFactories(
    require("resource://devtools/client/jsonview/components/reps/Toolbar.js")
  );

  /**
   * This template is responsible for rendering a toolbar
   * within the 'Headers' panel.
   */
  class HeadersToolbar extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
      };
    }

    constructor(props) {
      super(props);
      this.onCopy = this.onCopy.bind(this);
    }

    // Commands

    onCopy() {
      this.props.actions.onCopyHeaders();
    }

    render() {
      return Toolbar(
        {},
        ToolbarButton(
          { className: "btn copy", onClick: this.onCopy },
          JSONView.Locale["jsonViewer.Copy"]
        )
      );
    }
  }

  // Exports from this module
  exports.HeadersToolbar = HeadersToolbar;
});
