/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import {
  SearchEngine,
  EngineURL,
} from "resource://gre/modules/SearchEngine.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

/**
 * @typedef FormInfo
 *
 * Information about a search engine. This is similar to the WebExtension
 * style object used by `SearchEngine._initWithDetails` but it contains a
 * FormData object so it can easily be generated from an HTML form.
 *
 * Either `url` or `formData` must contain {searchTerms}.
 *
 * @property {string} url
 *   The url template for searches.
 * @property {string} name
 *   The name of the engine.
 * @property {FormData} [formData]
 *   The search parameters. May only contain string values.
 * @property {string} [charset]
 *   The encoding for the requests. Defaults to `SearchUtils.DEFAULT_QUERY_CHARSET`.
 * @property {string} [method]
 *   The HTTP method. Defaults to GET.
 * @property {string} [icon]
 *   A URL to the engine's icon.
 * @property {string} [alias]
 *   An engine keyword.
 * @property {string} [suggestUrl]
 *   The url template for suggestions.
 */

/**
 * UserSearchEngine represents a search engine defined by a user or generated
 * from a web page.
 */
export class UserSearchEngine extends SearchEngine {
  /**
   * Creates a UserSearchEngine from either a FormInfo object or JSON settings.
   *
   * @param {object} options
   *   The options for this search engine.
   * @param {FormInfo} [options.formInfo]
   *   General information about the search engine.
   * @param {object} [options.json]
   *   An object that represents the saved JSON settings for the engine.
   */
  constructor(options = {}) {
    super({
      loadPath: "[user]",
    });

    if (options.formInfo) {
      this.#initWithFormInfo(options.formInfo);
    } else {
      this._initWithJSON(options.json);
    }
  }

  /**
   * Generates a search engine from a FormInfo object.
   * The alias is treated as a user-defined alias.
   *
   * @param {FormInfo} formInfo
   *   General information about the search engine.
   */
  #initWithFormInfo(formInfo) {
    this._name = formInfo.name.trim();

    let url = new EngineURL(
      lazy.SearchUtils.URL_TYPE.SEARCH,
      formInfo.method ?? "GET",
      formInfo.url
    );
    for (let [key, value] of formInfo.formData ?? []) {
      if (typeof value != "string") {
        throw new Error("Non-string values are not supported.");
      }
      url.addParam(
        Services.textToSubURI.ConvertAndEscape(formInfo.charset, key),
        Services.textToSubURI
          .ConvertAndEscape(formInfo.charset, value)
          .replaceAll("%7BsearchTerms%7D", "{searchTerms}")
      );
    }
    this._urls.push(url);

    if (formInfo.suggestUrl) {
      let suggestUrl = new EngineURL(
        lazy.SearchUtils.URL_TYPE.SUGGEST_JSON,
        "GET",
        formInfo.suggestUrl
      );
      this._urls.push(suggestUrl);
    }

    if (formInfo.icon) {
      this._setIcon(formInfo.icon);
    }
    if (formInfo.charset) {
      this._queryCharset = formInfo.charset;
    }
    this.alias = formInfo.alias;
  }

  /**
   * Returns the appropriate identifier to use for telemetry.
   *
   * @returns {string}
   */
  get telemetryId() {
    return `other-${this.name}`;
  }
}
