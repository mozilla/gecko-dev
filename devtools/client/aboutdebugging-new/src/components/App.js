/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const LocalizationProvider = createFactory(FluentReact.LocalizationProvider);

const { PAGES } = require("../constants");
const Types = require("../types/index");

const ConnectPage = createFactory(require("./connect/ConnectPage"));
const RuntimePage = createFactory(require("./RuntimePage"));
const Sidebar = createFactory(require("./sidebar/Sidebar"));

class App extends PureComponent {
  static get propTypes() {
    return {
      adbAddonStatus: PropTypes.string,
      // The "dispatch" helper is forwarded to the App component via connect.
      // From that point, components are responsible for forwarding the dispatch
      // property to all components who need to dispatch actions.
      dispatch: PropTypes.func.isRequired,
      fluentBundles: PropTypes.arrayOf(PropTypes.object).isRequired,
      isScanningUsb: PropTypes.bool.isRequired,
      networkEnabled: PropTypes.bool.isRequired,
      networkLocations: PropTypes.arrayOf(PropTypes.string).isRequired,
      networkRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
      selectedPage: PropTypes.string,
      usbRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
      wifiEnabled: PropTypes.bool.isRequired,
    };
  }

  getSelectedPageComponent() {
    const {
      adbAddonStatus,
      dispatch,
      networkEnabled,
      networkLocations,
      selectedPage,
      wifiEnabled,
    } = this.props;

    if (!selectedPage) {
      // No page selected.
      return null;
    }

    switch (selectedPage) {
      case PAGES.CONNECT:
        return ConnectPage({
          adbAddonStatus,
          dispatch,
          networkEnabled,
          networkLocations,
          wifiEnabled,
        });
      default:
        // All pages except for the CONNECT page are RUNTIME pages.
        return RuntimePage({ dispatch });
    }
  }

  render() {
    const {
      adbAddonStatus,
      dispatch,
      fluentBundles,
      isScanningUsb,
      networkRuntimes,
      selectedPage,
      usbRuntimes,
    } = this.props;

    return LocalizationProvider(
      { messages: fluentBundles },
      dom.div(
        { className: "app" },
        Sidebar(
          {
            adbAddonStatus,
            className: "app__sidebar",
            dispatch,
            isScanningUsb,
            networkRuntimes,
            selectedPage,
            usbRuntimes,
          }
        ),
        dom.main(
          { className: "app__content" },
          this.getSelectedPageComponent()
        )
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    adbAddonStatus: state.ui.adbAddonStatus,
    isScanningUsb: state.ui.isScanningUsb,
    networkEnabled: state.ui.networkEnabled,
    networkLocations: state.ui.networkLocations,
    networkRuntimes: state.runtimes.networkRuntimes,
    selectedPage: state.ui.selectedPage,
    usbRuntimes: state.runtimes.usbRuntimes,
    wifiEnabled: state.ui.wifiEnabled,
  };
};

const mapDispatchToProps = dispatch => ({
  dispatch,
});

module.exports = connect(mapStateToProps, mapDispatchToProps)(App);
