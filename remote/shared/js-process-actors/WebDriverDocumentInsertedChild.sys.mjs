/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  getBrowsingContextDetails:
    "chrome://remote/content/shared/messagehandler/transports/BrowsingContextUtils.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  truncate: "chrome://remote/content/shared/Log.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

export class WebDriverDocumentInsertedChild extends JSProcessActorChild {
  actorCreated() {
    lazy.logger.trace(
      `WebDriverDocumentInsertedChild actor created for PID ${Services.appinfo.processID}`
    );
  }

  observe = async (subject, topic) => {
    if (topic === "initial-document-element-inserted") {
      const window = subject?.defaultView;
      // Ignore events without a window (such as from SVG documents).
      if (!window) {
        return;
      }
      const context = window.browsingContext;
      const url = window.location.href;
      const payload = {
        contextDetails: lazy.getBrowsingContextDetails(context),
        url,
      };

      try {
        this.sendAsyncMessage(
          "WebDriverDocumentInsertedChild:documentInserted",
          payload
        );
      } catch {
        lazy.logger.trace(
          lazy.truncate`Could not send WebDriverDocumentInsertedChild:documentInserted for URL: ${url}`
        );
      }
    }
  };
}
