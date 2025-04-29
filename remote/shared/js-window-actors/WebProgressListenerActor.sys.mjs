/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

let registered = false;
export function isWebProgressListenerActorRegistered() {
  return registered;
}

/**
 * Register the WebProgressListener actor that will keep track of all ongoing
 * navigations.
 */
export function registerWebProgressListenerActor() {
  if (registered) {
    return;
  }

  try {
    ChromeUtils.registerWindowActor("WebProgressListener", {
      kind: "JSWindowActor",
      parent: {
        esModuleURI:
          "chrome://remote/content/shared/js-window-actors/WebProgressListenerParent.sys.mjs",
      },
      child: {
        esModuleURI:
          "chrome://remote/content/shared/js-window-actors/WebProgressListenerChild.sys.mjs",
        events: {
          DOMWindowCreated: {},
        },
      },
      allFrames: true,
      messageManagerGroups: ["browsers"],
    });
    registered = true;

    // Ensure the WebProgress listener is started in existing contexts.
    for (const browser of lazy.TabManager.browsers) {
      if (!browser?.browsingContext) {
        continue;
      }

      for (const context of browser.browsingContext.getAllBrowsingContextsInSubtree()) {
        if (!context.currentWindowGlobal) {
          continue;
        }

        context.currentWindowGlobal
          .getActor("WebProgressListener")
          // Note that "createActor" is not explicitly referenced in the child
          // actor, this is only used to trigger the creation of the actor.
          .sendAsyncMessage("createActor");
      }
    }
  } catch (e) {
    if (e.name === "NotSupportedError") {
      lazy.logger.warn(`WebProgressListener actor is already registered!`);
    } else {
      throw e;
    }
  }
}

export function unregisterWebProgressListenerActor() {
  if (!registered) {
    return;
  }
  ChromeUtils.unregisterWindowActor("WebProgressListener");
  registered = false;
}
