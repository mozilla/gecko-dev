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
  "ramen ra": [
    {
      entity: "B-CITY",
      score: 0.6767462491989136,
      index: 3,
      word: "ra",
    },
  ],
  "plumbers in seattle,wa": [
    {
      entity: "B-CITYSTATE",
      index: 4,
      score: 0.99997478723526,
      word: "seattle",
    },
    {
      entity: "I-CITYSTATE",
      index: 5,
      score: 0.9999989867210388,
      word: ",",
    },
    {
      entity: "I-CITYSTATE",
      index: 6,
      score: 0.9999985098838806,
      word: "wa",
    },
  ],
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.nerThreshold", 0.5],
      ["browser.urlbar.intentThreshold", 0.5],
    ],
  });
  // Stub these out so we don't end up invoking engine calls for now
  // until end-to-end engine calls work
  sinon
    .stub(MLSuggest, "_findIntent")
    .returns({ label: "yelp_intent", score: 0.9 });
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
  await testSuggestion(
    "plumbers in seattle,wa",
    "seattle",
    "wa",
    remoteClients
  );

  Assert.strictEqual(
    Services.prefs.getFloatPref("browser.urlbar.nerThreshold"),
    0.5,
    "nerThreshold pref should have the expected default value"
  );

  await MLSuggest.shutdown();
  await EngineProcess.destroyMLEngine();

  await cleanup();
  sinon.restore();
});

/**
 *
 */
class MLEngineThatFails {
  static cnt = 0;
  // prefix with _query avoids lint error
  async run(_query) {
    MLEngineThatFails.cnt += 1;
    if (MLEngineThatFails.cnt === 1) {
      throw Error("Fake error");
    }
    return [
      {
        label: "yelp_intent",
        score: 0.9,
      },
    ];
  }
}

add_task(async function test_MLSuggest_restart_after_failure() {
  // Restore any previous stubs
  sinon.restore();

  sinon.stub(MLSuggest, "createEngine").callsFake(() => {
    return new MLEngineThatFails();
  });
  const { cleanup } = await setup();

  await MLSuggest.initialize();

  let suggestion = await MLSuggest.makeSuggestions(
    "restaurants in seattle, wa"
  );
  Assert.ok(!suggestion, "Suggestion should be null due to error");
  // second request
  let suggestion2;
  await TestUtils.waitForCondition(async () => {
    suggestion2 = await MLSuggest.makeSuggestions("restaurants in seattle, wa");
    return !!suggestion2;
  }, "Wait for suggestion after reinitialization");
  Assert.ok(suggestion2, "Suggestion should be good");
  const expected = { intent: "yelp_intent" };
  Assert.deepEqual(suggestion2.intent, expected.intent);

  await MLSuggest.shutdown();
  await EngineProcess.destroyMLEngine();
  await cleanup();
  sinon.restore();
});

/**
 * For Mocking MLEngine with low score
 */
class MLEngineWithLowYelpIntent {
  // prefix with _query avoids lint error
  async run(_query) {
    return [
      {
        label: "yelp_intent",
        score: 0.3,
      },
    ];
  }
}

add_task(async function test_MLSuggest_low_intent_threshold() {
  // Restore any previous stubs
  sinon.restore();

  sinon.stub(MLSuggest, "createEngine").callsFake(() => {
    return new MLEngineWithLowYelpIntent();
  });
  const { cleanup } = await setup();

  await MLSuggest.initialize();

  let suggestion = await MLSuggest.makeSuggestions("no yelp");
  Assert.ok(suggestion, "Suggestion should be good");
  const expected = { intent: "" };
  Assert.deepEqual(suggestion.intent, expected.intent);

  await MLSuggest.shutdown();
  await EngineProcess.destroyMLEngine();
  await cleanup();
  sinon.restore();
});

/**
 * For Mocking MLEngine with positive yelp itent
 */
class MLEngineWithHighYelpIntent {
  // prefix with _query avoids lint error
  async run(_query) {
    return [
      {
        label: "yelp_intent",
        score: 0.9,
      },
    ];
  }
}

add_task(async function test_MLSuggest_city_dup_in_subject() {
  // Restore any previous stubs
  sinon.restore();

  sinon.stub(MLSuggest, "createEngine").callsFake(() => {
    return new MLEngineWithHighYelpIntent();
  });
  sinon.stub(MLSuggest, "_findNER").callsFake(query => {
    return nerResultsMap[query] || [];
  });
  const { cleanup } = await setup();

  await MLSuggest.initialize();

  let suggestion = await MLSuggest.makeSuggestions("ramen ra");
  Assert.ok(suggestion, "Suggestion should be good");
  const expected = {
    intent: "yelp_intent",
    location: { city: "ra", state: null },
    subject: "ramen",
  };
  Assert.deepEqual(suggestion.intent, expected.intent);
  Assert.deepEqual(suggestion.location, expected.location);
  Assert.deepEqual(suggestion.subject, expected.subject);

  await MLSuggest.shutdown();
  await EngineProcess.destroyMLEngine();
  await cleanup();
  sinon.restore();
});
