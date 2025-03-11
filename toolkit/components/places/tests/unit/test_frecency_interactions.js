/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for integrating interaction table data with alternative frecency.
 * If the interaction data is considered "interesting", alternative frecency
 * should experience a small boost.
 *
 * Since we don't know the precise values of the score, the tests typically
 * have a pattern of caching a baseline without interactions and then checking
 * what happens when either interesting or un-interesting interactions are
 * inserted.
 */

"use strict";

// The Viewtime Threshold preference is stored in seconds. When recorded in
// places, the data is stored in milliseconds.
const VIEWTIME_THRESHOLD =
  Services.prefs.getIntPref(
    "places.frecency.pages.alternative.interactions.viewTimeSeconds"
  ) * 1000;
const MANY_KEYPRESSES_THRESHOLD = Services.prefs.getIntPref(
  "places.frecency.pages.alternative.interactions.manyKeypresses"
);

// The Viewtime Threshold for Keypresses preference is stored in seconds.
// When recorded in places, the data is stored in milliseconds.
const VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD =
  Services.prefs.getIntPref(
    "places.frecency.pages.alternative.interactions.viewTimeIfManyKeypressesSeconds"
  ) * 1000;

const SAMPLED_VISITS_THRESHOLD = Services.prefs.getIntPref(
  "places.frecency.pages.alternative.numSampledVisits"
);
const MAX_VISIT_GAP = Services.prefs.getIntPref(
  "places.frecency.pages.alternative.interactions.maxVisitGapSeconds"
);

async function insertIntoMozPlacesMetadata(
  place_id,
  {
    referrer_place_id = null,
    created_at = Date.now(),
    updated_at = Date.now(),
    total_view_time = 0,
    typing_time = 0,
    key_presses = 0,
    scrolling_time = 0,
    scrolling_distance = 0,
    document_type = 0,
    search_query_id = null,
  }
) {
  info("Inserting interaction into moz_places_metadata.");
  await PlacesUtils.withConnectionWrapper(
    "test_frecency_interactions::insertIntoMozPlacesMetadata",
    async db => {
      await db.execute(
        `
        INSERT INTO moz_places_metadata (
          place_id,
          referrer_place_id,
          created_at,
          updated_at,
          total_view_time,
          typing_time,
          key_presses,
          scrolling_time,
          scrolling_distance,
          document_type,
          search_query_id
        ) VALUES (
         :place_id,
         :referrer_place_id,
         :created_at,
         :updated_at,
         :total_view_time,
         :typing_time,
         :key_presses,
         :scrolling_time,
         :scrolling_distance,
         :document_type,
         :search_query_id
        )
      `,
        {
          place_id,
          referrer_place_id,
          created_at,
          updated_at,
          total_view_time,
          typing_time,
          key_presses,
          scrolling_time,
          scrolling_distance,
          document_type,
          search_query_id,
        }
      );
    }
  );
}

async function insertIntoMozPlaces({
  url,
  guid,
  url_hash,
  origin_id,
  frecency,
  alt_frecency,
}) {
  await PlacesUtils.withConnectionWrapper(
    "test_frecency_interactions::insertIntoMozPlaces",
    async db => {
      await db.execute(
        `
        INSERT INTO moz_places (
          url,
          guid,
          url_hash,
          origin_id,
          frecency,
          alt_frecency
        ) VALUES (
          :url,
          :guid,
          :url_hash,
          :origin_id,
          :frecency,
          :alt_frecency
        )
      `,
        {
          url,
          guid,
          url_hash,
          origin_id,
          frecency,
          alt_frecency,
        }
      );
    }
  );
}

async function getPageWithUrl(url) {
  info(`Find ${url} in moz_places.`);
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute(`SELECT * FROM moz_places WHERE url = :url`, {
    url,
  });
  Assert.equal(rows.length, 1, "Found one matching row in moz_places.");
  return rows.map(r => ({
    id: r.getResultByName("id"),
    url: r.getResultByName("url"),
    title: r.getResultByName("title"),
    frecency: r.getResultByName("frecency"),
    recalc_frecency: r.getResultByName("recalc_frecency"),
    alt_frecency: r.getResultByName("alt_frecency"),
    recalc_alt_frecency: r.getResultByName("recalc_alt_frecency"),
  }))[0];
}

add_setup(async function () {
  registerCleanupFunction(PlacesUtils.history.clear);
});

/**
 * Each of the interactions occur at the same time as the visit so they
 * are paired as one sample.
 */
