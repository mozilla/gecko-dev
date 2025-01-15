/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const throttlingProfiles = require("resource://devtools/client/shared/components/throttling/profiles.js");
const Types = require("resource://devtools/client/shared/components/throttling/types.js");

// Localization
const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
const L10N = new LocalizationHelper(
  "devtools/client/locales/network-throttling.properties"
);
const NO_THROTTLING_LABEL = L10N.getStr("responsive.noThrottling");
const OFFLINE_LABEL = L10N.getStr("responsive.offline");

loader.lazyGetter(this, "MenuItem", () => {
  const menuItemClass = require("resource://devtools/client/shared/components/menu/MenuItem.js");
  const menuItem = createFactory(menuItemClass);
  menuItem.DUMMY_ICON = menuItemClass.DUMMY_ICON;
  return menuItem;
});

loader.lazyGetter(this, "MenuButton", () => {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuButton.js")
  );
});
loader.lazyGetter(this, "MenuList", () => {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuList.js")
  );
});

/**
 * This component represents selector button that can be used
 * to throttle network bandwidth.
 */
class NetworkThrottlingMenu extends PureComponent {
  static get propTypes() {
    return {
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      onChangeNetworkThrottling: PropTypes.func.isRequired,
      toolboxDoc: PropTypes.object.isRequired,
    };
  }

  renderThrottlingMenu() {
    const { networkThrottling, onChangeNetworkThrottling } = this.props;

    const menuItems = throttlingProfiles.profiles.map(profile => {
      const isOffline =
        throttlingProfiles.PROFILE_CONSTANTS.OFFLINE === profile.id;
      return MenuItem({
        id: profile.id,
        key: profile.id,
        className: "network-throttling-item",
        checked:
          networkThrottling.enabled && profile.id == networkThrottling.profile,
        icon: null,
        label: isOffline ? OFFLINE_LABEL : profile.menuItemLabel,
        tooltip: isOffline ? OFFLINE_LABEL : profile.description,
        onClick: () => onChangeNetworkThrottling(true, profile.id),
      });
    });

    menuItems.unshift(dom.hr({ key: "separator" }));

    menuItems.unshift(
      MenuItem({
        id: NO_THROTTLING_LABEL,
        key: NO_THROTTLING_LABEL,
        className: "network-throttling-item",
        checked: !networkThrottling.enabled,
        icon: null,
        label: NO_THROTTLING_LABEL,
        tooltip: NO_THROTTLING_LABEL,
        onClick: () => onChangeNetworkThrottling(false, ""),
      })
    );
    return MenuList({}, menuItems);
  }

  render() {
    const { networkThrottling, toolboxDoc } = this.props;
    const label = networkThrottling.enabled
      ? networkThrottling.profile
      : NO_THROTTLING_LABEL;

    let title = NO_THROTTLING_LABEL;

    if (networkThrottling.enabled) {
      const id = networkThrottling.profile;
      const selectedProfile = throttlingProfiles.profiles.find(
        profile => profile.id === id
      );
      title = selectedProfile.description;
    }

    return MenuButton(
      {
        id: "network-throttling",
        menuId: "network-throttling-menu",
        toolboxDoc,
        className: "devtools-button devtools-dropdown-button",
        icon: null,
        label,
        title,
      },
      () => this.renderThrottlingMenu()
    );
  }
}

module.exports = NetworkThrottlingMenu;
