/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check
/* exported gInit, gDestroy, loader */

/**
 * @typedef {import("../@types/perf").PerfFront} PerfFront
 * @typedef {import("../@types/perf").PreferenceFront} PreferenceFront
 * @typedef {import("../@types/perf").RecordingSettings} RecordingSettings
 * @typedef {import("../@types/perf").PageContext} PageContext
 * @typedef {import("../@types/perf").PanelWindow} PanelWindow
 * @typedef {import("../@types/perf").Store} Store
 * @typedef {import("../@types/perf").ProfileCaptureResult} ProfileCaptureResult
 * @typedef {import("../@types/perf").ProfilerViewMode} ProfilerViewMode
 * @typedef {import("../@types/perf").RootTraits} RootTraits
 */
"use strict";

{
  // Create the browser loader, but take care not to conflict with
  // TypeScript. See devtools/client/performance-new/typescript.md and
  // the section on "Do not overload require" for more information.

  const { BrowserLoader } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/browser-loader.sys.mjs"
  );
  const browserLoader = BrowserLoader({
    baseURI: "resource://devtools/client/performance-new/",
    window,
  });

  /**
   * @type {any} - Coerce the current scope into an `any`, and assign the
   *     loaders to the scope. They can then be used freely below.
   */
  const scope = this;
  scope.require = browserLoader.require;
  scope.loader = browserLoader.loader;
}

const ReactDOM = require("resource://devtools/client/shared/vendor/react-dom.mjs");
const React = require("resource://devtools/client/shared/vendor/react.mjs");
const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const {
  FluentL10n,
} = require("resource://devtools/client/shared/fluent-l10n/fluent-l10n.js");
const Provider = React.createFactory(
  require("resource://devtools/client/shared/vendor/react-redux.js").Provider
);
const LocalizationProvider = React.createFactory(
  FluentReact.LocalizationProvider
);
const DevToolsPanel = React.createFactory(
  require("resource://devtools/client/performance-new/components/panel/DevToolsPanel.js")
);
const ProfilerEventHandling = React.createFactory(
  require("resource://devtools/client/performance-new/components/panel/ProfilerEventHandling.js")
);
const ProfilerPreferenceObserver = React.createFactory(
  require("resource://devtools/client/performance-new/components/shared/ProfilerPreferenceObserver.js")
);
const createStore = require("resource://devtools/client/shared/redux/create-store.js");
const selectors = require("resource://devtools/client/performance-new/store/selectors.js");
const reducers = require("resource://devtools/client/performance-new/store/reducers.js");
const actions = require("resource://devtools/client/performance-new/store/actions.js");
const {
  openProfilerTab,
} = require("resource://devtools/client/performance-new/shared/browser.js");
const { createLocalSymbolicationService } = ChromeUtils.importESModule(
  "resource://devtools/shared/performance-new/symbolication.sys.mjs"
);
const { registerProfileCaptureForBrowser } = ChromeUtils.importESModule(
  "resource://devtools/client/performance-new/shared/background.sys.mjs"
);
const { presets, getProfilerViewModeForCurrentPreset } =
  ChromeUtils.importESModule(
    "resource://devtools/shared/performance-new/prefs-presets.sys.mjs"
  );

/**
 * This file initializes the DevTools Panel UI. It is in charge of initializing
 * the DevTools specific environment, and then passing those requirements into
 * the UI.
 */

/**
 * Initialize the panel by creating a redux store, and render the root component.
 *
 * @param {PerfFront} perfFront - The Perf actor's front. Used to start and stop recordings.
 * @param {RootTraits} traits - The traits coming from the root actor. This
 *                              makes it possible to change some code path
 *                              depending on the server version.
 * @param {PageContext} pageContext - The context that the UI is being loaded in under.
 * @param {(() => void)} openAboutProfiling - Optional call to open about:profiling
 */
async function gInit(perfFront, traits, pageContext, openAboutProfiling) {
  const store = createStore(reducers);
  const isSupportedPlatform = await perfFront.isSupportedPlatform();
  const supportedFeatures = await perfFront.getSupportedFeatures();

  {
    // Expose the store as a global, for testing.
    const anyWindow = /** @type {any} */ (window);
    const panelWindow = /** @type {PanelWindow} */ (anyWindow);
    // The store variable is a `ReduxStore`, not our `Store` type, as defined
    // in perf.d.ts. Coerce it into the `Store` type.
    const anyStore = /** @type {any} */ (store);
    panelWindow.gStore = anyStore;
  }

  const l10n = new FluentL10n();
  await l10n.init([
    "devtools/client/perftools.ftl",
    // For -brand-shorter-name used in some profiler preset descriptions.
    "branding/brand.ftl",
    // Needed for the onboarding UI
    "devtools/client/toolbox-options.ftl",
    "toolkit/branding/brandings.ftl",
  ]);

  // Do some initialization, especially with privileged things that are part of the
  // the browser.
  store.dispatch(
    actions.initializeStore({
      isSupportedPlatform,
      presets,
      supportedFeatures,
      pageContext,
    })
  );

  /**
   * @param {MockedExports.ProfileAndAdditionalInformation | null} profileAndAdditionalInformation
   * @param {Error | string} [error]
   */
  const onProfileReceived = async (profileAndAdditionalInformation, error) => {
    const objdirs = selectors.getObjdirs(store.getState());
    const profilerViewMode = getProfilerViewModeForCurrentPreset(pageContext);
    const browser = await openProfilerTab({ profilerViewMode });

    if (error || !profileAndAdditionalInformation) {
      if (!error) {
        error =
          "No profile data has been passed to onProfileReceived, and no specific error has been specified. This is unexpected.";
      }
      /**
       * @type {ProfileCaptureResult}
       */
      const profileCaptureResult = {
        type: "ERROR",
        error: typeof error === "string" ? new Error(error) : error,
      };
      registerProfileCaptureForBrowser(browser, profileCaptureResult, null);
      return;
    }

    const { profile, additionalInformation } = profileAndAdditionalInformation;
    const sharedLibraries = additionalInformation?.sharedLibraries ?? [];
    if (!sharedLibraries.length) {
      console.error(
        `[devtools perf] No shared libraries information have been retrieved from the profiled target, this is unexpected.`
      );
    }
    const symbolicationService = createLocalSymbolicationService(
      sharedLibraries,
      objdirs,
      perfFront
    );

    /**
     * @type {ProfileCaptureResult}
     */
    const profileCaptureResult = { type: "SUCCESS", profile };

    registerProfileCaptureForBrowser(
      browser,
      profileCaptureResult,
      symbolicationService
    );
  };

  const onEditSettingsLinkClicked = openAboutProfiling;

  ReactDOM.render(
    Provider(
      { store },
      LocalizationProvider(
        { bundles: l10n.getBundles() },
        React.createElement(
          React.Fragment,
          null,
          ProfilerEventHandling({ perfFront, traits }),
          ProfilerPreferenceObserver(),
          DevToolsPanel({
            perfFront,
            onProfileReceived,
            onEditSettingsLinkClicked,
          })
        )
      )
    ),
    document.querySelector("#root")
  );

  window.addEventListener("unload", () => gDestroy(), { once: true });
}

function gDestroy() {
  const root = document.querySelector("#root");
  if (root) {
    ReactDOM.unmountComponentAtNode(root);
  }
}