add_task(async function one_visit_one_matching_interaction() {
  const TESTS = [
    {
      title: "View time is under threshold",
      interactionData: {
        total_view_time: VIEWTIME_THRESHOLD - 1,
      },
      expectIncrease: false,
    },
    {
      title: "View time exceeds threshold",
      interactionData: {
        total_view_time: VIEWTIME_THRESHOLD,
      },
      expectIncrease: true,
    },
    {
      title: "View time and key presses under threshold",
      interactionData: {
        total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD - 1,
        key_presses: MANY_KEYPRESSES_THRESHOLD,
      },
      expectIncrease: false,
    },
    {
      title: "View time and key presses under threshold",
      interactionData: {
        total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD,
        key_presses: MANY_KEYPRESSES_THRESHOLD - 1,
      },
      expectIncrease: false,
    },
    {
      title: "View time and key presses exceed threshold",
      interactionData: {
        total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD,
        key_presses: MANY_KEYPRESSES_THRESHOLD,
      },
      expectIncrease: true,
    },
  ];
  for (let test of TESTS) {
    info(`Running test: ${test.title}`);
    let url = "https://testdomain1.moz.org/";
    await PlacesTestUtils.addVisits([url]);

    let page = await getPageWithUrl(url);
    let oldAltFrecency = page.alt_frecency;
    Assert.notEqual(
      oldAltFrecency,
      null,
      "Alt frecency with a visit but no interaction should not be null."
    );

    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Assert.equal(
      page.alt_frecency,
      oldAltFrecency,
      `Alt frecency of ${url} should not have changed.`
    );

    await insertIntoMozPlacesMetadata(page.id, test.interactionData);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    page = await getPageWithUrl(url);
    if (test.expectIncrease) {
      Assert.greater(
        page.alt_frecency,
        oldAltFrecency,
        `Alt frecency of ${url} should have increased.`
      );
    } else {
      Assert.equal(
        page.alt_frecency,
        oldAltFrecency,
        `Alt frecency of ${url} should not have changed.`
      );
    }
    await PlacesUtils.history.clear();
  }
});

/**
 * Each of the interactions are one day before the visit so the interaction
 * and visit are separate samples.
 */
add_task(async function one_visit_one_non_matching_interaction() {
  const today = new Date();
  const yesterday = new Date();
  yesterday.setDate(today.getDate() - 1);
  const TESTS = [
    {
      title: "View time is under threshold",
      interactionData: {
        total_view_time: VIEWTIME_THRESHOLD - 1,
        created_at: yesterday.getTime(),
      },
      expectIncrease: false,
    },
    {
      title: "View time exceeds threshold",
      interactionData: {
        total_view_time: VIEWTIME_THRESHOLD,
        created_at: yesterday.getTime(),
      },
      expectIncrease: true,
    },
    {
      title: "View time and key presses under threshold",
      interactionData: {
        total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD - 1,
        key_presses: MANY_KEYPRESSES_THRESHOLD,
        created_at: yesterday.getTime(),
      },
      expectIncrease: false,
    },
    {
      title: "View time and key presses under threshold",
      interactionData: {
        total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD,
        key_presses: MANY_KEYPRESSES_THRESHOLD - 1,
        created_at: yesterday.getTime(),
      },
      expectIncrease: false,
    },
    {
      title: "View time and key presses exceed threshold",
      interactionData: {
        total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD,
        key_presses: MANY_KEYPRESSES_THRESHOLD,
        created_at: yesterday.getTime(),
      },
      expectIncrease: true,
    },
  ];
  for (let test of TESTS) {
    info(test.title);
    let url = "https://testdomain1.moz.org/";
    await PlacesTestUtils.addVisits([url]);

    let page = await getPageWithUrl(url);
    let oldAltFrecency = page.alt_frecency;

    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Assert.equal(
      page.alt_frecency,
      oldAltFrecency,
      `Alt frecency of ${url} should not have changed.`
    );

    await insertIntoMozPlacesMetadata(page.id, test.interactionData);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

    page = await getPageWithUrl(url);
    if (test.expectIncrease) {
      Assert.greater(
        page.alt_frecency,
        oldAltFrecency,
        `Alt frecency of ${url} should have increased.`
      );
    } else {
      Assert.equal(
        page.alt_frecency,
        oldAltFrecency,
        `Alt frecency of ${url} should not have changed.`
      );
    }
    await PlacesUtils.history.clear();
  }
});

