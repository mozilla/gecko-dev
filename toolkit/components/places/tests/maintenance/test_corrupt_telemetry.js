/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that history initialization correctly handles a request to forcibly
// replace the current database.

add_setup(async function () {
  do_get_profile();
  Services.fog.initializeFOG();
});

add_task(async function () {
  await createCorruptDb("places.sqlite");

  let count = Services.telemetry
    .getHistogramById("PLACES_DATABASE_CORRUPTION_HANDLING_STAGE")
    .snapshot().values[3];
  Assert.equal(count, undefined, "There should be no telemetry");

  let gleanValue =
    Glean.places.placesDatabaseCorruptionHandlingStage[
      "places.sqlite"
    ].testGetValue();
  Assert.ok(!gleanValue, "There should be no glean telemetry.");

  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CORRUPT
  );

  count = Services.telemetry
    .getHistogramById("PLACES_DATABASE_CORRUPTION_HANDLING_STAGE")
    .snapshot().values[3];
  Assert.equal(count, 1, "Telemetry should have been added");

  gleanValue =
    Glean.places.placesDatabaseCorruptionHandlingStage[
      "places.sqlite"
    ].testGetValue();
  Assert.ok(gleanValue, "Glean metric should have been recorded.");
  Assert.equal(
    gleanValue,
    "stage_replaced",
    "The correct corruption stage should be recorded."
  );
});
