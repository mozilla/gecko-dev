/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-shadow: error, mozilla/no-aArgs: error */

import {
  SearchEngine,
  EngineURL,
} from "moz-src:///toolkit/components/search/SearchEngine.sys.mjs";
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = XPCOMUtils.declareLazy({
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  SearchUtils: "moz-src:///toolkit/components/search/SearchUtils.sys.mjs",
});

/**
 * @typedef FormInfo
 *
 * Information about a search engine. This is similar to the WebExtension
 * style object used by `SearchEngine._initWithDetails` but with a
 * URLSearchParams object so it can easily be generated from an HTML form.
 *
 * Either `url` or `params` must contain {searchTerms}.
 *
 * @property {string} name
 *   The name of the engine.
 * @property {string} url
 *   The url template for searches.
 * @property {URLSearchParams} [params]
 *   The parameters for searches.
 * @property {string} [charset]
 *   The encoding for the requests. Defaults to `SearchUtils.DEFAULT_QUERY_CHARSET`.
 * @property {string} [method]
 *   The HTTP method. Defaults to GET.
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
    let charset = formInfo.charset ?? lazy.SearchUtils.DEFAULT_QUERY_CHARSET;

    let url = new EngineURL(
      lazy.SearchUtils.URL_TYPE.SEARCH,
      formInfo.method ?? "GET",
      formInfo.url
    );
    for (let [key, value] of formInfo.params ?? []) {
      url.addParam(
        Services.textToSubURI.ConvertAndEscape(charset, key),
        Services.textToSubURI
          .ConvertAndEscape(charset, value)
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

    if (formInfo.charset) {
      this._queryCharset = formInfo.charset;
    }

    this.alias = formInfo.alias;
    this.updateFavicon();
  }

  /**
   * Returns the appropriate identifier to use for telemetry.
   *
   * @returns {string}
   */
  get telemetryId() {
    return `other-${this.name}`;
  }

  /**
   * Changes the name of the engine if the new name is available.
   *
   * @param {string} newName
   *   The new name.
   * @returns {boolean}
   *   Whether the name was changed successfully.
   */
  rename(newName) {
    if (newName == this.name) {
      return true;
    } else if (Services.search.getEngineByName(newName)) {
      return false;
    }
    this._name = newName;
    lazy.SearchUtils.notifyAction(this, lazy.SearchUtils.MODIFIED_TYPE.CHANGED);
    return true;
  }

  /**
   * Changes the url of the specified type.
   * The HTTP method is determined by whether postData is null.
   *
   * @param {string} type
   *   The type of url to change. Must be a `SearchUtils.URL_TYPE`.
   * @param {?string} template
   *    The URL to which search queries should be sent. Should contain
   *    "{searchTerms}" as the placeholder for the search terms for GET
   *    requests. Use null to remove the URL of the specified type.
   * @param {?string} postData
   *   x-www-form-urlencoded body containing "{searchTerms}" for POST or
   *   null for GET.
   */
  changeUrl(type, template, postData) {
    if (type == lazy.SearchUtils.URL_TYPE.SEARCH && !template) {
      throw new Error("Cannot remove search URL.");
    }

    // Remove existing URL.
    this._urls = this._urls.filter(url => url.type != type);

    if (template) {
      let method = postData ? "POST" : "GET";
      let url = new EngineURL(type, method, template);
      for (let [key, value] of new URLSearchParams(postData ?? "").entries()) {
        url.addParam(key, value);
      }
      this._urls.push(url);
    }

    // Notify about added/changed/removed URL.
    lazy.SearchUtils.notifyAction(this, lazy.SearchUtils.MODIFIED_TYPE.CHANGED);
  }

  /**
   * Replaces the current icon.
   *
   * @param {string} newIconURL
   */
  async changeIcon(newIconURL) {
    let [iconURL, size] = await this._downloadAndRescaleIcon(newIconURL);

    this._iconMapObj = {};
    this._addIconToMap(iconURL, size);
    lazy.SearchUtils.notifyAction(
      this,
      lazy.SearchUtils.MODIFIED_TYPE.ICON_CHANGED
    );
  }

  /**
   * Changes the icon to favicon of the search url origin and logs potential
   * errors.
   */
  updateFavicon() {
    let searchUrl = this._getURLOfType(lazy.SearchUtils.URL_TYPE.SEARCH);
    let searchUrlOrigin = new URL(searchUrl.template).origin;

    lazy.PlacesUtils.favicons
      .getFaviconForPage(Services.io.newURI(searchUrlOrigin))
      .then(iconURL => {
        if (iconURL) {
          this.changeIcon(iconURL.dataURI.spec);
        } else if (Object.keys(this._iconMapObj).length) {
          // There was an icon before but now there is none.
          // Remove previous icon in case the origin changed.
          this._iconMapObj = {};
          lazy.SearchUtils.notifyAction(
            this,
            lazy.SearchUtils.MODIFIED_TYPE.ICON_CHANGED
          );
        }
      })
      .catch(e =>
        console.warn(`Unable to change icon of engine ${this.name}:`, e.message)
      );
  }
}
