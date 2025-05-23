/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let registered = false;

/**
 * Register the DocumentInserted actor that will propagate
 * initial-document-element-inserted notifications from content processes to the
 * parent process.
 */
export function registerWebDriverDocumentInsertedActor() {
  if (registered) {
    return;
  }

  ChromeUtils.registerProcessActor("WebDriverDocumentInserted", {
    kind: "JSProcessActor",
    parent: {
      esModuleURI:
        "chrome://remote/content/shared/js-process-actors/WebDriverDocumentInsertedParent.sys.mjs",
    },
    child: {
      esModuleURI:
        "chrome://remote/content/shared/js-process-actors/WebDriverDocumentInsertedChild.sys.mjs",
      observers: ["initial-document-element-inserted"],
    },
    includeParent: true,
  });
  registered = true;
}

export function unregisterWebDriverDocumentInsertedActor() {
  if (!registered) {
    return;
  }
  ChromeUtils.unregisterProcessActor("WebDriverDocumentInserted");
  registered = false;
}
