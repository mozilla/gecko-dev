/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const VIEWTIME_THRESHOLD_SECONDS = Services.prefs.getIntPref(
  "places.frecency.pages.alternative.interactions.viewTimeSeconds"
);

const VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS = Services.prefs.getIntPref(
  "places.frecency.pages.alternative.interactions.viewTimeIfManyKeypressesSeconds"
);

const MANY_KEYPRESSES_THRESHOLD = Services.prefs.getIntPref(
  "places.frecency.pages.alternative.interactions.manyKeypresses"
);

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

async function updateMozPlacesMetadata(
  id,
  { total_view_time = 0, key_presses = 0 }
) {
  info(`Updating interaction ${id} in moz_places_metadata.`);
  await PlacesUtils.withConnectionWrapper(
    "test_frecency_interactions::updateMozPlacesMetadata",
    async db => {
      await db.execute(
        `
        UPDATE
          moz_places_metadata
        SET
          total_view_time = :total_view_time,
          key_presses = :key_presses
        WHERE
          id = :id`,
        {
          total_view_time,
          key_presses,
          id,
        }
      );
    }
  );
}

add_setup(async function () {
  await PlacesUtils.history.clear();
});

add_task(async function test_insertion_triggers() {
  const TEST_CASES = [
    {
      title: "Empty view time.",
      interaction: {
        total_view_time: 0,
      },
      expectRecalc: false,
    },
    {
      title: "Well below either viewtime threshold.",
      interaction: {
        total_view_time: 120,
      },
      expectRecalc: false,
    },
    {
      title: "At keypress viewtime threshold.",
      interaction: {
        total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
      },
      expectRecalc: false,
    },
    {
      title: "At keypresses threshold but below keypress viewtime threshold.",
      interaction: {
        total_view_time: 10,
        key_presses: MANY_KEYPRESSES_THRESHOLD,
      },
      expectRecalc: false,
    },
    {
      title: "At keypresses AND keypress viewtime threshold.",
      interaction: {
        total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
        key_presses: MANY_KEYPRESSES_THRESHOLD + 1,
      },
      expectRecalc: true,
    },
    {
      title: "At viewtime threshold.",
      interaction: {
        total_view_time: VIEWTIME_THRESHOLD_SECONDS * 1000,
      },
      expectRecalc: true,
    },
  ];

  for (let test of TEST_CASES) {
    info(`Testing: ${test.title}`);
    let url = "https://www.example.com/page";
    await PlacesTestUtils.addVisits([url]);

    let page = await getPageWithUrl(url);
    await insertIntoMozPlacesMetadata(page.id, test.interaction);
    page = await getPageWithUrl(url);

    if (test.expectRecalc) {
      Assert.equal(page.recalc_alt_frecency, 1, "Should recalc alt frecency.");
    } else {
      Assert.equal(
        page.recalc_alt_frecency,
        0,
        "Should not recalc alt frecency."
      );
    }

    await PlacesUtils.history.clear();
  }
});

