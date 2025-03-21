/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {import("../../@types/perf").State} StoreState
 */

"use strict";

const {
  PureComponent,
  createFactory,
  createElement: h,
  Fragment,
  createRef,
} = require("resource://devtools/client/shared/vendor/react.mjs");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const {
  div,
  h1,
  button,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const Localized = createFactory(
  require("resource://devtools/client/shared/vendor/fluent-react.js").Localized
);
const Settings = createFactory(
  require("resource://devtools/client/performance-new/components/aboutprofiling/Settings.js")
);
const Presets = createFactory(
  require("resource://devtools/client/performance-new/components/aboutprofiling/Presets.js")
);

const selectors = require("resource://devtools/client/performance-new/store/selectors.js");
const {
  restartBrowserWithEnvironmentVariable,
} = require("resource://devtools/client/performance-new/shared/browser.js");

/**
 * This component implements the button that triggers the menu that makes it
 * possible to show more actions.
 * @extends {React.PureComponent}
 */
class MoreActionsButton extends PureComponent {
  _menuRef = createRef();

  /**
   * See the part "Showing the menu" in
   * https://searchfox.org/mozilla-central/rev/4bacdbc8ac088f2ee516daf42c535fab2bc24a04/toolkit/content/widgets/panel-list/README.stories.md
   * Strangely our React's type doesn't have the `detail` property for
   * MouseEvent, so we're defining it manually.
   * @param {React.MouseEvent & { detail: number }} e
   */
  handleClickOrMousedown = e => {
    // The menu is toggled either for a "mousedown", or for a keyboard enter
    // (which triggers a "click" event with 0 clicks (detail == 0)).
    if (this._menuRef.current && (e.type == "mousedown" || e.detail === 0)) {
      this._menuRef.current.toggle(e.nativeEvent, e.currentTarget);
    }
  };

  render() {
    return h(
      Fragment,
      null,
      Localized(
        {
          id: "perftools-menu-more-actions-button",
          attrs: { title: true },
        },
        h("moz-button", {
          iconsrc: "chrome://global/skin/icons/more.svg",
          "aria-expanded": "false",
          "aria-haspopup": "menu",
          onClick: this.handleClickOrMousedown,
          onMouseDown: this.handleClickOrMousedown,
        })
      ),
      h(
        "panel-list",
        { ref: this._menuRef },
        h("panel-item", null, "To be continued")
      )
    );
  }
}

/**
 * @typedef {import("../../@types/perf").PageContext} PageContext
 *
 * @typedef {Object} StateProps
 * @property {boolean?} isSupportedPlatform
 * @property {PageContext} pageContext
 * @property {string | null} promptEnvRestart
 * @property {(() => void) | undefined} openRemoteDevTools
 *
 * @typedef {StateProps} Props
 */

/**
 * This is the top level component for the about:profiling page. It shares components
 * with the popup and DevTools page.
 *
 * @extends {React.PureComponent<Props>}
 */
class AboutProfiling extends PureComponent {
  render() {
    const {
      isSupportedPlatform,
      pageContext,
      promptEnvRestart,
      openRemoteDevTools,
    } = this.props;

    if (isSupportedPlatform === null) {
      // We don't know yet if this is a supported platform, wait for a response.
      return null;
    }

    return div(
      { className: `perf perf-${pageContext}` },
      promptEnvRestart
        ? div(
            { className: "perf-env-restart" },
            div(
              {
                className:
                  "perf-photon-message-bar perf-photon-message-bar-warning perf-env-restart-fixed",
              },
              div({ className: "perf-photon-message-bar-warning-icon" }),
              Localized({ id: "perftools-status-restart-required" }),
              button(
                {
                  className: "perf-photon-button perf-photon-button-micro",
                  type: "button",
                  onClick: () => {
                    restartBrowserWithEnvironmentVariable({
                      [promptEnvRestart]: "1",
                    });
                  },
                },
                Localized({ id: "perftools-button-restart" })
              )
            )
          )
        : null,

      openRemoteDevTools
        ? div(
            { className: "perf-back" },
            button(
              {
                className: "perf-back-button",
                type: "button",
                onClick: openRemoteDevTools,
              },
              Localized({ id: "perftools-button-save-settings" })
            )
          )
        : null,

      div(
        { className: "perf-intro" },
        div(
          { className: "perf-intro-title-bar" },
          h1(
            { className: "perf-intro-title" },
            Localized({ id: "perftools-intro-title" })
          ),
          h(MoreActionsButton)
        ),
        div(
          { className: "perf-intro-row" },
          div({}, div({ className: "perf-intro-icon" })),
          Localized({
            className: "perf-intro-text",
            id: "perftools-intro-description",
          })
        )
      ),
      Presets(),
      Settings()
    );
  }
}

/**
 * @param {StoreState} state
 * @returns {StateProps}
 */
function mapStateToProps(state) {
  return {
    isSupportedPlatform: selectors.getIsSupportedPlatform(state),
    pageContext: selectors.getPageContext(state),
    promptEnvRestart: selectors.getPromptEnvRestart(state),
    openRemoteDevTools: selectors.getOpenRemoteDevTools(state),
  };
}

module.exports = connect(mapStateToProps)(AboutProfiling);
