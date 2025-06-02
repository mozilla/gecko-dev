/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.mjs");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.mjs");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const { KeyCodes } = require("resource://devtools/client/shared/keycodes.js");

const MenuButton = createFactory(
  require("resource://devtools/client/shared/components/menu/MenuButton.js")
);
const {
  parseUserAgent,
} = require("resource://devtools/client/responsive/utils/ua.js");

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

loader.lazyGetter(this, "MenuItem", () => {
  const menuItemClass = require("resource://devtools/client/shared/components/menu/MenuItem.js");
  const menuItem = createFactory(menuItemClass);
  menuItem.DUMMY_ICON = menuItemClass.DUMMY_ICON;
  return menuItem;
});

loader.lazyGetter(this, "MenuList", () => {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuList.js")
  );
});

const {
  getStr,
} = require("resource://devtools/client/responsive/utils/l10n.js");

class UserAgentInput extends PureComponent {
  static get propTypes() {
    return {
      onChangeUserAgent: PropTypes.func.isRequired,
      userAgent: PropTypes.string.isRequired,
      selectedDeviceName: PropTypes.string,
      selectedDeviceUserAgent: PropTypes.string,
    };
  }

  static getDerivedStateFromProps(props, state) {
    if (props.userAgent !== state.prevUserAgent) {
      return {
        value: props.userAgent,
        prevUserAgent: props.userAgent,
      };
    }
    return null;
  }

  constructor(props) {
    super(props);

    this.state = {
      // The user agent input value.
      value: this.props.userAgent,
      // Track the last passed userAgent value in the props to
      // to update the local state "value" when the prop changes
      prevUserAgent: this.props.userAgent,
    };

    this.onChange = this.onChange.bind(this);
    this.onKeyUp = this.onKeyUp.bind(this);
  }

  /**
   * Input change handler.
   *
   * @param  {Event} event
   */
  onChange({ target }) {
    const value = target.value;

    this.setState(prevState => {
      return {
        ...prevState,
        value,
      };
    });
  }

  /**
   * Input key up handler.
   *
   * @param  {Event} event
   */
  onKeyUp({ target, keyCode }) {
    if (keyCode == KeyCodes.DOM_VK_RETURN) {
      this.props.onChangeUserAgent(target.value);
      target.blur();
    }

    if (keyCode == KeyCodes.DOM_VK_ESCAPE) {
      this.setState({ value: this.props.userAgent });
      target.blur();
    }
  }

  onChangeUserAgent(userAgent) {
    this.setState({
      value: userAgent,
    });
    this.props.onChangeUserAgent(userAgent);
  }

  renderMenuList() {
    const browsers = [];

    const { selectedDeviceName, selectedDeviceUserAgent } = this.props;
    if (selectedDeviceName) {
      const { browser } = parseUserAgent(selectedDeviceUserAgent);
      browsers.push({
        name: selectedDeviceName,
        userAgent: selectedDeviceUserAgent,
        icon: browser ? browser.name.toLowerCase() : "",
        separator: true,
      });
    }

    const androidVersion = "15";
    const firefoxVersion = AppConstants.MOZ_APP_VERSION.replace(/[ab]\d+/, "");
    // Bug 1953205 should revisit how the browser/user agent string list is implemented
    // and avoid hardcoding the chrome version number
    const chromeVersion = "134.0.0.0";
    // Chrome uses a fixed version to reference WebKit and Safari
    const frozenWebkitVersionForChromeUA = "537.36";

    browsers.push(
      {
        name: "Firefox Desktop",
        // Empty string will default the firefox original user agent
        userAgent: "",
        icon: "firefox",
        version: firefoxVersion,
      },
      {
        name: "Firefox for Android",
        userAgent: `Mozilla/5.0 (Android ${androidVersion}; Mobile; rv:${firefoxVersion}) Gecko/${firefoxVersion} Firefox/${firefoxVersion}`,
        icon: "firefox",
        version: firefoxVersion,
      },
      {
        name: "Chrome Desktop",
        userAgent: `Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/${frozenWebkitVersionForChromeUA} (KHTML, like Gecko) Chrome/${chromeVersion} Safari/${frozenWebkitVersionForChromeUA}`,
        icon: "chrome",
        version: chromeVersion,
      }
    );

    const menuItems = [];
    for (const browser of browsers) {
      const { icon, name, userAgent, version, separator } = browser;
      menuItems.push(
        MenuItem({
          key: name,
          className:
            "user-agent-selector-item" + (separator ? " separator" : ""),
          label: [
            name,
            // Only show the major version as chrome uses 136.0.0.0
            version
              ? dom.span(
                  { className: "user-agent-browser-version" },
                  version.split(".")[0]
                )
              : null,
          ],
          icon: icon
            ? `chrome://devtools/skin/images/browsers/${icon}.svg`
            : MenuItem.DUMMY_ICON,
          tooltip: name,
          checked: this.state.value == userAgent,
          onClick: () => this.onChangeUserAgent(userAgent),
        })
      );
    }

    return MenuList({}, menuItems);
  }

  render() {
    return dom.label(
      { id: "user-agent-label" },
      "UA:",
      dom.input({
        id: "user-agent-input",
        className: "text-input",
        onChange: this.onChange,
        onKeyUp: this.onKeyUp,
        placeholder: getStr("responsive.customUserAgent"),
        type: "text",
        value: this.state.value,
      }),
      MenuButton(
        {
          id: "user-agent-selector",
          menuId: "user-agent-selector-menu",
          toolboxDoc: window.document,
          className: "devtools-button devtools-dropdown-button",
          label: "",
          title: getStr("responsive.userAgentList"),
        },
        () => this.renderMenuList()
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    userAgent: state.ui.userAgent,
  };
};

module.exports = connect(mapStateToProps)(UserAgentInput);
