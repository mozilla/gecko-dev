/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  getBrowsingContextDetails:
    "chrome://remote/content/shared/messagehandler/transports/BrowsingContextUtils.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  truncate: "chrome://remote/content/shared/Format.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

export class WebProgressListenerChild extends JSWindowActorChild {
  #listener;
  #webProgress;

  constructor() {
    super();

    this.#listener = {
      onLocationChange: this.#onLocationChange,
      onStateChange: this.#onStateChange,
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),
    };
    this.#webProgress = null;
  }

  actorCreated() {
    this.#webProgress = this.manager.browsingContext.docShell
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebProgress);

    this.#webProgress.addProgressListener(
      this.#listener,
      Ci.nsIWebProgress.NOTIFY_LOCATION |
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );
  }

  didDestroy() {
    try {
      this.#webProgress.removeProgressListener(this.#listener);
    } catch (e) {
      // Ignore potential errors if the window global was already destroyed.
    }
  }

  // Note: we rely on events and messages to trigger the actor creation, but
  // all the logic is in the actorCreated callback. The handleEvent and
  // receiveMessage methods are only there as placeholders to avoid errors.

  /**
   * See note above
   */
  handleEvent() {}

  /**
   * See note above
   */
  receiveMessage() {}

  #getTargetURI(request) {
    try {
      return request.QueryInterface(Ci.nsIChannel).originalURI;
    } catch (e) {}

    return null;
  }

  #onLocationChange = (progress, request, location, stateFlags) => {
    const context = progress.browsingContext;

    if (context !== this.manager.browsingContext) {
      // Skip notifications which don't relate to this browsing context
      // (eg frames).
      return;
    }

    if (stateFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
      const payload = {
        contextDetails: lazy.getBrowsingContextDetails(context),
        url: location.spec,
      };

      if (
        // history.pushState / replaceState
        progress.loadType & Ci.nsIDocShell.LOAD_CMD_PUSHSTATE ||
        // moving to a history entry created by pushState / replaceState
        (progress.loadType & Ci.nsIDocShell.LOAD_CMD_HISTORY &&
          // TODO: We need to only select history traversals which are not
          // fragment navigations. However we don't have a flag dedicated to
          // such traversals, they are identical to same document + same hash
          // navigations.
          stateFlags ===
            Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)
      ) {
        this.#trace(
          lazy.truncate`Location=historyUpdated: ${location.spec}`,
          context.id
        );
        this.sendAsyncMessage(
          "WebProgressListenerChild:historyUpdated",
          payload
        );
        return;
      }

      if (location.hasRef) {
        // If the target URL contains a hash, handle the navigation as a
        // fragment navigation.
        this.#trace(
          context.id,
          lazy.truncate`Location=fragmentNavigated: ${location.spec}`
        );

        this.sendAsyncMessage(
          "WebProgressListenerChild:fragmentNavigated",
          payload
        );
        return;
      }

      this.#trace(
        context.id,
        lazy.truncate`Location=sameDocumentChanged: ${location.spec}`
      );

      this.sendAsyncMessage(
        "WebProgressListenerChild:sameDocumentChanged",
        payload
      );
    }
  };

  #onStateChange = (progress, request, stateFlags, status) => {
    const context = progress.browsingContext;
    const targetURI = this.#getTargetURI(request);

    const isBindingAborted = status == Cr.NS_BINDING_ABORTED;
    const isStart = !!(stateFlags & Ci.nsIWebProgressListener.STATE_START);
    const isStop = !!(stateFlags & Ci.nsIWebProgressListener.STATE_STOP);

    if (lazy.Log.isTraceLevelOrMore) {
      const isNetwork = !!(
        stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK
      );
      this.#trace(
        context.id,
        `Loading state: flags: ${stateFlags}, status: ${status}, ` +
          ` isStart: ${isStart}, isStop: ${isStop}, isNetwork: ${isNetwork},` +
          ` isBindingAborted: ${isBindingAborted},` +
          lazy.truncate` targetURI: ${targetURI?.spec}`
      );
    }

    try {
      if (isStart) {
        this.sendAsyncMessage("WebProgressListenerChild:navigationStarted", {
          contextDetails: lazy.getBrowsingContextDetails(context),
          url: targetURI?.spec,
        });

        return;
      }

      if (isStop && !isBindingAborted) {
        // Skip NS_BINDING_ABORTED state changes as this can happen during a
        // browsing context + process change and we should get the real stop state
        // change from the correct process later.
        this.sendAsyncMessage("WebProgressListenerChild:navigationStopped", {
          contextDetails: lazy.getBrowsingContextDetails(context),
          status,
          url: targetURI?.spec,
        });
      }
    } catch (e) {
      if (e.name === "InvalidStateError") {
        // We'll arrive here if we no longer have our manager, so we can
        // just swallow this error.
        return;
      }
      throw e;
    }
  };

  #trace(contextId, message) {
    lazy.logger.trace(`[${contextId}] ${this.constructor.name} ${message}`);
  }
}
