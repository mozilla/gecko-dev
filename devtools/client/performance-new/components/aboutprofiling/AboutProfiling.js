/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {import("../../@types/perf").State} StoreState
 * @typedef {import("../../@types/perf").PerformancePref} PerformancePref
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

/** @type {PerformancePref["AboutProfilingHasDeveloperOptions"]} */
const ABOUTPROFILING_HAS_DEVELOPER_OPTIONS_PREF =
  "devtools.performance.aboutprofiling.has-developer-options";

/**
 * This function encodes the parameter so that it can be used as an environment
 * variable value.
 * Basically it uses single quotes, but replacing any single quote by '"'"':
 * 1. close the previous single-quoted string,
 * 2. add a double-quoted string containing only a single quote
 * 3. start a single-quoted string again.
 * so that it's properly retained.
 *
 * @param {string} value
 * @returns {string}
 */
function encodeShellValue(value) {
  return "'" + value.replaceAll("'", `'"'"'`) + "'";
}

/**
 * @typedef {import("../../@types/perf").RecordingSettings} RecordingSettings
 *
 * @typedef {Object} ButtonStateProps
 * @property {RecordingSettings} recordingSettings
 *
 * @typedef {ButtonStateProps} ButtonProps
 *
 * @typedef {Object} ButtonState
 * @property {boolean} hasDeveloperOptions
 */

/**
 * This component implements the button that triggers the menu that makes it
 * possible to show more actions.
 * @extends {React.PureComponent<ButtonProps, ButtonState>}
 */
class MoreActionsButtonImpl extends PureComponent {
  state = {
    hasDeveloperOptions: Services.prefs.getBoolPref(
      ABOUTPROFILING_HAS_DEVELOPER_OPTIONS_PREF,
      false
    ),
  };

  componentDidMount() {
    Services.prefs.addObserver(
      ABOUTPROFILING_HAS_DEVELOPER_OPTIONS_PREF,
      this.onHasDeveloperOptionsPrefChanges
    );
  }

  componentWillUnmount() {
    Services.prefs.removeObserver(
      ABOUTPROFILING_HAS_DEVELOPER_OPTIONS_PREF,
      this.onHasDeveloperOptionsPrefChanges
    );
  }
  _menuRef = createRef();

  onHasDeveloperOptionsPrefChanges = () => {
    this.setState({
      hasDeveloperOptions: Services.prefs.getBoolPref(
        ABOUTPROFILING_HAS_DEVELOPER_OPTIONS_PREF,
        false
      ),
    });
  };

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

  /**
   * @returns {Record<string, string>}
   */
  getEnvironmentVariablesForStartupFromRecordingSettings = () => {
    const { interval, entries, threads, features } =
      this.props.recordingSettings;
    return {
      MOZ_PROFILER_STARTUP: "1",
      MOZ_PROFILER_STARTUP_INTERVAL: String(interval),
      MOZ_PROFILER_STARTUP_ENTRIES: String(entries),
      MOZ_PROFILER_STARTUP_FEATURES: features.join(","),
      MOZ_PROFILER_STARTUP_FILTERS: threads.join(","),
    };
  };

  onRestartWithProfiling = () => {
    const envVariables =
      this.getEnvironmentVariablesForStartupFromRecordingSettings();
    restartBrowserWithEnvironmentVariable(envVariables);
  };

  onCopyEnvVariables = async () => {
    const envVariables =
      this.getEnvironmentVariablesForStartupFromRecordingSettings();
    const envString = Object.entries(envVariables)
      .map(([key, value]) => `${key}=${encodeShellValue(value)}`)
      .join(" ");
    await navigator.clipboard.writeText(envString);
  };

  onCopyTestVariables = async () => {
    const { interval, entries, threads, features } =
      this.props.recordingSettings;

    const envString =
      "--gecko-profile" +
      ` --gecko-profile-interval ${interval}` +
      ` --gecko-profile-entries ${entries}` +
      ` --gecko-profile-features ${encodeShellValue(features.join(","))}` +
      ` --gecko-profile-threads ${encodeShellValue(threads.join(","))}`;
    await navigator.clipboard.writeText(envString);
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
        Localized(
          { id: "perftools-menu-more-actions-restart-with-profiling" },
          h(
            "panel-item",
            { onClick: this.onRestartWithProfiling },
            "Restart Firefox with startup profiling enabled"
          )
        ),
        this.state.hasDeveloperOptions
          ? Localized(
              { id: "perftools-menu-more-actions-copy-for-startup" },
              h(
                "panel-item",
                { onClick: this.onCopyEnvVariables },
                "Copy environment variables for startup profiling"
              )
            )
          : null,
        this.state.hasDeveloperOptions
          ? Localized(
              { id: "perftools-menu-more-actions-copy-for-perf-tests" },
              h(
                "panel-item",
                { onClick: this.onCopyTestVariables },
                "Copy parameters for mach try perf"
              )
            )
          : null
      )
    );
  }
}

/**
 * @param {StoreState} state
 * @returns {ButtonStateProps}
 */
function mapStateToButtonProps(state) {
  return {
    recordingSettings: selectors.getRecordingSettings(state),
  };
}
const MoreActionsButton = connect(mapStateToButtonProps)(MoreActionsButtonImpl);

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
