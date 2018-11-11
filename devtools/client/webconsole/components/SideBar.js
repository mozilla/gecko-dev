/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { getObjectInspector } = require("devtools/client/webconsole/utils/object-inspector");
const actions = require("devtools/client/webconsole/actions/index");
const SplitBox = createFactory(require("devtools/client/shared/components/splitter/SplitBox"));
const { l10n } = require("devtools/client/webconsole/utils/messages");

const reps = require("devtools/client/shared/components/reps/reps");
const { MODE } = reps;

class SideBar extends Component {
  static get propTypes() {
    return {
      serviceContainer: PropTypes.object,
      dispatch: PropTypes.func.isRequired,
      sidebarVisible: PropTypes.bool,
      grip: PropTypes.object,
    };
  }

  constructor(props) {
    super(props);
    this.onClickSidebarClose = this.onClickSidebarClose.bind(this);
  }

  shouldComponentUpdate(nextProps) {
    const {
      grip,
      sidebarVisible,
    } = nextProps;

    return sidebarVisible !== this.props.sidebarVisible
      || grip !== this.props.grip;
  }

  onClickSidebarClose() {
    this.props.dispatch(actions.sidebarClose());
  }

  render() {
    if (!this.props.sidebarVisible) {
      return null;
    }

    const {
      grip,
      serviceContainer,
    } = this.props;

    const objectInspector = getObjectInspector(grip, serviceContainer, {
      autoExpandDepth: 1,
      mode: MODE.SHORT,
      autoFocusRoot: true,
    });

    const endPanel = dom.aside({
      className: "sidebar-wrapper",
    },
      dom.header({
        className: "devtools-toolbar webconsole-sidebar-toolbar",
      },
        dom.button({
          className: "devtools-button sidebar-close-button",
          title: l10n.getStr("webconsole.closeSidebarButton.tooltip"),
          onClick: this.onClickSidebarClose,
        })
      ),
      dom.aside({
        className: "sidebar-contents",
      }, objectInspector)
    );

    return SplitBox({
      className: "sidebar",
      endPanel,
      endPanelControl: true,
      initialSize: "200px",
      minSize: "100px",
      vert: true,
    });
  }
}

function mapStateToProps(state, props) {
  return {
    sidebarVisible: state.ui.sidebarVisible,
    grip: state.ui.gripInSidebar,
  };
}

module.exports = connect(mapStateToProps)(SideBar);
