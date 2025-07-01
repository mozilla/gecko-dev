/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { NimbusTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { TrendingSearchFeed } = ChromeUtils.importESModule(
  "resource://newtab/lib/TrendingSearchFeed.sys.mjs"
);

const searchControllerResponse = {
  remote: [
    {
      value: "response1",
      matchPrefix: null,
      tail: null,
      trending: true,
      icon: null,
      description: null,
    },
    {
      value: "response2",
      matchPrefix: null,
      tail: null,
      trending: true,
      icon: null,
      description: null,
    },
  ],
};

add_task(async function test_nimbus_experiment_enabled() {
  sinon
    .stub(TrendingSearchFeed.prototype, "SearchSuggestionController")
    .returns({
      fetch: () => searchControllerResponse,
    });
  let trendingsearchfeed = AboutNewTab.activityStream.store.feeds.get(
    "feeds.trendingsearchfeed"
  );

  // Initialize the feed, because that doesn't happen by default.
  await trendingsearchfeed.onAction({ type: "INIT" });

  ok(!trendingsearchfeed?.initialized, "Should initially not be loaded.");

  // Setup the experiment.
  await ExperimentAPI.ready();
  let doExperimentCleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "newtabTrendingSearchWidget",
    value: { enabled: true },
  });

  ok(trendingsearchfeed?.initialized, "Should now be loaded.");

  await doExperimentCleanup();
});
