/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that recalc_frecency in the moz_origins table works is consistent.

add_task(async function test() {
  // test recalc_frecency is set to 1 when frecency of a page changes.
  // Add a couple visits, then remove one of them.
  const now = new Date();
  const host = "mozilla.org";
  const url = `https://${host}/test/`;
  await PlacesTestUtils.addVisits([
    {
      url,
      visitDate: now,
    },
    {
      url,
      visitDate: new Date(new Date().setDate(now.getDate() - 30)),
    },
  ]);
  // Temporarily unset recalculation, otherwise the task may flip the recalc
  // fields before we check them.
  PlacesUtils.history.shouldStartFrecencyRecalculation = false;
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_origins", "recalc_frecency", {
      host,
    }),
    1,
    "Frecency should be calculated"
  );
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue(
      "moz_origins",
      "recalc_alt_frecency",
      {
        host,
      }
    ),
    1,
    "Alt frecency should be calculated"
  );

  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let alt_frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_origins",
    "alt_frecency",
    { host }
  );
  let frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_origins",
    "frecency",
    { host }
  );

  info("Remove only one visit (otherwise the page would be orphaned).");
  await PlacesUtils.history.removeVisitsByFilter({
    beginDate: new Date(now.valueOf() - 10000),
    endDate: new Date(now.valueOf() + 10000),
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_origins", "recalc_frecency", {
      host,
    }),
    0,
    "Should have been calculated"
  );
  Assert.greater(
    frecency,
    await PlacesTestUtils.getDatabaseValue("moz_origins", "frecency", {
      host,
    }),
    "frecency should have decreased"
  );
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue(
      "moz_origins",
      "recalc_alt_frecency",
      { host }
    ),
    0,
    "Should have been calculated"
  );
  Assert.greater(
    alt_frecency,
    await PlacesTestUtils.getDatabaseValue("moz_origins", "alt_frecency", {
      host,
    }),
    "alternative frecency should have decreased"
  );

  info("Add another page to the same host.");
  const url2 = `https://${host}/second/`;
  await PlacesTestUtils.addVisits(url2);
  info("Remove the first page.");
  await PlacesUtils.history.remove(url);
  // Temporarily unset recalculation, otherwise the task may flip the recalc
  // fields before we check them.
  PlacesUtils.history.shouldStartFrecencyRecalculation = false;
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_origins", "recalc_frecency", {
      host,
    }),
    1,
    "Frecency should be calculated"
  );
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue(
      "moz_origins",
      "recalc_alt_frecency",
      { host }
    ),
    1,
    "Alt frecency should be calculated"
  );
});

add_task(async function test_frecency_decay() {
  const url = new URL("https://example.com/");
  async function checkRecalcFields(value) {
    // Since we use concurrent connections, we must await for the table update.
    await TestUtils.waitForCondition(
      async () =>
        (await PlacesTestUtils.getDatabaseValue(
          "moz_origins",
          "recalc_frecency",
          {
            host: url.host,
          }
        )) == value,
      `Frecency should ${value ? "" : "not "}have been calculated`
    );
    Assert.equal(
      await PlacesTestUtils.getDatabaseValue(
        "moz_origins",
        "recalc_alt_frecency",
        { host: url.host }
      ),
      value,
      `Alternative frecency should ${value ? "" : "not "}have been calculated`
    );
  }

  const now = new Date();
  // Add a very old visit, then a recent one.
  await PlacesTestUtils.addVisits([
    {
      url,
      visitDate: new Date(new Date().setDate(now.getDate() - 100)),
    },
    { url, visitDate: now },
  ]);
  info("Recalculate frecencies.");
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_origins",
    "frecency",
    { host: "example.com" }
  );
  Assert.greater(frecency, 1, "Frecency should be set");
  await checkRecalcFields(0);
  Assert.ok(
    !(await PlacesUtils.metadata.get(
      "origins_frecency_last_decay_timestamp",
      0
    )),
    "Check meta key has not been set."
  );

  info("Simulate idle-daily topic to the component");
  await PlacesUtils.metadata.set("origins_frecency_last_decay_timestamp", 0);
  PlacesFrecencyRecalculator.observe(null, "idle-daily");
  await PlacesFrecencyRecalculator.pendingOriginsDecayPromise;
  // Nothing should have changed.
  await checkRecalcFields(0);

  info("Remove the most recent visit");
  await PlacesUtils.history.removeVisitsByFilter({
    beginDate: new Date(new Date().setDate(now.getDate() - 10)),
    endDate: now,
  });

  info("Simulate idle-daily topic to the component");
  await PlacesUtils.metadata.set("origins_frecency_last_decay_timestamp", 0);
  PlacesFrecencyRecalculator.observe(null, "idle-daily");
  await PlacesFrecencyRecalculator.pendingOriginsDecayPromise;
  await checkRecalcFields(1);
  Assert.ok(
    await PlacesUtils.metadata.get("origins_frecency_last_decay_timestamp", 0),
    "Check meta key has been set."
  );
});
