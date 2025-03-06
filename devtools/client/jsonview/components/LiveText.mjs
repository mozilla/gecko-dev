/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "resource://devtools/client/shared/vendor/react.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import ReactDOM from "resource://devtools/client/shared/vendor/react-dom.mjs";
import { pre } from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";

const { Component } = React;
const { findDOMNode } = ReactDOM;

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

export default { LiveText };
