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

// Additional events to listen with others to create the actor in BrowserGlue
const EVENTS = ["mousedown", "mouseup", "pagehide"];

/**
 * JSWindowActor to detect content page events to send GenAI related data.
 */
export class GenAIChild extends JSWindowActorChild {
  mouseUpTimeout = null;
  downTime = Date.now();
  downSelection = null;
  debounceDelay = 200;

  actorCreated() {
    this.document.addEventListener("selectionchange", this);
    // Use capture as some pages might stop the events
    EVENTS.forEach(ev => this.contentWindow.addEventListener(ev, this, true));
  }

  didDestroy() {
    this.document.removeEventListener("selectionchange", this);
    EVENTS.forEach(ev =>
      this.contentWindow?.removeEventListener(ev, this, true)
    );
  }

  handleEvent(event) {
    const sendHide = () =>
      this.sendAsyncMessage("GenAI:HideShortcuts", event.type);
    switch (event.type) {
      case "mousedown":
        this.downSelection = this.getSelectionInfo().selection;
        this.downTime = Date.now();
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

        this.mouseUpTimeout = this.contentWindow.setTimeout(() => {
          const selectionInfo = this.getSelectionInfo();
          const delay = Date.now() - this.downTime;

          // Only send a message if there's a new selection or a long press
          if (
            (selectionInfo.selection &&
              selectionInfo.selection !== this.downSelection) ||
            delay > lazy.shortcutsDelay
          ) {
            this.sendAsyncMessage("GenAI:ShowShortcuts", {
              ...selectionInfo,
              delay,
              screenXDevPx: event.screenX * this.contentWindow.devicePixelRatio,
              screenYDevPx: event.screenY * this.contentWindow.devicePixelRatio,
            });
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
