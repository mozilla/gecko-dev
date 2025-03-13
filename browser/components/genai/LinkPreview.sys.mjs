/* eslint-disable no-console */
/**
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  LinkPreviewModel:
    "moz-src:///browser/components/genai/LinkPreviewModel.sys.mjs",
});
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gLinkPreviewEnabled",
  "browser.ml.linkPreview.enabled",
  false,
  (_pref, _old, val) => LinkPreview.onEnabledPref(val)
);

export const LinkPreview = {
  keyboardComboActive: false,
  _windowStates: new Map(),

  /**
   * Handles the preference change for enabling/disabling Link Preview.
   * It adds or removes event listeners for all tracked windows based on the new preference value.
   *
   * @param {boolean} enabled - The new state of the Link Preview preference.
   */
  onEnabledPref(enabled) {
    const method = enabled ? "_addEventListeners" : "_removeEventListeners";
    for (const win of this._windowStates.keys()) {
      this[method](win);
    }
  },

  /**
   * Handles startup tasks such as telemetry and adding listeners.
   *
   * @param {Window} win - The window context used to add event listeners.
   */
  init(win) {
    this._windowStates.set(win, {});

    if (lazy.gLinkPreviewEnabled) {
      this._addEventListeners(win);
    }
  },

  /**
   * Teardown the Link Preview feature for the given window.
   * Removes event listeners from the specified window and removes it from the window map.
   *
   * @param {Window} win - The window context to uninitialize.
   */
  teardown(win) {
    // Remove event listeners from the specified window
    if (lazy.gLinkPreviewEnabled) {
      this._removeEventListeners(win);
    }

    // Remove the window from the map
    this._windowStates.delete(win);
  },

  /**
   * Adds all needed event listeners and updates the state.
   *
   * @param {Window} win - The window to which event listeners are added.
   */
  _addEventListeners(win) {
    win.addEventListener("OverLink", this, true);
    win.addEventListener("keydown", this, true);
    win.addEventListener("keyup", this, true);
  },

  /**
   * Removes all event listeners and updates the state.
   *
   * @param {Window} win - The window from which event listeners are removed.
   */
  _removeEventListeners(win) {
    win.removeEventListener("OverLink", this, true);
    win.removeEventListener("keydown", this, true);
    win.removeEventListener("keyup", this, true);
  },

  /**
   * Handles keyboard events ("keydown" and "keyup") for the Link Preview feature.
   * Adjusts the state of keyboardComboActive based on modifier keys.
   *
   * @param {KeyboardEvent} event - The keyboard event to be processed.
   */
  handleEvent(event) {
    switch (event.type) {
      case "keydown":
        this._onKeyDown(event);
        break;
      case "keyup":
        this._onKeyUp(event);
        break;
      case "OverLink":
        this._onLinkPreview(event);
        break;
      default:
        break;
    }
  },

  /**
   * Handles "keydown" events.
   *
   * @param {KeyboardEvent} event - The keyboard event to be processed.
   */
  _onKeyDown(event) {
    const win = event.currentTarget;
    if (event.altKey) {
      if (!this.keyboardComboActive) {
        this.keyboardComboActive = true;
        this._maybeLinkPreview(win);
      }
    }
  },

  /**
   * Handles "keyup" events.
   *
   * @param {KeyboardEvent} event - The keyboard event to be processed.
   */
  _onKeyUp(event) {
    const win = event.currentTarget;
    // Clear the flag when the Alt key is released.
    if (!event.altKey) {
      if (this.keyboardComboActive) {
        this.keyboardComboActive = false;
        this._maybeLinkPreview(win);
      }
    }
  },

  /**
   * Handles "OverLink" events.
   * Stores the hovered link URL in the per-window state object and processes the
   * link preview if the keyboard combination is active.
   *
   * @param {CustomEvent} event - The event object containing details about the link preview.
   */
  _onLinkPreview(event) {
    const win = event.currentTarget;
    const url = event.detail.url;

    // Store the current overLink in the per-window state object.
    const stateObject = this._windowStates.get(win);
    stateObject.overLink = url;

    if (this.keyboardComboActive) {
      this._maybeLinkPreview(win);
    }
  },
  /**
   * Determines whether to process or cancel the link preview based on the current state.
   * If a URL is available and the keyboard combination is active, it processes the link preview.
   * Otherwise, it cancels the link preview.
   *
   * @param {Window} win - The window context in which the link preview may occur.
   */
  async _maybeLinkPreview(win) {
    const stateObject = this._windowStates.get(win);
    const url = stateObject.overLink;

    if (url && this.keyboardComboActive) {
      console.log(`Previewing link: ${url}`);
      const browsingContext = win.browsingContext;
      const actor = browsingContext.currentWindowGlobal.getActor("LinkPreview");
      //TODO: use result from sendQuery below for link preview rendering
      //TODO: figure out how to get read duration data from Reader mode
      const result = await actor.fetchPageData(url);
      console.log(result);
      console.log("Generating text AI...");
      lazy.LinkPreviewModel.generateTextAI(result.article.textContent, {
        onError: console.error,
        onText: console.log,
      });
    }
  },
};
