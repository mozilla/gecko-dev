/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EscapablePageParent } from "resource://gre/actors/NetErrorParent.sys.mjs";

export class AboutHttpsOnlyErrorParent extends EscapablePageParent {
  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "goBack":
        this.leaveErrorPage(this.browser);
        break;
    }
  }
}
