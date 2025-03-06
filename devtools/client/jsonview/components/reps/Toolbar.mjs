/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Component } from "resource://devtools/client/shared/vendor/react.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import * as dom from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

/**
 * Renders a simple toolbar.
 */
class Toolbar extends Component {
  static get propTypes() {
    return {
      children: PropTypes.oneOfType([PropTypes.array, PropTypes.element]),
    };
  }

  render() {
    return dom.div({ className: "toolbar" }, this.props.children);
  }
}

/**
 * Renders a simple toolbar button.
 */
class ToolbarButton extends Component {
  static get propTypes() {
    return {
      active: PropTypes.bool,
      disabled: PropTypes.bool,
      children: PropTypes.string,
    };
  }

  render() {
    const props = Object.assign({ className: "btn" }, this.props);
    return dom.button(props, this.props.children);
  }
}

export default { Toolbar, ToolbarButton };