add_task(async function zero_visits_one_interaction() {
  const TESTS = [
    {
      title: "View time is under threshold",
      interactionData: {
        total_view_time: VIEWTIME_THRESHOLD - 1,
      },
      expectIncrease: false,
    },
    {
      title: "View time is at threshold",
      interactionData: {
        total_view_time: VIEWTIME_THRESHOLD,
      },
      expectIncrease: true,
    },
  ];

  for (let test of TESTS) {
    info(test.title);

    let url = "https://testdomain1.moz.org/";
    await insertIntoMozPlaces({
      url,
      url_hash: "1234567890",
      frecency: 0,
      alt_frecency: null,
    });

    let page = await getPageWithUrl(url);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Assert.equal(page.alt_frecency, null, "Alt frecency is null");

    await insertIntoMozPlacesMetadata(page.id, test.interactionData);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

    page = await getPageWithUrl(url);
    if (test.expectIncrease) {
      Assert.notEqual(page.alt_frecency, null, "Alt frecency is non null");
      Assert.greater(page.alt_frecency, 0, "Alt frecency greater than zero");
    } else {
      Assert.equal(page.alt_frecency, null, "Alt frecency is null");
    }

    await PlacesUtils.history.clear();
  }
});

// When the sample threshold is reached, check the correct samples are
// used. This is done by inserting the same number of visits as the sample
// threshold and adding an interaction that doesn't belong to any of the visits
// before or after the visits.
add_task(async function max_samples_threshold() {
  const today = new Date();
  const yesterday = new Date();
  const twoDaysAgo = new Date();
  yesterday.setDate(today.getDate() - 1);
  twoDaysAgo.setDate(today.getDate() - 2);
  const TESTS = [
    {
      title: "Interactions are beyond sample threshold",
      interactionData: {
        total_view_time: VIEWTIME_THRESHOLD,
        created_at: twoDaysAgo.getTime(),
      },
      expectIncrease: false,
    },
    {
      title: "Interactions are within sample threshold",
      interactionData: {
        total_view_time: VIEWTIME_THRESHOLD,
        created_at: today.getTime(),
      },
      expectIncrease: true,
    },
  ];

  for (let test of TESTS) {
    info(test.title);
    let url = "https://testdomain1.moz.org/";
    for (let i = 0; i < SAMPLED_VISITS_THRESHOLD; ++i) {
      await PlacesTestUtils.addVisits([
        {
          url,
          visitDate: yesterday.getTime() * 1000,
        },
      ]);
    }

    let page = await getPageWithUrl(url);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    Assert.notEqual(page.alt_frecency, null, "Alt frecency is non-null");

    let oldAltFrecency = page.alt_frecency;
    await insertIntoMozPlacesMetadata(page.id, test.interactionData);
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

    page = await getPageWithUrl(url);
    if (test.expectIncrease) {
      Assert.notEqual(page.alt_frecency, null, "Alt frecency is non null");
      Assert.greater(
        page.alt_frecency,
        oldAltFrecency,
        "Alt frecency greater than the old value."
      );
    } else {
      Assert.equal(
        page.alt_frecency,
        oldAltFrecency,
        "Alt frecency didn't change"
      );
    }

    await PlacesUtils.history.clear();
  }
});

