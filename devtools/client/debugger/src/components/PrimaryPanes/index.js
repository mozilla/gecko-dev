/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "devtools/client/shared/vendor/react";
import PropTypes from "devtools/client/shared/vendor/react-prop-types";

import actions from "../../actions/index";
import { getSelectedPrimaryPaneTab } from "../../selectors/index";
import { prefs, features } from "../../utils/prefs";
import { connect } from "devtools/client/shared/vendor/react-redux";
import { primaryPaneTabs } from "../../constants";

import Outline from "./Outline";
import SourcesTree from "./SourcesTree";
import ProjectSearch from "./ProjectSearch";
import Tracer from "./Tracer";
const AppErrorBoundary = require("resource://devtools/client/shared/components/AppErrorBoundary.js");

const {
  TabPanel,
  Tabs,
} = require("resource://devtools/client/shared/components/tabs/Tabs.js");

// Note that the following list should follow the same order as displayed
const tabs = [
  primaryPaneTabs.SOURCES,
  primaryPaneTabs.OUTLINE,
  primaryPaneTabs.PROJECT_SEARCH,
];
if (features.javascriptTracing) {
  tabs.push(primaryPaneTabs.TRACER);
}

class PrimaryPanes extends Component {
  constructor(props) {
    super(props);

    this.state = {
      alphabetizeOutline: prefs.alphabetizeOutline,
    };
  }

  static get propTypes() {
    return {
      selectedTab: PropTypes.oneOf(tabs).isRequired,
      setPrimaryPaneTab: PropTypes.func.isRequired,
      setActiveSearch: PropTypes.func.isRequired,
      closeActiveSearch: PropTypes.func.isRequired,
    };
  }

  onAlphabetizeClick = () => {
    const alphabetizeOutline = !prefs.alphabetizeOutline;
    prefs.alphabetizeOutline = alphabetizeOutline;
    this.setState({ alphabetizeOutline });
  };

  onActivateTab = index => {
    const tab = tabs.at(index);
    this.props.setPrimaryPaneTab(tab);
    if (tab == primaryPaneTabs.PROJECT_SEARCH) {
      this.props.setActiveSearch(tab);
    } else {
      this.props.closeActiveSearch();
    }
  };

  render() {
    const { selectedTab } = this.props;
    return React.createElement(
      "aside",
      {
        className: "tab-panel sources-panel",
      },
      React.createElement(
        Tabs,
        {
          activeTab: tabs.indexOf(selectedTab),
          onAfterChange: this.onActivateTab,
        },
        React.createElement(
          TabPanel,
          {
            id: "sources-tab",
            key: `sources-tab${
              selectedTab === primaryPaneTabs.SOURCES ? "-selected" : ""
            }`,
            className: "tab sources-tab",
            title: L10N.getStr("sources.header"),
          },
          React.createElement(SourcesTree, null)
        ),
        React.createElement(
          TabPanel,
          {
            id: "outline-tab",
            key: `outline-tab${
              selectedTab === primaryPaneTabs.OUTLINE ? "-selected" : ""
            }`,
            className: "tab outline-tab",
            title: L10N.getStr("outline.header"),
          },
          React.createElement(Outline, {
            alphabetizeOutline: this.state.alphabetizeOutline,
            onAlphabetizeClick: this.onAlphabetizeClick,
          })
        ),
        React.createElement(
          TabPanel,
          {
            id: "search-tab",
            key: `search-tab${
              selectedTab === primaryPaneTabs.PROJECT_SEARCH ? "-selected" : ""
            }`,
            className: "tab search-tab",
            title: L10N.getStr("search.header"),
          },
          React.createElement(ProjectSearch, null)
        ),
        features.javascriptTracing
          ? React.createElement(
              TabPanel,
              {
                id: "tracer-tab",
                key: `tracer-tab${
                  selectedTab === primaryPaneTabs.TRACER ? "-selected" : ""
                }`,
                className: "tab tracer-tab",
                title: L10N.getStr("tracer.header"),
              },
              // As the tracer is an application on its own (and is prototypish)
              // let's encapsulate it to track its own exceptions.
              React.createElement(
                AppErrorBoundary,
                {
                  componentName: "Debugger",
                  panel: "JavaScript Tracer",
                },
                React.createElement(Tracer)
              )
            )
          : null
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    selectedTab: getSelectedPrimaryPaneTab(state),
  };
};

const connector = connect(mapStateToProps, {
  setPrimaryPaneTab: actions.setPrimaryPaneTab,
  setActiveSearch: actions.setActiveSearch,
  closeActiveSearch: actions.closeActiveSearch,
});

export default connector(PrimaryPanes);
