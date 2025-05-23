/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  notifyNavigationCommitted:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
});

export class WebDriverDocumentInsertedParent extends JSProcessActorParent {
  async receiveMessage(message) {
    const { data, name } = message;

    const payload = {
      contextDetails: data.contextDetails,
      url: data.url,
    };

    switch (name) {
      case "WebDriverDocumentInsertedChild:documentInserted": {
        lazy.notifyNavigationCommitted(payload);
        break;
      }
      default:
        throw new Error("Unsupported message:" + name);
    }
  }
}
