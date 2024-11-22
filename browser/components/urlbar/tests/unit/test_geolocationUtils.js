/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests GeolocationUtils.sys.mjs.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  GeolocationUtils:
    "resource:///modules/urlbar/private/GeolocationUtils.sys.mjs",
});

add_setup(async () => {
  // This test assume the mock geolocation is Kanagawa, Yokohama, Japan.
  await MerinoTestUtils.initGeolocation();
});

const KAWASAKI = {
  latitude: 35.516667,
  longitude: 139.7,
  country: "JP",
  region: "14",
  population: 1531646,
};

// Made up city with the same location Kawasaki except it has a larger
// population.
const KAWASAKI_LARGER_POP = {
  latitude: 35.516667,
  longitude: 139.7,
  country: "JP",
  region: "14",
  population: Infinity,
};

const YOKOSUKA = {
  latitude: 35.2815,
  longitude: 139.672083,
  country: "JP",
  region: "14",
  population: 388078,
};

const OSAKA = {
  latitude: 34.693889,
  longitude: 135.502222,
  country: "JP",
  region: "27",
  population: 2753862,
};

const MITOYO = {
  latitude: 34.1825,
  longitude: 133.715,
  country: "JP",
  region: "37",
  population: 59876,
};

const NYC = {
  latitude: 40.71427,
  longitude: -74.00597,
  country: "US",
  region: "NY",
  population: 8804190,
};

// Tests items whose locations have latitude and longitude.
add_task(async function bestByDistance() {
  // The made-up city with the same location as Kawasaki but with a larger
  // population should be returned due to its larger population.
  Assert.deepEqual(
    await GeolocationUtils.best([
      MITOYO,
      YOKOSUKA,
      KAWASAKI,
      KAWASAKI_LARGER_POP,
      NYC,
    ]),
    KAWASAKI_LARGER_POP,
    "Made-up Kawasaki-like city should be best when listed after Kawasaki"
  );
  Assert.deepEqual(
    await GeolocationUtils.best([
      MITOYO,
      YOKOSUKA,
      KAWASAKI_LARGER_POP,
      KAWASAKI,
      NYC,
    ]),
    KAWASAKI_LARGER_POP,
    "Made-up Kawasaki-like city should be best when listed before Kawasaki"
  );

  // Kawasaki is closest to Yokohama.
  Assert.deepEqual(
    await GeolocationUtils.best([MITOYO, YOKOSUKA, KAWASAKI, NYC]),
    KAWASAKI,
    "Kawasaki should be best"
  );

  // Yokosuka is next closest when the Kawasaki location is messed up.
  Assert.deepEqual(
    await GeolocationUtils.best([
      MITOYO,
      YOKOSUKA,
      { ...KAWASAKI, latitude: null },
      NYC,
    ]),
    YOKOSUKA,
    "Yokosuka should be best when Kawasaki location is missing latitude"
  );
  Assert.deepEqual(
    await GeolocationUtils.best([
      MITOYO,
      YOKOSUKA,
      { ...KAWASAKI, longitude: null },
      NYC,
    ]),
    YOKOSUKA,
    "Yokosuka should be best when Kawasaki location is missing longitude"
  );
});

// Tests items whose locations have region and country but no latitude or
// longitude.
add_task(async function bestByRegion() {
  let kawasaki = { ...KAWASAKI, latitude: null, longitude: null };
  let yokosuka = { ...YOKOSUKA, latitude: null, longitude: null };
  let osaka = { ...OSAKA, latitude: null, longitude: null };
  let mitoyo = { ...MITOYO, latitude: null, longitude: null };
  let nyc = { ...NYC, latitude: null, longitude: null };

  // Kawasaki and Yokosuka are in the same region as Yokohama, and Kawasaki has
  // a larger population, so it should be returned.
  Assert.deepEqual(
    await GeolocationUtils.best([mitoyo, yokosuka, kawasaki, nyc]),
    kawasaki,
    "Kawasaki should be best when listed after Yokosuka"
  );
  Assert.deepEqual(
    await GeolocationUtils.best([mitoyo, kawasaki, yokosuka, nyc]),
    kawasaki,
    "Kawasaki should be best when listed before Yokosuka"
  );

  // Yokosuka is in the same region as Yokohama, so it should be returned.
  Assert.deepEqual(
    await GeolocationUtils.best([mitoyo, yokosuka, nyc]),
    yokosuka,
    "Yokosuka should be best"
  );

  // Osaka is in a different region but the same country as the geolocation, and
  // it has a larger population than Mitoyo, so it should be returned.
  Assert.deepEqual(
    await GeolocationUtils.best([mitoyo, osaka, nyc]),
    osaka,
    "Osaka should be best when listed after Mitoyo"
  );
  Assert.deepEqual(
    await GeolocationUtils.best([osaka, mitoyo, nyc]),
    osaka,
    "Osaka should be best when listed before Mitoyo"
  );

  // Mitoyo is in a different region but the same country as the geolocation, so
  // it should be returned.
  Assert.deepEqual(
    await GeolocationUtils.best([mitoyo, nyc]),
    mitoyo,
    "Mitoyo should be best when listed first"
  );
  Assert.deepEqual(
    await GeolocationUtils.best([nyc, mitoyo]),
    mitoyo,
    "Mitoyo should be best when listed last"
  );
});

