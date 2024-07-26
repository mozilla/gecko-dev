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
    switch (event.type) {
      case "mousemove":
      case "resize":
      case "scroll":
      case "selectionchange":
    }
  }
}
