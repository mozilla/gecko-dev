/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  MerinoClient: "resource:///modules/MerinoClient.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

// Cache period for Merino's geolocation response. This is intentionally a small
// amount of time. See the `cachePeriodMs` discussion in `MerinoClient`.
const GEOLOCATION_CACHE_PERIOD_MS = 120000; // 2 minutes

// The mean Earth radius used in distance calculations.
const EARTH_RADIUS_KM = 6371.009;

/**
 * Utils for fetching the client's geolocation from Merino, computing distances
 * between locations, and finding suggestions that best match the geolocation.
 */
class _GeolocationUtils {
  constructor() {
    ChromeUtils.defineLazyGetter(this, "logger", () =>
      lazy.UrlbarUtils.getLogger({ prefix: "GeolocationUtils" })
    );
  }

  /**
   * Fetches the client's geolocation from Merino. Merino gets the geolocation
   * by looking up the client's IP address in its MaxMind database. We cache
   * responses for a brief period of time so that fetches during a urlbar
   * session don't ping Merino over and over.
   *
   * @returns {object}
   *   An object with the following properties (see Merino source for latest):
   *
   *   {string} country
   *     The full country name.
   *   {string} country_code
   *     The country ISO code.
   *   {string} region
   *     The full region name, e.g., the full name of a U.S. state.
   *   {string} region_code
   *     The region ISO code, e.g., the two-letter abbreviation for U.S. states.
   *   {string} city
   *     The city name.
   *   {object} location
   *     This object has the following properties:
   *     {number} latitude
   *       Latitude in decimal degrees.
   *     {number} longitude
   *       Longitude in decimal degrees.
   *     {number} radius
   *       Accuracy radius in km.
   */
  async geolocation() {
    if (!this.#merino) {
      this.#merino = new lazy.MerinoClient("GeolocationUtils", {
        cachePeriodMs: GEOLOCATION_CACHE_PERIOD_MS,
      });
    }

    this.logger.debug("Fetching geolocation from Merino");
    let results = await this.#merino.fetch({
      providers: ["geolocation"],
      query: "",
    });

    this.logger.debug("Got geolocation from Merino", results);

    return results?.[0]?.custom_details?.geolocation || null;
  }

  /**
   * Returns the item from an array of candidate items that best matches the
   * client's geolocation. For urlbar, typically the items are suggestions, but
   * they can be anything.
   *
   * The best item is determined as follows:
   *
   * 1. If any item locations include geographic coordinates, then the item with
   *    the closest location to the client's geolocation will be returned.
   * 2. If any item locations include regions and populations, then the item
   *    with the most populous location in the client's region will be returned.
   * 3. If any item locations include regions, then the first item with a
   *    location in the client's region will be returned.
   * 4. If any item locations include countries and populations, then the item
   *    with the most populous location in the client's country will be
   *    returned.
   * 5. If any item locations include countries, then the first item with the
   *    same country as the client will be returned.
   *
   * @param {Array} items
   *   Array of items, which can be anything.
   * @param {Function} locationFromItem
   *   A function that maps an item to its location. It will be called as
   *   `locationFromItem(item)` and it should return an object with the
   *   following properties, all optional:
   *
   *   {number} latitude
   *     The location's latitude in decimal coordinates.
   *   {number} longitude
   *     The location's longitude in decimal coordinates.
   *   {string} country
   *     The location's two-digit ISO country code. Case doesn't matter.
   *   {string} region
   *     The location's region, e.g., a U.S. state. This is compared to the
   *     `region_code` in the Merino geolocation response (case insensitive) so
   *     it should be the same format: the region ISO code, e.g., the two-letter
   *     abbreviation for U.S. states.
   *   {number} population
   *     The location's population.
   * @returns {object|null}
   *   The best item as described above, or null if `items` is empty.
   */
  async best(items, locationFromItem = i => i) {
    if (items.length <= 1) {
      return items[0] || null;
    }

    let geo = await this.geolocation();
    if (!geo) {
      return items[0];
    }
    return (
      this.#bestByDistance(geo, items, locationFromItem) ||
      this.#bestByRegion(geo, items, locationFromItem) ||
      items[0]
    );
  }

  /**
   * Returns the item with the city nearest the client's geolocation based on
   * the great-circle distance between the coordinates [1]. This isn't
   * necessarily super accurate, but that's OK since it's stable and accurate
   * enough to find a good matching item.
   *
   * [1] https://en.wikipedia.org/wiki/Great-circle_distance
   *
   * @param {object} geo
   *   The `geolocation` object returned by Merino's geolocation provider. It's
   *   expected to have at least the properties below, but we gracefully handle
   *   exceptions. The coordinates are expected to be in decimal and the radius
   *   is expected to be in km.
   *
   *   ```
   *   { location: { latitude, longitude, radius }}
   *   ```
   * @param {Array} items
   *   Array of items as described in the doc for `best()`.
   * @param {Function} locationFromItem
   *   Mapping function as described in the doc for `best()`.
   * @returns {object|null}
   *   The nearest item as described above. If there are multiple nearest items
   *   within the accuracy radius, the most populous one is returned. If the
   *   `geo` does not include a location or coordinates, null is returned.
   */
  #bestByDistance(geo, items, locationFromItem) {
    let geoLat = geo.location?.latitude;
    let geoLong = geo.location?.longitude;
    if (typeof geoLat != "number" || typeof geoLong != "number") {
      return null;
    }

    // All distances are in km.
    [geoLat, geoLong] = [geoLat, geoLong].map(toRadians);
    let geoLatSin = Math.sin(geoLat);
    let geoLatCos = Math.cos(geoLat);
    let geoRadius = geo.location?.radius || 5;

    let bestTuple;
    let dMin = Infinity;
    for (let item of items) {
      let location = locationFromItem(item);
      if (
        typeof location.latitude != "number" ||
        typeof location.longitude != "number"
      ) {
        continue;
      }
      let [itemLat, itemLong] = [location.latitude, location.longitude].map(
        toRadians
      );
      let d =
        EARTH_RADIUS_KM *
        Math.acos(
          geoLatSin * Math.sin(itemLat) +
            geoLatCos *
              Math.cos(itemLat) *
              Math.cos(Math.abs(geoLong - itemLong))
        );
      if (
        !bestTuple ||
        // The item's location is closer to the client than the best
        // location.
        d + geoRadius < dMin ||
        // The item's location is the same distance from the client as the
        // best location, i.e., the difference between the two distances is
        // within the accuracy radius. Choose the item if it has a larger
        // population.
        (Math.abs(d - dMin) <= geoRadius &&
          hasLargerPopulation(location, bestTuple.location))
      ) {
        dMin = d;
        bestTuple = { item, location };
      }
    }

    return bestTuple?.item || null;
  }

  /**
   * Returns the first item with a city located in the same region and country
   * as the client's geolocation. If there is no such item, the first item in
   * the same country is returned. If there is no item in the same country, null
   * is returned. Ties are broken by population.
   *
   * @param {object} geo
   *   The `geolocation` object returned by Merino's geolocation provider. It's
   *   expected to have at least the properties listed below, but we gracefully
   *   handle exceptions:
   *
   *   ```
   *   { region_code, country_code }
   *   ```
   * @param {Array} items
   *   Array of items as described in the doc for `best()`.
   * @param {Function} locationFromItem
   *   Mapping function as described in the doc for `best()`.
   * @returns {object|null}
   *   The item as described above or null.
   */
  #bestByRegion(geo, items, locationFromItem) {
    let geoCountry = geo.country_code?.toLowerCase();
    if (!geoCountry) {
      return null;
    }

    let geoRegion = geo.region_code?.toLowerCase();

    let bestCountryTuple;
    let bestRegionTuple;
    for (let item of items) {
      let location = locationFromItem(item);
      if (location.country?.toLowerCase() == geoCountry) {
        if (
          !bestCountryTuple ||
          hasLargerPopulation(location, bestCountryTuple.location)
        ) {
          bestCountryTuple = { item, location };
        }
        if (
          location.region?.toLowerCase() == geoRegion &&
          (!bestRegionTuple ||
            hasLargerPopulation(location, bestRegionTuple.location))
        ) {
          bestRegionTuple = { item, location };
        }
      }
    }

    return bestRegionTuple?.item || bestCountryTuple?.item || null;
  }

  // `MerinoClient`
  #merino;
}

function toRadians(deg) {
  return (deg * Math.PI) / 180;
}

function hasLargerPopulation(a, b) {
  return (
    typeof a.population == "number" &&
    (typeof b.population != "number" || b.population < a.population)
  );
}

export const GeolocationUtils = new _GeolocationUtils();
