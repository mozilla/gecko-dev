/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

import { UrlbarUtils } from "resource:///modules/UrlbarUtils.sys.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  UrlbarUtils.getLogger({ prefix: "UrlbarSearchTermsPersistence" })
);

const URLBAR_PERSISTENCE_SETTINGS_KEY = "urlbar-persisted-search-terms";

/**
 * Provides utilities to manage and validate search terms persistence in the URL
 * bar. This class is designed to handle the identification of default search
 * engine results pages (SERPs), retrieval of search terms, and validation of
 * conditions for persisting search terms based on predefined provider
 * information.
 */
class _UrlbarSearchTermsPersistence {
  // Whether or not this class is initialised.
  #initialized = false;

  // The original provider information, mainly used for tests.
  #originalProviderInfo = [];

  // The current search provider info.
  #searchProviderInfo = [];

  // An instance of remote settings that is used to access the provider info.
  #urlbarSearchTermsPersistenceSettings;

  // Callback used when syncing Urlbar Search Terms Persistence config settings.
  #urlbarSearchTermsPersistenceSettingsSync;

  async init() {
    if (this.#initialized) {
      return;
    }

    this.#urlbarSearchTermsPersistenceSettings = lazy.RemoteSettings(
      URLBAR_PERSISTENCE_SETTINGS_KEY
    );
    let rawProviderInfo = [];
    try {
      rawProviderInfo = await this.#urlbarSearchTermsPersistenceSettings.get();
    } catch (ex) {
      lazy.logger.error("Could not get settings:", ex);
    }

