/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test for MLSuggest.sys.mjs.
 */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  MLSuggest: "resource:///modules/urlbar/private/MLSuggest.sys.mjs",
});

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/ml/tests/browser/head.js",
  this
);

let nerResultsMap = {
  "restaurants in seattle, wa": [
    {
      entity: "B-CITYSTATE",
      score: 0.9999846816062927,
      index: 3,
      word: "seattle",
    },
    {
      entity: "I-CITYSTATE",
      score: 0.9999918341636658,
      index: 4,
      word: ",",
    },
    {
      entity: "I-CITYSTATE",
      score: 0.9999667406082153,
      index: 5,
      word: "wa",
    },
  ],
  "hotels in new york, ny": [
    {
      entity: "B-CITYSTATE",
      score: 0.999022364616394,
      index: 3,
      word: "new",
    },
    {
      entity: "I-CITYSTATE",
      score: 0.9999206066131592,
      index: 4,
      word: "york",
    },
    {
      entity: "I-CITYSTATE",
      score: 0.9999917149543762,
      index: 5,
      word: ",",
    },
    {
      entity: "I-CITYSTATE",
      score: 0.9999532103538513,
      index: 6,
      word: "ny",
    },
  ],
  "restaurants seattle": [
    {
      entity: "B-CITY",
      score: 0.9980050921440125,
      index: 2,
      word: "seattle",
    },
  ],
  "restaurants in seattle": [
    {
      entity: "B-CITY",
      score: 0.9980319738388062,
      index: 3,
      word: "seattle",
    },
  ],
  "restaurants near seattle": [
    {
      entity: "B-CITY",
      score: 0.998751163482666,
      index: 3,
      word: "seattle",
    },
  ],
  "seattle restaurants": [
    {
      entity: "B-CITY",
      score: 0.8563504219055176,
      index: 1,
      word: "seattle",
    },
  ],
  "seattle wa restaurants": [
    {
      entity: "B-CITY",
      score: 0.5729296207427979,
      index: 1,
      word: "seattle",
    },
    {
      entity: "B-STATE",
      score: 0.7850125432014465,
      index: 2,
      word: "wa",
    },
  ],
  "seattle, wa restaurants": [
    {
      entity: "B-CITYSTATE",
      score: 0.9999499320983887,
      index: 1,
      word: "seattle",
    },
    {
      entity: "I-CITYSTATE",
      score: 0.9999974370002747,
      index: 2,
      word: ",",
    },
    {
      entity: "I-CITYSTATE",
      score: 0.9999855160713196,
      index: 3,
      word: "wa",
    },
  ],
  "dumplings in ca": [
    {
      entity: "B-STATE",
      score: 0.998980700969696,
      index: 4,
      word: "ca",
    },
  ],
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.nerThreshold", 0.5]],
  });
  // Stub these out so we don't end up invoking engine calls for now
  // until end-to-end engine calls work
  sinon.stub(MLSuggest, "_findIntent").returns("yelp_intent");
  sinon.stub(MLSuggest, "_findNER").callsFake(query => {
    return nerResultsMap[query] || [];
  });

  registerCleanupFunction(async function () {
    sinon.restore();
  });
});

async function setup() {
  const { removeMocks, remoteClients } = await createAndMockMLRemoteSettings({
    autoDownloadFromRemoteSettings: false,
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.ml.enable", true],
      ["browser.ml.logLevel", "All"],
      ["browser.ml.modelCacheTimeout", 1000],
    ],
  });

  return {
    remoteClients,
    async cleanup() {
      await removeMocks();
      await waitForCondition(
        () => EngineProcess.areAllEnginesTerminated(),
        "Waiting for all of the engines to be terminated.",
        100,
        200
      );
    },
  };
}

// Helper function to test suggestions
async function testSuggestion(
  query,
  expectedCity,
  expectedState,
  remoteClients
) {
  let suggestion = MLSuggest.makeSuggestions(query);
  await remoteClients["ml-onnx-runtime"].rejectPendingDownloads(1);
  suggestion = await suggestion;

  info("Got suggestion for query: " + query);
  info("Got suggestion: " + JSON.stringify(suggestion));
  Assert.ok(suggestion, "MLSuggest returned a result");

  if (expectedCity) {
    Assert.equal(
      suggestion.location.city,
      expectedCity,
      "City extraction is correct"
    );
  }

  if (expectedState) {
    Assert.equal(
      suggestion.location.state,
      expectedState,
      "State extraction is correct"
    );
  }
}

add_task(async function test_MLSuggest() {
  const { cleanup, remoteClients } = await setup();

  await MLSuggest.initialize();
  await testSuggestion(
    "restaurants in seattle, wa",
    "seattle",
    "wa",
    remoteClients
  );
  await testSuggestion(
    "hotels in new york, ny",
    "new york",
    "ny",
    remoteClients
  );
  await testSuggestion("restaurants seattle", "seattle", null, remoteClients);
  await testSuggestion(
    "restaurants in seattle",
    "seattle",
    null,
    remoteClients
  );
  await testSuggestion(
    "restaurants near seattle",
    "seattle",
    null,
    remoteClients
  );
  await testSuggestion("seattle restaurants", "seattle", null, remoteClients);
  await testSuggestion(
    "seattle wa restaurants",
    "seattle",
    "wa",
    remoteClients
  );
  await testSuggestion(
    "seattle, wa restaurants",
    "seattle",
    "wa",
    remoteClients
  );

  await testSuggestion("dumplings in ca", null, "ca", remoteClients);

  Assert.strictEqual(
    Services.prefs.getFloatPref("browser.urlbar.nerThreshold"),
    0.5,
    "nerThreshold pref should have the expected default value"
  );
  await MLSuggest.shutdown();
  await EngineProcess.destroyMLEngine();

  await cleanup();
});
