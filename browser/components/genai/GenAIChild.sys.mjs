/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * JSWindowActor to detect content page events to send GenAI related data.
 */
export class GenAIChild extends JSWindowActorChild {
  actorCreated() {
    this.document.addEventListener("selectionchange", this);
  }

  didDestroy() {
    this.document.removeEventListener("selectionchange", this);
  }

  handleEvent(event) {
    const sendHide = () => this.sendQuery("GenAI:HideShortcuts", event.type);
    switch (event.type) {
      case "mousemove":
        // Track the pointer's screen position to avoid container positioning
        this.lastX = event.screenX;
        this.lastY = event.screenY;
        break;
      case "resize":
      case "scroll":
        // Hide if selection might have shifted away from shortcuts
        sendHide();
        break;
      case "selectionchange": {
        const selection = this.contentWindow.getSelection().toString().trim();
        if (!selection) {
          sendHide();
          break;
        }
        this.sendQuery("GenAI:SelectionChange", {
          x: this.lastX ?? 0,
          y: this.lastY ?? 0,
          selection,
        });
        break;
      }
    }
  }
}
