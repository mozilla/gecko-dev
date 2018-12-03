/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { PAGE_TYPES, RUNTIMES } = require("../../constants");
const Types = require("../../types/index");
loader.lazyRequireGetter(this, "ADB_ADDON_STATES", "devtools/shared/adb/adb-addon", true);

const SidebarItem = createFactory(require("./SidebarItem"));
const SidebarFixedItem = createFactory(require("./SidebarFixedItem"));
const SidebarRuntimeItem = createFactory(require("./SidebarRuntimeItem"));
const RefreshDevicesButton = createFactory(require("./RefreshDevicesButton"));
const FIREFOX_ICON = "chrome://devtools/skin/images/aboutdebugging-firefox-logo.svg";
const CONNECT_ICON = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";
const GLOBE_ICON = "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg";
const USB_ICON = "chrome://devtools/skin/images/aboutdebugging-connect-icon.svg";

class Sidebar extends PureComponent {
  static get propTypes() {
    return {
      adbAddonStatus: PropTypes.string,
      className: PropTypes.string,
      dispatch: PropTypes.func.isRequired,
      isScanningUsb: PropTypes.bool.isRequired,
      networkRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
      selectedPage: PropTypes.string,
      selectedRuntime: PropTypes.string,
      usbRuntimes: PropTypes.arrayOf(Types.runtime).isRequired,
    };
  }

  renderAdbAddonStatus() {
    const isAddonInstalled = this.props.adbAddonStatus === ADB_ADDON_STATES.INSTALLED;
    const localizationId = isAddonInstalled ? "about-debugging-sidebar-usb-enabled" :
                                              "about-debugging-sidebar-usb-disabled";
    return Localized(
      {
        id: localizationId,
      }, dom.aside(
        {
          className: "sidebar__devices__message js-sidebar-usb-status",
        },
        localizationId
      )
    );
  }

  renderDevicesEmpty() {
    return SidebarItem(
      {
        isSelected: false,
      },
      Localized(
        {
          id: "about-debugging-sidebar-no-devices",
        },
        dom.aside(
          {
            className: "sidebar__devices__message js-sidebar-no-devices",
          },
          "No devices discovered"
        )
      )
    );
  }

  renderDevices() {
    const { networkRuntimes, usbRuntimes } = this.props;

    // render a "no devices" messages when the lists are empty
    if (!networkRuntimes.length && !usbRuntimes.length) {
      return this.renderDevicesEmpty();
    }
    // render all devices otherwise
    return [
      ...this.renderSidebarItems(GLOBE_ICON, networkRuntimes),
      ...this.renderSidebarItems(USB_ICON, usbRuntimes),
    ];
  }

  renderSidebarItems(icon, runtimes) {
    const { dispatch, selectedPage, selectedRuntime } = this.props;

    return runtimes.map(runtime => {
      const keyId = `${runtime.type}-${runtime.id}`;
      const runtimeHasDetails = !!runtime.runtimeDetails;
      const isSelected = selectedPage === PAGE_TYPES.RUNTIME &&
        runtime.id === selectedRuntime;

      return SidebarRuntimeItem({
        deviceName: runtime.extra.deviceName,
        dispatch,
        icon,
        key: keyId,
        isConnected: runtimeHasDetails,
        isSelected,
        name: runtime.name,
        runtimeId: runtime.id,
      });
    });
  }

  render() {
    const { dispatch, selectedPage, selectedRuntime, isScanningUsb } = this.props;

    return dom.aside(
      {
        className: `sidebar ${this.props.className || ""}`,
      },
      dom.ul(
        {},
        Localized(
          { id: "about-debugging-sidebar-this-firefox", attrs: { name: true } },
          SidebarFixedItem({
            icon: FIREFOX_ICON,
            isSelected: PAGE_TYPES.RUNTIME === selectedPage &&
              selectedRuntime === RUNTIMES.THIS_FIREFOX,
            key: RUNTIMES.THIS_FIREFOX,
            name: "This Firefox",
            to: `/runtime/${RUNTIMES.THIS_FIREFOX}`,
          })
        ),
        Localized(
          { id: "about-debugging-sidebar-connect", attrs: { name: true } },
          SidebarFixedItem({
            dispatch,
            icon: CONNECT_ICON,
            isSelected: PAGE_TYPES.CONNECT === selectedPage,
            key: PAGE_TYPES.CONNECT,
            name: "Connect",
            to: "/connect",
          })
        ),
        SidebarItem(
          {
            isSelected: false,
            key: "separator-0",
          },
          dom.hr({ className: "separator" }),
          this.renderAdbAddonStatus(),
        ),
        this.renderDevices(),
        SidebarItem(
          {
            className: "sidebar-item--breathe sidebar__refresh-usb",
            isSelected: false,
            key: "refresh-devices",
          },
          RefreshDevicesButton({
            dispatch,
            isScanning: isScanningUsb,
          })
        ),
      )
    );
  }
}

module.exports = Sidebar;
