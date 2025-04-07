/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { UITourUtils } from "moz-src:///browser/components/uitour/UITourUtils.sys.mjs";

export class UITourChild extends JSWindowActorChild {
  handleEvent(event) {
    if (!UITourUtils.ensureTrustedOrigin(this.manager)) {
      return;
    }

    this.sendAsyncMessage("UITour:onPageEvent", {
      detail: event.detail,
      type: event.type,
      pageVisibilityState: this.document.visibilityState,
    });
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "UITour:SendPageCallback":
        this.sendPageEvent("Response", aMessage.data);
        break;
      case "UITour:SendPageNotification":
        this.sendPageEvent("Notification", aMessage.data);
        break;
    }
  }

  sendPageEvent(type, detail) {
    if (!UITourUtils.ensureTrustedOrigin(this.manager)) {
      return;
    }

    let win = this.contentWindow;
    let eventName = "mozUITour" + type;
    let event = new win.CustomEvent(eventName, {
      bubbles: true,
      detail: Cu.cloneInto(detail, win),
    });
    win.document.dispatchEvent(event);
  }
}
