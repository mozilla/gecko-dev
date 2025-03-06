/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: ["error", { "allow": ["Headers"] }] */

import { Component } from "resource://devtools/client/shared/vendor/react.mjs";
import PropTypes from "resource://devtools/client/shared/vendor/react-prop-types.mjs";
import * as dom from "resource://devtools/client/shared/vendor/react-dom-factories.mjs";
import { createFactories } from "resource://devtools/client/shared/react-utils.mjs";

import HeadersClass from "resource://devtools/client/jsonview/components/Headers.mjs";

const { Headers } = createFactories(HeadersClass);
import HeadersToolbarClass from "resource://devtools/client/jsonview/components/HeadersToolbar.mjs";

const { HeadersToolbar } = createFactories(HeadersToolbarClass);

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

export default { HeadersPanel };
