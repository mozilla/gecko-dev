/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "shortcutsDelay",
  "browser.ml.chat.shortcuts.longPress"
);

// Events to register after shortcuts are shown
const HIDE_EVENTS = ["pagehide", "resize", "scroll"];

/**
 * JSWindowActor to detect content page events to send GenAI related data.
 */
export class GenAIChild extends JSWindowActorChild {
  mouseUpTimeout = null;
  downSelection = null;
  downTimeStamp = 0;
  debounceDelay = 200;
  pendingHide = false;

  registerHideEvents() {
    this.document.addEventListener("selectionchange", this);
    HIDE_EVENTS.forEach(ev =>
      this.contentWindow.addEventListener(ev, this, true)
    );
    this.pendingHide = true;
  }

  removeHideEvents() {
    this.document.removeEventListener("selectionchange", this);
    HIDE_EVENTS.forEach(ev =>
      this.contentWindow?.removeEventListener(ev, this, true)
    );
    this.pendingHide = false;
  }

  handleEvent(event) {
    const sendHide = () => {
      // Only remove events and send message if shortcuts are actually visible
      if (this.pendingHide) {
        this.sendAsyncMessage("GenAI:HideShortcuts", event.type);
        this.removeHideEvents();
      }
    };

    switch (event.type) {
      case "mousedown":
        this.downSelection = this.getSelectionInfo().selection;
        this.downTimeStamp = event.timeStamp;
        sendHide();
        break;
      case "mouseup": {
        // Only handle plain clicks
        if (
          event.button ||
          event.altKey ||
          event.ctrlKey ||
          event.metaKey ||
          event.shiftKey
        ) {
          return;
        }

        // Clear any previously scheduled mouseup actions
        if (this.mouseUpTimeout) {
          this.contentWindow.clearTimeout(this.mouseUpTimeout);
        }

        const { screenX, screenY } = event;

        this.mouseUpTimeout = this.contentWindow.setTimeout(() => {
          const selectionInfo = this.getSelectionInfo();
          const delay = event.timeStamp - this.downTimeStamp;

          // Only send a message if there's a new selection or a long press
          if (
            (selectionInfo.selection &&
              selectionInfo.selection !== this.downSelection) ||
            delay > lazy.shortcutsDelay
          ) {
            this.sendAsyncMessage("GenAI:ShowShortcuts", {
              ...selectionInfo,
              delay,
              screenXDevPx: screenX * this.contentWindow.devicePixelRatio,
              screenYDevPx: screenY * this.contentWindow.devicePixelRatio,
            });
            this.registerHideEvents();
          }

          // Clear the timeout reference after execution
          this.mouseUpTimeout = null;
        }, this.debounceDelay);

        break;
      }
      case "pagehide":
      case "resize":
      case "scroll":
      case "selectionchange":
        // Hide if selection might have shifted away from shortcuts
        sendHide();
        break;
    }
  }

  /**
   * Provide the selected text and input type.
   *
   * @returns {object} selection info
   */
  getSelectionInfo() {
    // Handle regular selection outside of inputs
    const { activeElement } = this.document;
    const selection = this.contentWindow.getSelection()?.toString().trim();
    if (selection) {
      return {
        inputType: activeElement.closest("[contenteditable]")
          ? "contenteditable"
          : "",
        selection,
      };
    }

    // Selection within input elements
    const { selectionStart, value } = activeElement;
    if (selectionStart != null && value != null) {
      return {
        inputType: activeElement.localName,
        selection: value.slice(selectionStart, activeElement.selectionEnd),
      };
    }
    return { inputType: "", selection: "" };
  }
}
