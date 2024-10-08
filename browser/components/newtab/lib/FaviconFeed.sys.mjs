/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionTypes as at } from "resource://activity-stream/common/Actions.mjs";
import { getDomain } from "resource://activity-stream/lib/TippyTopProvider.sys.mjs";

// We use importESModule here instead of static import so that
// the Karma test environment won't choke on this module. This
// is because the Karma test environment already stubs out
// RemoteSettings, and overrides importESModule to be a no-op (which
// can't be done for a static import statement).

// eslint-disable-next-line mozilla/use-static-import
const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

const MIN_FAVICON_SIZE = 96;

export class FaviconFeed {
  constructor() {
    this._queryForRedirects = new Set();
  }

  /**
   * fetchIcon attempts to fetch a rich icon for the given url from two sources.
   * First, it looks up the tippy top feed, if it's still missing, then it queries
   * the places for rich icon with its most recent visit in order to deal with
   * the redirected visit. See Bug 1421428 for more details.
   */
  async fetchIcon(url) {
    // Avoid initializing and fetching icons if prefs are turned off
    if (!this.shouldFetchIcons) {
      return;
    }

    const site = await this.getSite(getDomain(url));
    if (!site) {
      if (!this._queryForRedirects.has(url)) {
        this._queryForRedirects.add(url);
        Services.tm.idleDispatchToMainThread(() =>
          this.fetchIconFromRedirects(url)
        );
      }
      return;
    }

    let iconUri = Services.io.newURI(site.image_url);
    // The #tippytop is to be able to identify them for telemetry.
    iconUri = iconUri.mutate().setRef("tippytop").finalize();
    await this.#setFaviconForPage(Services.io.newURI(url), iconUri);
  }

  /**
   * Get the site tippy top data from Remote Settings.
   */
  async getSite(domain) {
    const sites = await this.tippyTop.get({
      filters: { domain },
      syncIfEmpty: false,
    });
    return sites.length ? sites[0] : null;
  }

  /**
   * Get the tippy top collection from Remote Settings.
   */
  get tippyTop() {
    if (!this._tippyTop) {
      this._tippyTop = RemoteSettings("tippytop");
    }
    return this._tippyTop;
  }

  /**
   * Determine if we should be fetching and saving icons.
   */
  get shouldFetchIcons() {
    return Services.prefs.getBoolPref("browser.chrome.site_icons");
  }

  onAction(action) {
    switch (action.type) {
      case at.RICH_ICON_MISSING:
        this.fetchIcon(action.data.url);
        break;
    }
  }

  /**
   * Get favicon info (uri and size) for a uri from Places.
   *
   * @param uri {nsIURI} Page to check for favicon data
   * @returns A promise of an object (possibly null) containing the data
   */
  getFaviconInfo(uri) {
    return new Promise(resolve =>
      lazy.PlacesUtils.favicons.getFaviconDataForPage(
        uri,
        // Package up the icon data in an object if we have it; otherwise null
        (iconUri, faviconLength, favicon, mimeType, faviconSize) =>
          resolve(iconUri ? { iconUri, faviconSize } : null),
        lazy.NewTabUtils.activityStreamProvider.THUMB_FAVICON_SIZE
      )
    );
  }

  /**
   * Fetch favicon for a url by following its redirects in Places.
   *
   * This can improve the rich icon coverage for Top Sites since Places only
   * associates the favicon to the final url if the original one gets redirected.
   * Note this is not an urgent request, hence it is dispatched to the main
   * thread idle handler to avoid any possible performance impact.
   */
  async fetchIconFromRedirects(url) {
    const visitPaths = await this.#fetchVisitPaths(url);
    if (visitPaths.length > 1) {
      const lastVisit = visitPaths.pop();
      const redirectedUri = Services.io.newURI(lastVisit.url);
      const iconInfo = await this.getFaviconInfo(redirectedUri);
      if (iconInfo?.faviconSize >= MIN_FAVICON_SIZE) {
        try {
          lazy.PlacesUtils.favicons.copyFavicons(
            redirectedUri,
            Services.io.newURI(url),
            lazy.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE
          );
        } catch (ex) {
          console.error(`Failed to copy favicon [${ex}]`);
        }
      }
    }
  }

