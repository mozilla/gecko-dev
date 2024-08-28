/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global browser */

// Telemetry values
const TELEMETRY_VALUE_EXTENSION = "extension";
const TELEMETRY_VALUE_SERVER = "server";

class AddonsSearchDetection {
  constructor() {
    // The key is an URL pattern to monitor and its corresponding value is a
    // list of add-on IDs.
    this.matchPatterns = {};

    this.onRedirectedListener = this.onRedirectedListener.bind(this);
  }

  async getMatchPatterns() {
    try {
      this.matchPatterns =
        await browser.addonsSearchDetection.getMatchPatterns();
    } catch (err) {
      console.error(`failed to retrieve the list of URL patterns: ${err}`);
      this.matchPatterns = {};
    }

    return this.matchPatterns;
  }

  // When the search service changes the set of engines that are enabled, we
  // update our pattern matching in the webrequest listeners (go to the bottom
  // of this file for the search service events we listen to).
  async monitor() {
    // If there is already a listener, remove it so that we can re-add one
    // after. This is because we're using the same listener with different URL
    // patterns (when the list of search engines changes).
    if (
      browser.addonsSearchDetection.onRedirected.hasListener(
        this.onRedirectedListener
      )
    ) {
      browser.addonsSearchDetection.onRedirected.removeListener(
        this.onRedirectedListener
      );
    }
    // If there is already a listener, remove it so that we can re-add one
    // after. This is because we're using the same listener with different URL
    // patterns (when the list of search engines changes).
    if (browser.webRequest.onBeforeRequest.hasListener(this.noOpListener)) {
      browser.webRequest.onBeforeRequest.removeListener(this.noOpListener);
    }

    // Retrieve the list of URL patterns to monitor with our listener.
    //
    // Note: search suggestions are system principal requests, so webRequest
    // cannot intercept them.
    const matchPatterns = await this.getMatchPatterns();
    const patterns = Object.keys(matchPatterns);

    if (patterns.length === 0) {
      return;
    }

    browser.webRequest.onBeforeRequest.addListener(
      this.noOpListener,
      { types: ["main_frame"], urls: patterns },
      ["blocking"]
    );

    browser.addonsSearchDetection.onRedirected.addListener(
      this.onRedirectedListener,
      { urls: patterns }
    );
  }

  // This listener is required to force the registration of traceable channels.
  noOpListener() {
    // Do nothing.
  }

  async onRedirectedListener({ addonId, firstUrl, lastUrl }) {
    // When we do not have an add-on ID (in the request property bag), we
    // likely detected a search server-side redirect.
    const maybeServerSideRedirect = !addonId;

    let addonIds = [];
    // Search server-side redirects are possible because an extension has
    // registered a search engine, which is why we can (hopefully) retrieve the
    // add-on ID.
    if (maybeServerSideRedirect) {
      addonIds = this.getAddonIdsForUrl(firstUrl);
    } else if (addonId) {
      addonIds = [addonId];
    }

    if (addonIds.length === 0) {
      // No add-on ID means there is nothing we can report.
      return;
    }

    // This is the monitored URL that was first redirected.
    const from = await browser.addonsSearchDetection.getPublicSuffix(firstUrl);
    // This is the final URL after redirect(s).
    const to = await browser.addonsSearchDetection.getPublicSuffix(lastUrl);

    if (from === to) {
      // We do not want to report redirects to same public suffixes. However,
      // we will report redirects from public suffixes belonging to a same
      // entity (.e.g., `example.com` -> `example.fr`).
      //
      // Known limitation: if a redirect chain starts and ends with the same
      // public suffix, we won't report any event, even if the chain contains
      // different public suffixes in between.
      return;
    }

    for (const id of addonIds) {
      const addonVersion = await browser.addonsSearchDetection.getAddonVersion(
        id
      );
      const extra = {
        addonId: id,
        addonVersion,
        from,
        to,
        value: maybeServerSideRedirect
          ? TELEMETRY_VALUE_SERVER
          : TELEMETRY_VALUE_EXTENSION,
      };
      browser.addonsSearchDetection.report(maybeServerSideRedirect, extra);
    }
  }

  getAddonIdsForUrl(url) {
    for (const pattern of Object.keys(this.matchPatterns)) {
      // `getMatchPatterns()` returns the prefix plus "*".
      const urlPrefix = pattern.slice(0, -1);

      if (url.startsWith(urlPrefix)) {
        return this.matchPatterns[pattern];
      }
    }

    return [];
  }
}

const exp = new AddonsSearchDetection();
exp.monitor();

browser.addonsSearchDetection.onSearchEngineModified.addListener(async () => {
  await exp.monitor();
});
