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
    findDOMNode,
  } = require("resource://devtools/client/shared/vendor/react-dom.js");
  const {
    pre,
  } = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

  /**
   * This object represents a live DOM text node in a <pre>.
   */
  class LiveText extends Component {
    static get propTypes() {
      return {
        data: PropTypes.instanceOf(Text),
      };
    }

    componentDidMount() {
      this.componentDidUpdate();
    }

    componentDidUpdate() {
      const el = findDOMNode(this);
      if (el.firstChild === this.props.data) {
        return;
      }
      el.textContent = "";
      el.append(this.props.data);
    }

    render() {
      return pre({ className: "data" });
    }
  }

  // Exports from this module
  exports.LiveText = LiveText;
});