add_task(async function test_update_triggers_to_recalc() {
  const TEST_CASES = [
    {
      title: "Update view time too low.",
      insert: {
        interaction: {
          total_view_time: 0,
        },
      },
      update: {
        interaction: {
          total_view_time: 120,
        },
      },
      expectRecalc: false,
    },
    {
      title: "Update view time at view time threshold.",
      insert: {
        interaction: {
          total_view_time: 0,
        },
      },
      update: {
        interaction: {
          total_view_time: VIEWTIME_THRESHOLD_SECONDS * 1000,
        },
      },
      expectRecalc: true,
    },
    {
      title: "Update view time at keypress threshold but not keypresses.",
      insert: {
        interaction: {
          total_view_time: 0,
          key_presses: 0,
        },
      },
      update: {
        interaction: {
          total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: 10,
        },
      },
      expectRecalc: false,
    },
    {
      title: "Keypress update is at threshold but not keypress view time.",
      insert: {
        interaction: {
          total_view_time: 0,
          key_presses: 0,
        },
      },
      update: {
        interaction: {
          total_view_time: 10,
          key_presses: MANY_KEYPRESSES_THRESHOLD,
        },
      },
      expectRecalc: false,
    },
    {
      title:
        "Keypresses and viewtime keypresses reach thresholds at the same time.",
      insert: {
        interaction: {
          total_view_time: 0,
          key_presses: 0,
        },
      },
      update: {
        interaction: {
          total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: MANY_KEYPRESSES_THRESHOLD,
        },
      },
      expectRecalc: true,
    },
    {
      title: "Keypresses reach threshold after viewtime keypress threshold.",
      insert: {
        interaction: {
          total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: 0,
        },
      },
      update: {
        interaction: {
          total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: MANY_KEYPRESSES_THRESHOLD,
        },
      },
      expectRecalc: true,
    },
    {
      title: "Viewtime keypresses reach threshold after keypresses.",
      insert: {
        interaction: {
          total_view_time: 0,
          key_presses: MANY_KEYPRESSES_THRESHOLD,
        },
      },
      update: {
        interaction: {
          total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: MANY_KEYPRESSES_THRESHOLD,
        },
      },
      expectRecalc: true,
    },
  ];

  for (let test of TEST_CASES) {
    info(`Testing: ${test.title}`);
    let url = "https://www.example.com/page";
    await PlacesTestUtils.addVisits([url]);

    let page = await getPageWithUrl(url);
    await insertIntoMozPlacesMetadata(page.id, test.insert.interaction);
    page = await getPageWithUrl(url);
    Assert.equal(
      page.recalc_alt_frecency,
      0,
      "Should not recalc alt frecency."
    );

    await updateMozPlacesMetadata(1, test.update.interaction);
    page = await getPageWithUrl(url);
    if (test.expectRecalc) {
      Assert.equal(page.recalc_alt_frecency, 1, "Should recalc alt frecency.");
    } else {
      Assert.equal(
        page.recalc_alt_frecency,
        0,
        "Should not recalc alt frecency."
      );
    }

    await PlacesUtils.history.clear();
  }
});

// Once we've recalculated alternative frecency, don't recalculate it again
// upon further updates to the same interaction.
add_task(async function test_no_additional_recalc() {
  const TEST_CASES = [
    {
      title: "Update view time increased.",
      insert: {
        interaction: {
          total_view_time: VIEWTIME_THRESHOLD_SECONDS * 1000,
        },
      },
      update: {
        interaction: {
          total_view_time: 6 * VIEWTIME_THRESHOLD_SECONDS * 1000,
        },
      },
      expectRecalc: false,
    },
    {
      title: "Keypress viewtime threshold increased.",
      insert: {
        interaction: {
          total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: MANY_KEYPRESSES_THRESHOLD,
        },
      },
      update: {
        interaction: {
          total_view_time:
            2 * VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: MANY_KEYPRESSES_THRESHOLD,
        },
      },
      expectRecalc: false,
    },
    {
      title: "Keypress threshold increased.",
      insert: {
        interaction: {
          total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: MANY_KEYPRESSES_THRESHOLD,
        },
      },
      update: {
        interaction: {
          total_view_time: VIEWTIME_IF_MANY_KEYPRESSES_THRESHOLD_SECONDS * 1000,
          key_presses: 10 * MANY_KEYPRESSES_THRESHOLD,
        },
      },
      expectRecalc: false,
    },
  ];

  for (let test of TEST_CASES) {
    info(`Testing: ${test.title}`);
    let url = "https://www.example.com/page";
    await PlacesTestUtils.addVisits([url]);

    let page = await getPageWithUrl(url);
    await insertIntoMozPlacesMetadata(page.id, test.insert.interaction);
    page = await getPageWithUrl(url);
    Assert.equal(page.recalc_alt_frecency, 1, "Should recalc alt frecency.");

    await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
    page = await getPageWithUrl(url);
    Assert.equal(
      page.recalc_alt_frecency,
      0,
      "Should not recalc alt frecency."
    );

    await updateMozPlacesMetadata(1, test.update.interaction);
    page = await getPageWithUrl(url);
    Assert.equal(
      page.recalc_alt_frecency,
      0,
      "Should not recalc alt frecency."
    );

    await PlacesUtils.history.clear();
  }
});
