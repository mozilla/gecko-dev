/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");

const IssueItem = createFactory(require("./IssueItem"));

class IssueList extends PureComponent {
  static get propTypes() {
    return {
      issues: PropTypes.arrayOf(PropTypes.shape(Types.issue)).isRequired,
    };
  }

  render() {
    const { issues } = this.props;

    return dom.ul({}, issues.map(issue => IssueItem({ ...issue })));
  }
}

module.exports = IssueList;
