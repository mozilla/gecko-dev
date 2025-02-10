/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionTypes as at } from "resource://newtab/common/Actions.mjs";
import { FaviconProvider } from "resource:///modules/topsites/TopSites.sys.mjs";

export class FaviconFeed {
  constructor() {
    this.faviconProvider = new FaviconProvider();
  }

  onAction(action) {
    switch (action.type) {
      case at.RICH_ICON_MISSING:
        this.faviconProvider.fetchIcon(action.data.url);
        break;
    }
  }
}