  /**
   * Get favicon data for given URL from network.
   *
   * @param {nsIURI} faviconURI
   *        nsIURI for the favicon.
   * @return {nsIURI} data URL
   */
  async getFaviconDataURLFromNetwork(faviconURI) {
    let channel = lazy.NetUtil.newChannel({
      uri: faviconURI,
      loadingPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      securityFlags:
        Ci.nsILoadInfo.SEC_REQUIRE_CORS_INHERITS_SEC_CONTEXT |
        Ci.nsILoadInfo.SEC_ALLOW_CHROME |
        Ci.nsILoadInfo.SEC_DISALLOW_SCRIPT,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_INTERNAL_IMAGE_FAVICON,
    });

    let resolver = Promise.withResolvers();

    lazy.NetUtil.asyncFetch(channel, async (input, status, request) => {
      if (!Components.isSuccessCode(status)) {
        resolver.resolve();
        return;
      }

      try {
        let data = lazy.NetUtil.readInputStream(input, input.available());
        let { contentType } = request.QueryInterface(Ci.nsIChannel);
        input.close();

        let buffer = new Uint8ClampedArray(data);
        let blob = new Blob([buffer], { type: contentType });
        let dataURL = await new Promise((resolve, reject) => {
          let reader = new FileReader();
          reader.addEventListener("load", () => resolve(reader.result));
          reader.addEventListener("error", reject);
          reader.readAsDataURL(blob);
        });
        resolver.resolve(Services.io.newURI(dataURL));
      } catch (e) {
        resolver.reject(e);
      }
    });

    return resolver.promise;
  }

  /**
   * Set favicon for page.
   *
   * @param {nsIURI} pageURI
   * @param {nsIURI} faviconURI
   */
  async #setFaviconForPage(pageURI, faviconURI) {
    try {
      // If the given faviconURI is data URL, set it as is.
      if (faviconURI.schemeIs("data")) {
        lazy.PlacesUtils.favicons
          .setFaviconForPage(pageURI, faviconURI, faviconURI)
          .catch(console.error);
        return;
      }

      // Try to find the favicon data from DB.
      const faviconInfo = await this.getFaviconInfo(pageURI);
      if (faviconInfo?.faviconSize) {
        // As valid favicon data is already stored for the page,
        // we don't have to update.
        return;
      }

      // Otherwise, fetch from network.
      lazy.PlacesUtils.favicons
        .setFaviconForPage(
          pageURI,
          faviconURI,
          await this.getFaviconDataURLFromNetwork(faviconURI)
        )
        .catch(console.error);
    } catch (ex) {
      console.error(`Failed to set favicon for page:${ex}`);
    }
  }

  /**
   * Fetches visit paths for a given URL from its most recent visit in Places.
   *
   * Note that this includes the URL itself as well as all the following
   * permenent&temporary redirected URLs if any.
   *
   * @param {String} a URL string
   *
   * @returns {Array} Returns an array containing objects as
   *   {int}    visit_id: ID of the visit in moz_historyvisits.
   *   {String} url: URL of the redirected URL.
   */
  async #fetchVisitPaths(url) {
    const query = `
    WITH RECURSIVE path(visit_id)
    AS (
      SELECT v.id
      FROM moz_places h
      JOIN moz_historyvisits v
        ON v.place_id = h.id
      WHERE h.url_hash = hash(:url) AND h.url = :url
        AND v.visit_date = h.last_visit_date

      UNION

      SELECT id
      FROM moz_historyvisits
      JOIN path
        ON visit_id = from_visit
      WHERE visit_type IN
        (${lazy.PlacesUtils.history.TRANSITIONS.REDIRECT_PERMANENT},
         ${lazy.PlacesUtils.history.TRANSITIONS.REDIRECT_TEMPORARY})
    )
    SELECT visit_id, (
      SELECT (
        SELECT url
        FROM moz_places
        WHERE id = place_id)
      FROM moz_historyvisits
      WHERE id = visit_id) AS url
    FROM path
  `;

    const visits =
      await lazy.NewTabUtils.activityStreamProvider.executePlacesQuery(query, {
        columns: ["visit_id", "url"],
        params: { url },
      });
    return visits;
  }
}
