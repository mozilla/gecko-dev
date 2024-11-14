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
  const { JsonPanel } = createFactories(
    require("resource://devtools/client/jsonview/components/JsonPanel.js")
  );
  const { TextPanel } = createFactories(
    require("resource://devtools/client/jsonview/components/TextPanel.js")
  );
  const { HeadersPanel } = createFactories(
    require("resource://devtools/client/jsonview/components/HeadersPanel.js")
  );
  const { Tabs, TabPanel } = createFactories(
    require("resource://devtools/client/shared/components/tabs/Tabs.js")
  );

  /**
   * This object represents the root application template
   * responsible for rendering the basic tab layout.
   */
  class MainTabbedArea extends Component {
    static get propTypes() {
      return {
        jsonText: PropTypes.instanceOf(Text),
        activeTab: PropTypes.number,
        actions: PropTypes.object,
        headers: PropTypes.object,
        searchFilter: PropTypes.string,
        json: PropTypes.oneOfType([
          PropTypes.string,
          PropTypes.object,
          PropTypes.array,
          PropTypes.bool,
          PropTypes.number,
        ]),
        expandedNodes: PropTypes.instanceOf(Set),
      };
    }

    constructor(props) {
      super(props);

      this.state = {
        json: props.json,
        expandedNodes: props.expandedNodes,
        jsonText: props.jsonText,
        activeTab: props.activeTab,
      };

      this.onTabChanged = this.onTabChanged.bind(this);
    }

    onTabChanged(index) {
      this.setState({ activeTab: index });

      // Send notification event to the window. This is useful for tests.
      window.dispatchEvent(
        new CustomEvent("TabChanged", { detail: { index } })
      );
    }

    render() {
      return Tabs(
        {
          activeTab: this.state.activeTab,
          onAfterChange: this.onTabChanged,
          tall: true,
        },
        TabPanel(
          {
            id: "json",
            className: "json",
            title: JSONView.Locale["jsonViewer.tab.JSON"],
          },
          JsonPanel({
            data: this.state.json,
            expandedNodes: this.state.expandedNodes,
            actions: this.props.actions,
            searchFilter: this.state.searchFilter,
            dataSize: this.state.jsonText.length,
          })
        ),
        TabPanel(
          {
            id: "rawdata",
            className: "rawdata",
            title: JSONView.Locale["jsonViewer.tab.RawData"],
          },
          TextPanel({
            isValidJson:
              !(this.state.json instanceof Error) &&
              document.readyState != "loading",
            data: this.state.jsonText,
            errorMessage:
              this.state.json instanceof Error ? this.state.json + "" : null,
            actions: this.props.actions,
          })
        ),
        TabPanel(
          {
            id: "headers",
            className: "headers",
            title: JSONView.Locale["jsonViewer.tab.Headers"],
          },
          HeadersPanel({
            data: this.props.headers,
            actions: this.props.actions,
            searchFilter: this.props.searchFilter,
          })
        )
      );
    }
  }

  // Exports from this module
  exports.MainTabbedArea = MainTabbedArea;
});
