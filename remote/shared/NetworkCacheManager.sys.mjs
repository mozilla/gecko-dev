/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowsingContextListener:
    "chrome://remote/content/shared/listeners/BrowsingContextListener.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

/**
 * The NetworkCacheManager is responsible for managing the cache status (enabling/disabling cache)
 * for navigables. It's meant to be a singleton, and the consumers can use the exported
 * methods to change the cache status or perform the state cleanup.
 *
 * @class NetworkCacheManager
 */
class NetworkCacheManager {
  #contextListener;
  #defaultCacheBypass;
  #navigableCacheBypassSet;

  constructor() {
    this.#contextListener = new lazy.BrowsingContextListener();
    this.#contextListener.on("attached", this.#onContextAttached);

    this.#defaultCacheBypass = false;
    // WeakSet of navigables in which network caches are bypassed.
    this.#navigableCacheBypassSet = new WeakSet();
  }

  destroy() {
    this.#contextListener.off("attached", this.#onContextAttached);
    this.#contextListener.destroy();

    this.cleanup();
  }

  #getLoadFlags(bypassValue) {
    return bypassValue
      ? Ci.nsIRequest.LOAD_BYPASS_CACHE
      : Ci.nsIRequest.LOAD_NORMAL;
  }

  #onContextAttached = (eventName, data = {}) => {
    if (this.#defaultCacheBypass) {
      this.#setLoadFlagsForBrowsingContext(
        data.browsingContext,
        this.#getLoadFlags(true)
      );
    }
  };

  #setDefaultCacheBypass(value) {
    if (this.#defaultCacheBypass === value) {
      return;
    }

    this.#defaultCacheBypass = value;

    const loadFlags = this.#getLoadFlags(value);

    // Update cache settings for all existing navigables.
    for (const browser of lazy.TabManager.browsers) {
      this.#setLoadFlagsForBrowsingContext(browser.browsingContext, loadFlags);
    }

    // In case the cache is globally disabled we have to listen to all
    // newly attached contexts and disable cache for them.
    if (value) {
      this.#contextListener.startListening();
    } else {
      this.#contextListener.stopListening();
    }
  }

  #setLoadFlagsForBrowsingContext(browsingContext, loadFlags) {
    if (browsingContext.defaultLoadFlags !== loadFlags) {
      browsingContext.defaultLoadFlags = loadFlags;
    }
  }

  /**
   * Reset network cache bypassing logic.
   */
  cleanup() {
    this.#setDefaultCacheBypass(false);

    if (this.#navigableCacheBypassSet.size > 0) {
      const loadFlags = this.#getLoadFlags(false);

      for (const navigable of this.#navigableCacheBypassSet) {
        this.#setLoadFlagsForBrowsingContext(navigable, loadFlags);
      }

      this.#navigableCacheBypassSet.clear();
    }
  }

  /**
   * Set network cache bypassing logic to a provided value
   * and optionally specified contexts.
   *
   * @param {boolean} bypass
   *     The flag to enable or disable bypassing of the network cache.
   * @param {Array<BrowsingContext>=} contexts
   *     The list of browsing contexts where the network cache
   *     should be bypassed.
   *
   * @throws {UnsupportedOperationError}
   *     If unsupported configuration is passed.
   */
  setCacheBypass(bypass, contexts = null) {
    if (contexts === null) {
      // TODO: Bug 1905307. Add support for such case.
      if (
        ChromeUtils.nondeterministicGetWeakSetKeys(
          this.#navigableCacheBypassSet
        ).length
      ) {
        throw new lazy.error.UnsupportedOperationError(
          "Updating cache bypassing globally when the cache is already enabled for individual contexts is not supported yet"
        );
      }
      this.#setDefaultCacheBypass(bypass);
    } else {
      // TODO: Bug 1905307. Add support for such case.
      if (this.#defaultCacheBypass) {
        throw new lazy.error.UnsupportedOperationError(
          "Updating cache bypassing for individual contexts when it's already enabled globally is not supported yet"
        );
      }

      const loadFlags = this.#getLoadFlags(bypass);

      for (const context of contexts) {
        if (bypass) {
          this.#navigableCacheBypassSet.add(context);
        } else {
          this.#navigableCacheBypassSet.delete(context);
        }

        this.#setLoadFlagsForBrowsingContext(context, loadFlags);
      }
    }
  }
}

// Create a private NetworkCacheManager singleton.
const networkCacheManager = new NetworkCacheManager();

export function updateCacheBypassStatus(bypass, contexts) {
  return networkCacheManager.setCacheBypass(bypass, contexts);
}

export function cleanupCacheBypassState() {
  return networkCacheManager.cleanup();
}