    this.#urlbarSearchTermsPersistenceSettingsSync = event =>
      this.#onSettingsSync(event);
    this.#urlbarSearchTermsPersistenceSettings.on(
      "sync",
      this.#urlbarSearchTermsPersistenceSettingsSync
    );

    this.#originalProviderInfo = rawProviderInfo;
    this.#setSearchProviderInfo(rawProviderInfo);

    this.#initialized = true;
  }

  uninit() {
    if (!this.#initialized) {
      return;
    }

    try {
      this.#urlbarSearchTermsPersistenceSettings.off(
        "sync",
        this.#urlbarSearchTermsPersistenceSettingsSync
      );
    } catch (ex) {
      lazy.logger.error(
        "Failed to shutdown UrlbarSearchTermsPersistence Remote Settings.",
        ex
      );
    }
    this.#urlbarSearchTermsPersistenceSettings = null;
    this.#urlbarSearchTermsPersistenceSettingsSync = null;

    this.#initialized = false;
  }

  getSearchProviderInfo() {
    return this.#searchProviderInfo;
  }

  /**
   * Test-only function, used to override the provider information, so that
   * unit tests can set it to easy to test values.
   *
   * @param {Array} providerInfo
   *   An array of provider information to set.
   */
  overrideSearchTermsPersistenceForTests(providerInfo) {
    let info = providerInfo ? providerInfo : this.#originalProviderInfo;
    this.#setSearchProviderInfo(info);
  }

  /**
   * Determines if the URIs represent an application provided search
   * engine results page (SERP) and retrieves the search terms used.
   *
   * @param {nsIURI} originalURI
   *   The fallback URI to check. Used if `currentURI` is not provided or if
   *   conditions require fallback.
   * @param {nsIURI} currentURI
   *   The primary URI that is checked to determine if it matches the expected
   *   structure of a default SERP.
   * @returns {string}
   *   The search terms used.
   *   Will return an empty string if it's not a default SERP, the search term
   *   looks too similar to a URL, the string exceeds the maximum characters,
   *   or the default engine hasn't been initialized.
   */
  getSearchTerm(originalURI, currentURI) {
    if (
      !Services.search.hasSuccessfullyInitialized ||
      (!originalURI && !currentURI)
    ) {
      return "";
    }

    if (!originalURI) {
      originalURI = currentURI;
    }

    if (!currentURI) {
      currentURI = originalURI;
    }

    // Avoid inspecting URIs if they are non-http(s).
    if (!/^https?:\/\//.test(originalURI.spec)) {
      return "";
    }

    // Since we may have to use both URIs ensure they are similar.
    if (
      originalURI.prePath !== currentURI.prePath ||
      originalURI.filePath !== currentURI.filePath
    ) {
      return "";
    }

    let searchTerm = "";

    // If we have a provider, we have specific rules for dealing and can
    // understand changes to params.
    let provider = this.#getProviderInfoForURL(currentURI.spec);
    if (provider) {
      let result = Services.search.parseSubmissionURL(currentURI.spec);
      if (
        !result.engine?.isAppProvided ||
        !this.#shouldPersist(currentURI, provider)
      ) {
        return "";
      }
      searchTerm = result.terms;
    } else {
      // For all other providers, we use originalURI, because it doesn't change
      // and if it matches what the Engine would've generated, it means it's
      // a SERP.
      let result = Services.search.parseSubmissionURL(originalURI.spec);
      if (!result.engine?.isAppProvided) {
        return "";
      }
      searchTerm = result.engine.searchTermFromResult(originalURI);
    }

    if (!searchTerm || searchTerm.length > UrlbarUtils.MAX_TEXT_LENGTH) {
      return "";
    }

    let searchTermWithSpacesRemoved = searchTerm.replaceAll(/\s/g, "");

    // Check if the search string uses a commonly used URL protocol. This
    // avoids doing a fixup if we already know it matches a URL. Additionally,
    // it ensures neither http:// nor https:// will appear by themselves in
    // UrlbarInput. This is important because http:// can be trimmed, which in
    // the Persisted Search Terms case, will cause the UrlbarInput to appear
    // blank.
    if (
      searchTermWithSpacesRemoved.startsWith("https://") ||
      searchTermWithSpacesRemoved.startsWith("http://")
    ) {
      return "";
    }

    // We pass the search term to URIFixup to determine if it could be
    // interpreted as a URL, including typos in the scheme and/or the domain
    // suffix. This is to prevent search terms from persisting in the Urlbar if
    // they look too similar to a URL, but still allow phrases with periods
    // that are unlikely to be a URL.
    try {
      let info = Services.uriFixup.getFixupURIInfo(
        searchTermWithSpacesRemoved,
        Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
          Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP
      );
      if (info.keywordAsSent) {
        return searchTerm;
      }
    } catch (e) {}

    return "";
  }

  async #onSettingsSync(event) {
    let current = event.data?.current;
    if (current) {
      lazy.logger.debug("Update provider info due to Remote Settings sync.");
      this.#originalProviderInfo = current;
      this.#setSearchProviderInfo(current);
    } else {
      lazy.logger.debug(
        "Ignoring Remote Settings sync data due to missing records."
      );
    }
    Services.obs.notifyObservers(null, "urlbar-persisted-search-terms-synced");
  }

  /**
   * Used to set the local version of the search provider information.
   * This automatically maps the regexps to RegExp objects so that
   * we don't have to create a new instance each time.
   *
   * @param {Array} providerInfo
   *   A raw array of provider information to set.
   */
  #setSearchProviderInfo(providerInfo) {
    this.#searchProviderInfo = providerInfo.map(provider => {
      let newProvider = {
        ...provider,
        searchPageRegexp: new RegExp(provider.searchPageRegexp),
      };
      return newProvider;
    });
  }

  /**
   * Searches for provider information for a given url.
   *
   * @param {string} url The url to match for a provider.
   * @returns {Array | null} Returns an array of provider name and the provider
   *   information.
   */
  #getProviderInfoForURL(url) {
    return this.#searchProviderInfo.find(info =>
      info.searchPageRegexp.test(url)
    );
  }

  /**
   * Determines whether the search terms in the provided URL should be persisted
   * based on predefined criteria.
   *
   * @param {nsIURI} currentURI
   *   The current URI
   * @param {Array} provider
   *   An array of provider information
   * @returns {string | null} Returns null if there is no provider match, an
   *   empty string if search terms should not be persisted, or the value of the
   *   first matched query parameter to be persisted.
   */
  #shouldPersist(currentURI, provider) {
    let searchParams;
    try {
      searchParams = new URL(currentURI.spec).searchParams;
    } catch (ex) {
      return false;
    }
    if (provider.includeParams) {
      let foundMatch = false;
      for (let param of provider.includeParams) {
        // The param might not be present on page load.
        if (param.canBeMissing && !searchParams.has(param.key)) {
          foundMatch = true;
          break;
        }

        // If we didn't provide a specific param value,
        // the presence of the name is sufficient.
        if (searchParams.has(param.key) && !param.values?.length) {
          foundMatch = true;
          break;
        }

        let value = searchParams.get(param.key);
        // The param name and value must be present.
        if (value && param?.values.includes(value)) {
          foundMatch = true;
          break;
        }
      }
      if (!foundMatch) {
        return false;
      }
    }

    if (provider.excludeParams) {
      for (let param of provider.excludeParams) {
        let value = searchParams.get(param.key);
        // If we found a value for a key but didn't
        // provide a specific value to match.
        if (!param.values?.length && value) {
          return false;
        }
        // If we provided a value and it was present.
        if (param.values?.includes(value)) {
          return false;
        }
      }
    }
    return true;
  }
}

export var UrlbarSearchTermsPersistence = new _UrlbarSearchTermsPersistence();
