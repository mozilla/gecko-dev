/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { UITour } from "moz-src:///browser/components/uitour/UITour.sys.mjs";
import { UITourUtils } from "moz-src:///browser/components/uitour/UITourUtils.sys.mjs";

export class UITourParent extends JSWindowActorParent {
  receiveMessage(message) {
    if (!UITourUtils.ensureTrustedOrigin(this.manager)) {
      return;
    }
    switch (message.name) {
      case "UITour:onPageEvent":
        if (this.manager.rootFrameLoader) {
          let browser = this.manager.rootFrameLoader.ownerElement;
          UITour.onPageEvent(message.data, browser);
          break;
        }
    }
  }
}
