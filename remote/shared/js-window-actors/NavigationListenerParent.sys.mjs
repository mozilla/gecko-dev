/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  notifyFragmentNavigated:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  notifySameDocumentChanged:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  notifyNavigationFailed:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  notifyNavigationStarted:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
  notifyNavigationStopped:
    "chrome://remote/content/shared/NavigationManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

export class NavigationListenerParent extends JSWindowActorParent {
  async receiveMessage(message) {
    const { data, name } = message;

    try {
      const payload = {
        contextDetails: data.contextDetails,
        url: data.url,
      };

      switch (name) {
        case "NavigationListenerChild:fragmentNavigated": {
          lazy.notifyFragmentNavigated(payload);
          break;
        }
        case "NavigationListenerChild:sameDocumentChanged": {
          lazy.notifySameDocumentChanged(payload);
          break;
        }
        case "NavigationListenerChild:navigationFailed": {
          lazy.notifyNavigationFailed(payload);
          break;
        }
        case "NavigationListenerChild:navigationStarted": {
          lazy.notifyNavigationStarted(payload);
          break;
        }
        case "NavigationListenerChild:navigationStopped": {
          lazy.notifyNavigationStopped(payload);
          break;
        }
        default:
          throw new Error("Unsupported message:" + name);
      }
    } catch (e) {
      if (e instanceof TypeError) {
        // Avoid error spam from errors due to unavailable browsing contexts.
        lazy.logger.trace(
          `Failed to handle a navigation listener message: ${e.message}`
        );
      } else {
        throw e;
      }
    }
  }
}