// Tests non-default values for the `locationFromItem` param.
add_task(async function locationFromItem() {
  // Return an empty location for everything except NYC. Since NYC is the only
  // location with latitude and longitude, `best()` should return it even though
  // it's not actually the closest to Merino's mock geolocation.
  Assert.deepEqual(
    await GeolocationUtils.best([MITOYO, KAWASAKI, YOKOSUKA, NYC], item =>
      item == NYC ? NYC : {}
    ),
    NYC,
    "NYC should be best"
  );
});

// Tests unexpected Merino geolocation responses.
add_task(async function unexpectedMerinoGeolocation() {
  // The first item should be returned in all these cases.
  MerinoTestUtils.server.response = { status: 501 };
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO, YOKOSUKA, KAWASAKI]),
    NYC,
    "NYC should be best when geolocation response is not a 200"
  );

  await MerinoTestUtils.initGeolocation();
  MerinoTestUtils.server.response = { status: 200 };
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO, YOKOSUKA, KAWASAKI]),
    NYC,
    "NYC should be best when geolocation response is empty"
  );

  await MerinoTestUtils.initGeolocation();
  MerinoTestUtils.server.response.body.suggestions[0].custom_details = null;
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO, KAWASAKI, YOKOSUKA]),
    NYC,
    "NYC should be best when geolocation response is missing custom_details"
  );

  await MerinoTestUtils.initGeolocation();
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation =
    null;
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO, KAWASAKI, YOKOSUKA]),
    NYC,
    "NYC should be best when geolocation response is missing geolocation"
  );

  await MerinoTestUtils.initGeolocation();
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation.region_code =
    null;
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation.country_code =
    null;
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation.location =
    null;
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO, KAWASAKI, YOKOSUKA]),
    NYC,
    "NYC should be best when geolocation response is missing geolocation"
  );

  // We should fall back to `#bestByRegion()` in these cases.
  await MerinoTestUtils.initGeolocation();
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation.location =
    null;
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO, KAWASAKI, YOKOSUKA]),
    KAWASAKI,
    "Kawasaki should be best when geolocation response is missing location"
  );

  await MerinoTestUtils.initGeolocation();
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation.location.latitude =
    null;
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO, KAWASAKI, YOKOSUKA]),
    KAWASAKI,
    "Kawasaki should be best when geolocation response is missing latitude"
  );

  await MerinoTestUtils.initGeolocation();
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation.location.longitude =
    null;
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO, KAWASAKI, YOKOSUKA]),
    KAWASAKI,
    "Kawasaki should be best when geolocation response is missing longitude"
  );

  // Here `region_code` and `location` are null but `country_code` is still
  // correct, so the item with the same country should be returned.
  await MerinoTestUtils.initGeolocation();
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation.region_code =
    null;
  MerinoTestUtils.server.response.body.suggestions[0].custom_details.geolocation.location =
    null;
  Assert.deepEqual(
    await GeolocationUtils.best([NYC, MITOYO]),
    MITOYO,
    "Mitoyo should be best when geolocation response is missing region_code and location and Mitoyo is listed after NYC"
  );
  Assert.deepEqual(
    await GeolocationUtils.best([MITOYO, NYC]),
    MITOYO,
    "Mitoyo should be best when geolocation response is missing region_code and location and Mitoyo is listed before NYC"
  );

  // Reset for the next task.
  await MerinoTestUtils.initGeolocation();
});
