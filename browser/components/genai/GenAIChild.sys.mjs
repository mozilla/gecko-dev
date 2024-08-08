/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Additional events to listen with others to create the actor in BrowserGlue
const EVENTS = ["mousedown", "mouseup"];

/**
 * JSWindowActor to detect content page events to send GenAI related data.
 */
export class GenAIChild extends JSWindowActorChild {
  actorCreated() {
    this.document.addEventListener("selectionchange", this);
    EVENTS.forEach(ev => this.contentWindow.addEventListener(ev, this));
  }

  didDestroy() {
    this.document.removeEventListener("selectionchange", this);
    EVENTS.forEach(ev => this.contentWindow?.removeEventListener(ev, this));
  }

  handleEvent(event) {
    const sendHide = () => this.sendQuery("GenAI:HideShortcuts", event.type);
    switch (event.type) {
      case "mousedown":
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

        // Show immediately on selection or allow long press with no selection
        const selection =
          this.contentWindow.getSelection()?.toString().trim() ?? "";
        if (selection || Date.now() - (this.downTime ?? 0) > 200) {
          this.sendQuery("GenAI:ShowShortcuts", {
            x: event.screenX,
            y: event.screenY,
            selection,
          });
        }
        break;
      }
      case "resize":
      case "scroll":
      case "selectionchange":
        // Hide if selection might have shifted away from shortcuts
        sendHide();
        break;
    }
  }
}