// Verify that the number of interactions contributing to alternative frecency
// is as expected. Avoid using actual visits, ensuring all interactions are
// treated as virtual visits. Each virtual visit should increase the alternative
// frecency score until the threshold is exceeded. Interactions are deliberately
// inserted sequentially in the past, as the date can affect the score.
add_task(async function max_interesting_interactions() {
  const today = new Date();

  let url = "https://testdomain1.moz.org/";
  await insertIntoMozPlaces({
    url,
    url_hash: "1234567890",
    frecency: 0,
    alt_frecency: null,
  });

  let page = await getPageWithUrl(url);
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  Assert.equal(page.alt_frecency, null, "Alt frecency is null");

  info("Insert interesting interactions until threshold is met.");
  for (let i = 0; i < SAMPLED_VISITS_THRESHOLD; ++i) {
    let pageBefore = await getPageWithUrl(url);
    await insertIntoMozPlacesMetadata(page.id, {
      total_view_time: VIEWTIME_THRESHOLD,
      created_at: today.getTime() - i,
    });
    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    let pageAfter = await getPageWithUrl(url);
    Assert.greater(
      pageAfter.alt_frecency,
      // The first run, alt frecency will be null.
      pageBefore.alt_frecency ?? 0,
      "Alt frecency has increased."
    );
  }

  info("Insert an interesting interaction beyond the threshold.");
  let pageBefore = await getPageWithUrl(url);
  await insertIntoMozPlacesMetadata(page.id, {
    total_view_time: VIEWTIME_THRESHOLD,
    // Choose a time that would make this below all other existing interactions.
    created_at: today.getTime() - 100,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  let pageAfter = await getPageWithUrl(url);
  Assert.equal(
    pageBefore.alt_frecency,
    pageAfter.alt_frecency,
    "Alt frecency is the same."
  );

  await PlacesUtils.history.clear();
});

add_task(async function temp_redirect_alt_frecency() {
  const yesterday = new Date();

  let url1 = "http://testdomain1.moz.org/";
  let url2 = "https://testdomain2.moz.org/";
  let url3 = "https://testdomain2.moz.org/dashboard";

  const visitDate = yesterday * 1000;
  await PlacesTestUtils.addVisits([
    {
      url: url1,
      visitDate,
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
    {
      url: url2,
      visitDate,
      transition: PlacesUtils.history.TRANSITIONS.REDIRECT_PERMANENT,
      referrer: Services.io.newURI(url1),
    },
    {
      url: url3,
      visitDate,
      transition: PlacesUtils.history.TRANSITIONS.REDIRECT_TEMPORARY,
      referrer: Services.io.newURI(url2),
    },
  ]);

  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  let page1 = await getPageWithUrl(url1);
  Assert.notEqual(page1.alt_frecency, null, "Alt frecency is non-null");

  let page2 = await getPageWithUrl(url2);
  Assert.notEqual(page2.alt_frecency, null, "Alt frecency is non-null");

  let page3 = await getPageWithUrl(url3);
  Assert.notEqual(page3.alt_frecency, null, "Alt frecency is non-null");

  info("For each visit, add an interesting interaction.");
  const created_at = yesterday.getTime();
  const updated_at = yesterday.getTime();
  await insertIntoMozPlacesMetadata(page1.id, {
    total_view_time: VIEWTIME_THRESHOLD,
    created_at,
    updated_at,
  });
  await insertIntoMozPlacesMetadata(page2.id, {
    total_view_time: VIEWTIME_THRESHOLD,
    created_at,
    updated_at,
  });
  await insertIntoMozPlacesMetadata(page3.id, {
    total_view_time: VIEWTIME_THRESHOLD,
    created_at,
    updated_at,
  });

  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  let newPage1 = await getPageWithUrl(url1);
  Assert.equal(
    newPage1.alt_frecency,
    page1.alt_frecency,
    "Alt frecency is the same"
  );

  let newPage2 = await getPageWithUrl(url2);
  Assert.equal(
    newPage2.alt_frecency,
    page2.alt_frecency,
    "Alt frecency is the same"
  );

  let newPage3 = await getPageWithUrl(url3);
  Assert.greater(
    newPage3.alt_frecency,
    page3.alt_frecency,
    "Alt frecency has increased"
  );

  await PlacesUtils.history.clear();
});

add_task(async function interaction_visit_gap() {
  let url = "https://testdomain1.moz.org/";
  let now = new Date();
  let maxVisitGapMs = MAX_VISIT_GAP * 1000;

  // Insert visits that match the number of sample threshold to avoid
  // interesting interactions becoming virtual visits.
  for (let i = 0; i < SAMPLED_VISITS_THRESHOLD; ++i) {
    await PlacesTestUtils.addVisits([
      {
        url,
        visitDate: now,
      },
    ]);
  }
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  let page = await getPageWithUrl(url);
  Assert.notEqual(page.alt_frecency, null, "Alt frecency is not null");

  info("Add an interaction just below the visit gap.");
  await insertIntoMozPlacesMetadata(page.id, {
    total_view_time: VIEWTIME_THRESHOLD,
    created_at: now.getTime() - maxVisitGapMs - 1,
    updated_at: now.getTime() - maxVisitGapMs - 1,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  let updatedPage = await getPageWithUrl(url);
  Assert.equal(
    updatedPage.alt_frecency,
    page.alt_frecency,
    "Alt frecency didn't change."
  );

  info("Add an interaction at the visit gap.");
  await insertIntoMozPlacesMetadata(page.id, {
    total_view_time: VIEWTIME_THRESHOLD,
    created_at: now.getTime() - maxVisitGapMs,
    updated_at: now.getTime() - maxVisitGapMs,
  });
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();

  updatedPage = await getPageWithUrl(url);
  Assert.greater(
    updatedPage.alt_frecency,
    page.alt_frecency,
    "Alt frecency increased."
  );

  await PlacesUtils.history.clear();
});
