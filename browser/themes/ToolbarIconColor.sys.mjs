/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Module is used set the "brighttext" attribute to `true` or remove it
 * based on calculating a luminance value from the current toolbar color.
 * This causes items like icons on the toolbar to contrast in brightness
 * enough to be visible, depending on the current theme/coloring of the browser
 * window. Calculated luminance values are cached in `state.toolbarLuminanceCache`. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

// Track individual windowstates using WeakMap
const _windowStateMap = new WeakMap();

export const ToolbarIconColor = {
  init(window) {
    if (_windowStateMap.has(window)) {
      return;
    }

    const state = {
      active: false,
      fullscreen: false,
      customtitlebar: false,
      toolbarLuminanceCache: new Map(),
    };

    _windowStateMap.set(window, state);

    window.addEventListener("nativethemechange", this);
    window.addEventListener("activate", this);
    window.addEventListener("deactivate", this);
    window.addEventListener("toolbarvisibilitychange", this);
    window.addEventListener("windowlwthemeupdate", this);

    // If the window isn't active now, we assume that it has never been active
    // before and will soon become active such that inferFromText will be
    // called from the initial activate event.
    if (Services.focus.activeWindow == window) {
      this.inferFromText("activate", window);
    }
  },

  uninit(window) {
    const state = _windowStateMap.get(window);
    if (!state) {
      return;
    }

    window.removeEventListener("nativethemechange", this);
    window.removeEventListener("activate", this);
    window.removeEventListener("deactivate", this);
    window.removeEventListener("toolbarvisibilitychange", this);
    window.removeEventListener("windowlwthemeupdate", this);

    _windowStateMap.delete(window);
  },

  handleEvent(event) {
    const window = event.target.ownerGlobal;
    switch (event.type) {
      case "activate":
      case "deactivate":
      case "nativethemechange":
      case "windowlwthemeupdate":
        this.inferFromText(event.type, window);
        break;
      case "toolbarvisibilitychange":
        this.inferFromText(event.type, window, event.visible);
        break;
    }
  },

  inferFromText(reason, window, reasonValue) {
    const state = _windowStateMap.get(window);

    if (!state) {
      return;
    }

    switch (reason) {
      case "activate": // falls through
      case "deactivate":
        state.active = reason === "activate";
        break;
      case "fullscreen":
        state.fullscreen = reasonValue;
        break;
      case "nativethemechange":
      case "windowlwthemeupdate":
        // theme change, we'll need to recalculate all color values
        state.toolbarLuminanceCache.clear();
        break;
      case "toolbarvisibilitychange":
        // toolbar changes dont require reset of the cached color values
        break;
      case "customtitlebar":
        state.customtitlebar = reasonValue;
        break;
    }

    let toolbarSelector = ".browser-toolbar:not([collapsed=true])";
    if (AppConstants.platform == "macosx") {
      toolbarSelector += ":not([type=menubar])";
    }

    // The getComputedStyle calls and setting the brighttext are separated in
    // two loops to avoid flushing layout and making it dirty repeatedly.
    let cachedLuminances = state.toolbarLuminanceCache;
    let luminances = new Map();
    for (let toolbar of window.document.querySelectorAll(toolbarSelector)) {
      // toolbars *should* all have ids, but guard anyway to avoid blowing up
      let cacheKey = toolbar.id && toolbar.id + JSON.stringify(state);
      // lookup cached luminance value for this toolbar in this window state
      let luminance = cacheKey && cachedLuminances.get(cacheKey);
      if (isNaN(luminance)) {
        let { r, g, b } = InspectorUtils.colorToRGBA(
          window.getComputedStyle(toolbar).color
        );
        luminance = 0.2125 * r + 0.7154 * g + 0.0721 * b;
        if (cacheKey) {
          cachedLuminances.set(cacheKey, luminance);
        }
      }
      luminances.set(toolbar, luminance);
    }

    const luminanceThreshold = 127; // In between 0 and 255
    for (let [toolbar, luminance] of luminances) {
      if (luminance <= luminanceThreshold) {
        toolbar.removeAttribute("brighttext");
      } else {
        toolbar.setAttribute("brighttext", "true");
      }
    }
  },
};
