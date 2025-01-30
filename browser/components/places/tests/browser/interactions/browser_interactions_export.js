/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Checks exporting interaction data. This is a Nightly-only feature, so we
 * tests largely just check that some of the values are expected with the
 * assumption that if specific values are valid, others must be too.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
});

const COLUMNS = [
  "id",
  "place_id",
  "updated_at",
  "frecency",
  "total_view_time",
  "typing_time",
  "key_presses",
  "scrolling_time",
  "scrolling_distance",
  "referrer_place_id",
  "origin_id",
  "visit_count",
  "visit_dates",
  "visit_types",
];

const COLUMNS_WITH_OPTIONS_ON = [
  "id",
  "title",
  "url",
  "frecency",
  "updated_at",
  "total_view_time",
  "typing_time",
  "key_presses",
  "scrolling_time",
  "scrolling_distance",
  "referrer_url",
  "host",
  "visit_count",
  "visit_dates",
  "visit_types",
];

const TOTAL_VIEW_TIME_1 = 2345;
const TOTAL_VIEW_TIME_2 = 6789;

add_setup(async function () {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.download.start_downloads_in_tmp_dir", true],
      ["browser.helperApps.deleteTempFileOnExit", true],
    ],
  });

  await PlacesTestUtils.addVisits({
    uri: "http://mochi.test/",
    title: "Mochi",
  });

  let now = Date.now();
  // Include different values so assertions check:
  // 1. More than one result is provided
  // 2. Each row has the correct info
  await insertIntoMozPlacesMetadata(1, {
    created_at: now - 1,
    total_view_time: TOTAL_VIEW_TIME_1,
  });
  await insertIntoMozPlacesMetadata(1, {
    created_at: now,
    total_view_time: TOTAL_VIEW_TIME_2,
  });
  await assertDatabaseValues([
    {
      url: "http://mochi.test/",
      exactTotalViewTime: TOTAL_VIEW_TIME_1,
    },
    {
      url: "http://mochi.test/",
      exactTotalViewTime: TOTAL_VIEW_TIME_2,
    },
  ]);
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();

    Services.prefs.clearUserPref("browser.download.dir");
    Services.prefs.clearUserPref("browser.download.folderList");

    // Ensure all downloads are cleared.
    let downloadList = await Downloads.getList(Downloads.ALL);
    let downloads = await downloadList.getAll();
    for (let download of downloads) {
      await downloadList.remove(download);
      await download.finalize(true);
    }
  });
});

async function downloadFile(type, includePlaceData) {
  let elementId;
  switch (type) {
    case "json":
      elementId = "export-json";
      break;
    case "csv":
      elementId = "export-csv";
      break;
  }

  let downloadList = await Downloads.getList(Downloads.ALL);
  let downloadView;
  // When a download has been attempted, resolve the promise.
  let finishedAllDownloads = new Promise(resolve => {
    downloadView = {
      onDownloadAdded(aDownload) {
        resolve(aDownload);
      },
    };
  });
  await downloadList.addView(downloadView);

  let download;
  await BrowserTestUtils.withNewTab(
    "chrome://browser/content/places/interactionsViewer.html",
    async browser => {
      await SpecialPowers.spawn(
        browser,
        [elementId, includePlaceData],
        (id, clickIncludePlaceData) => {
          info("Opened interactions viewer.");
          if (clickIncludePlaceData) {
            info("Include places data.");
            content.document.getElementById("include-place-data").click();
          }
          info("Click download.");
          content.document.getElementById(id).click();
        }
      );

      download = await finishedAllDownloads;
      await BrowserTestUtils.waitForCondition(
        () => download.succeeded,
        "Download succeeded."
      );
      info("Finished downloading.");
      await downloadList.removeView(downloadView);
    }
  );

  let cleanUp = async () => {
    // Clean up the download from the list.
    await downloadList.remove(download);
    await download.finalize(true);
  };

  return { download, cleanUp };
}

// Checking the accuracy of the JSON is non-exhaustive. It only checks the
// presence of keys,
function assertJsonFile(values, includeExtra) {
  values = JSON.parse(values);
  Assert.equal(values.length, 2, "Number of rows.");

  for (let row of values) {
    let keys = Object.keys(row);
    let matchingKeys = keys.every(key =>
      includeExtra
        ? COLUMNS_WITH_OPTIONS_ON.includes(key)
        : COLUMNS.includes(key)
    );

    Assert.ok(matchingKeys, "Each key has a matching known value.");
    Assert.equal(
      keys.length,
      includeExtra ? COLUMNS_WITH_OPTIONS_ON.length : COLUMNS.length,
      "Number of keys."
    );

    if (includeExtra) {
      Assert.equal(row.title, "Mochi", "Page title.");
      Assert.equal(row.url, "http://mochi.test/", "Url.");
      Assert.equal(row.host, "mochi.test", "Host.");
    } else {
      Assert.equal(row.place_id, 1, "Place id.");
    }
  }

  Assert.equal(
    values[0].total_view_time,
    TOTAL_VIEW_TIME_1,
    "Total view time."
  );
  Assert.equal(
    values[1].total_view_time,
    TOTAL_VIEW_TIME_2,
    "Total view time."
  );
}

// Checking the accuracy of the CSV is non-exhaustive. This only checks the
// title column is the same and some unique values are present.
function assertCsvFile(values, includeExtra) {
  values = values.split("\n");
  info("Check the title row of the CSV.");

  let columns = includeExtra ? COLUMNS_WITH_OPTIONS_ON : COLUMNS;
  for (let column of columns) {
    Assert.ok(values[0].includes(column), `First row includes ${column}`);
  }

  Assert.ok(
    values[1].includes(TOTAL_VIEW_TIME_1),
    "Exact view time included in row 1."
  );
  if (includeExtra) {
    Assert.ok(values[1].includes("Mochi"), "Page title.");
    Assert.ok(values[1].includes("http://mochi.test/"), "Url.");
  }

  Assert.ok(
    values[2].includes(TOTAL_VIEW_TIME_2),
    "Exact view time included in row 2."
  );
  if (includeExtra) {
    Assert.ok(values[2].includes("Mochi"), "Page title.");
    Assert.ok(values[2].includes("http://mochi.test/"), "Url.");
  }
}

add_task(async function export_json() {
  let { download, cleanUp } = await downloadFile("json");
  info("Open JSON.");
  let values = await IOUtils.readUTF8(download.target.path);
  assertJsonFile(values, false);

  await cleanUp();
});

add_task(async function export_json_with_extra_data() {
  let { download, cleanUp } = await downloadFile("json", true);
  info("Open JSON.");
  let values = await IOUtils.readUTF8(download.target.path);
  assertJsonFile(values, true);

  await cleanUp();
});

add_task(async function export_csv() {
  let { download, cleanUp } = await downloadFile("csv");
  info("Open CSV.");
  let values = await IOUtils.readUTF8(download.target.path);
  assertCsvFile(values, false);

  await cleanUp();
});

add_task(async function export_csv_with_extra_data() {
  let { download, cleanUp } = await downloadFile("csv", true);
  info("Open CSV.");
  let values = await IOUtils.readUTF8(download.target.path);
  assertCsvFile(values, true);

  await cleanUp();
});
