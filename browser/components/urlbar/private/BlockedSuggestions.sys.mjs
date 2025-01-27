/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { SuggestFeature } from "resource:///modules/urlbar/private/SuggestFeature.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  TaskQueue: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

/**
 * A set of blocked suggestion URLs for Suggest. This feature is always enabled
 * as long as Suggest is enabled.
 */
export class BlockedSuggestions extends SuggestFeature {
  constructor() {
    super();
    this.#taskQueue = new lazy.TaskQueue();
    lazy.UrlbarPrefs.addObserver(this);
  }

  enable(enabled) {
    if (enabled) {
      this.#loadDigests();
    }
  }

  /**
   * Blocks a result's URL.
   *
   * @param {UrlbarResult} result
   *   The URL of this result will be blocked.
   */
  async blockResult(result) {
    // For some Suggest results, `url` is a value that is modified at query time
    // and that is potentially unique per query. For example, it might contain
    // timestamps or query-related search params. Those results will also have
    // an `originalUrl` that is the unmodified URL, and it should be used for
    // blocking purposes.
    await this.add(result.payload.originalUrl || result.payload.url);
  }

  /**
   * Returns true if a result's URL is blocked.
   *
   * @param {UrlbarResult} result
   *   The result to check.
   * @returns {boolean}
   *   Whether the result's URL is blocked.
   */
  async isResultBlocked(result) {
    // See `blockResult()` for a note on `originalUrl`.
    let isBlocked = await this.has(
      result.payload.originalUrl || result.payload.url
    );
    return isBlocked;
  }

  /**
   * Blocks a URL. Callers should use `blockResult()` instead when they have a
   * `UrlbarResult`.
   *
   * @param {string} originalUrl
   *   The URL to block. In cases where a URL is potentially unique to a query,
   *   this value should be the original unmodified URL.
   */
  async add(originalUrl) {
    await this.#taskQueue.queue(async () => {
      this.logger.info("Blocking suggestion", { originalUrl });
      let digest = await this.#getDigest(originalUrl);
      this.logger.debug("Got digest", { originalUrl, digest });
      this.#digests.add(digest);
      let json = JSON.stringify([...this.#digests]);
      this.#updatingDigests = true;
      try {
        lazy.UrlbarPrefs.set("quicksuggest.blockedDigests", json);
      } finally {
        this.#updatingDigests = false;
      }
      this.logger.debug("All blocked suggestions", json);
    });
  }

  /**
   * Returns true if a URL is blocked. Callers should use `isResultBlocked()`
   * instead when they have a `UrlbarResult`.
   *
   * @param {string} originalUrl
   *   The URL to check. In cases where a URL is potentially unique to a query,
   *   this value should be the original unmodified URL.
   * @returns {boolean}
   *   Whether the URL is blocked.
   */
  async has(originalUrl) {
    return this.#taskQueue.queue(async () => {
      let digest = await this.#getDigest(originalUrl);
      return this.#digests.has(digest);
    });
  }

  /**
   * Unblocks all URLs.
   */
  async clear() {
    await this.#taskQueue.queue(() => {
      this.logger.info("Clearing all blocked suggestions");
      this.#digests.clear();
      lazy.UrlbarPrefs.clear("quicksuggest.blockedDigests");
    });
  }

  /**
   * Called when a urlbar pref changes.
   *
   * @param {string} pref
   *   The name of the pref relative to `browser.urlbar`.
   */
  onPrefChanged(pref) {
    switch (pref) {
      case "quicksuggest.blockedDigests":
        if (!this.#updatingDigests) {
          this.logger.debug(
            "browser.urlbar.quicksuggest.blockedDigests changed"
          );
          this.#loadDigests();
        }
        break;
    }
  }

  /**
   * Loads blocked suggestion digests from the pref into `#digests`.
   */
  async #loadDigests() {
    await this.#taskQueue.queue(() => {
      let json = lazy.UrlbarPrefs.get("quicksuggest.blockedDigests");
      if (!json) {
        this.#digests.clear();
      } else {
        try {
          this.#digests = new Set(JSON.parse(json));
        } catch (error) {
          this.logger.error("Error loading blocked suggestion digests", error);
        }
      }
    });
  }

  /**
   * Returns the SHA-1 digest of a string as a 40-character hex-encoded string.
   *
   * @param {string} string
   *   The string to convert to SHA-1
   * @returns {string}
   *   The hex-encoded digest of the given string.
   */
  async #getDigest(string) {
    let stringArray = new TextEncoder().encode(string);
    let hashBuffer = await crypto.subtle.digest("SHA-1", stringArray);
    let hashArray = new Uint8Array(hashBuffer);
    return Array.from(hashArray, b => b.toString(16).padStart(2, "0")).join("");
  }

  get _test_readyPromise() {
    return this.#taskQueue.emptyPromise;
  }

  get _test_digests() {
    return this.#digests;
  }

  _test_getDigest(string) {
    return this.#getDigest(string);
  }

  // Set of digests of the original URLs of blocked suggestions. For some
  // Suggest results, `url` is a value that is modified at query time and that
  // is potentially unique per query. For example, it might contain timestamps
  // or query-related search params. Those results will also have an
  // `originalUrl` that is the unmodified URL, and it should be used for
  // blocking purposes. For details on the digests, see `#getDigest()`.
  //
  // The only reason we use URL digests is that suggestions currently do not
  // have persistent IDs. We could use the URLs themselves but SHA-1 digests are
  // only 40 chars long, so they save a little space. This is also consistent
  // with how blocked tiles on the newtab page are stored, but they use MD5. We
  // do *not* store digests for any security or obfuscation reason.
  //
  // This value is serialized as a JSON'ed array to the
  // `browser.urlbar.quicksuggest.blockedDigests` pref.
  #digests = new Set();

  // Used to serialize access to blocked suggestions. This is only necessary
  // because getting a suggestion's URL digest is async.
  #taskQueue = null;

  // Whether blocked digests are currently being updated.
  #updatingDigests = false;
}
