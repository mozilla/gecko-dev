/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This tests that the engine selector works handles various remote settings
 * scenarios correctly, e.g. configuration updates, bad databases.
 */

"use strict";

const TEST_CONFIG = [
  {
    recordType: "defaultEngines",
    globalDefault: "lycos",
    specificDefaults: [],
  },
  {
    orders: [],
    recordType: "engineOrders",
  },
];

for (let engineName of ["lycos", "altavista", "aol", "excite"]) {
  TEST_CONFIG.push({
    base: {
      name: engineName,
      urls: {
        search: {
          base: `https://example.com/${engineName}`,
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: {
          allRegionsAndLocales: "true",
        },
      },
    ],
    identifier: engineName,
    recordType: "engine",
  });
}

let getStub;

add_setup(async function () {
  const searchConfigSettings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
  getStub = sinon.stub(searchConfigSettings, "get");

  // We expect this error from remove settings as we're invalidating the
  // signature.
  consoleAllowList.push("Invalid content signature (abc)");
  // We also test returning an empty configuration.
  consoleAllowList.push("Received empty search configuration");

  registerCleanupFunction(async () => {
    sinon.restore();
  });
});

add_task(async function test_selector_basic_get() {
  const listenerSpy = sinon.spy();
  const engineSelector = new SearchEngineSelector(listenerSpy);
  getStub.onFirstCall().returns(TEST_CONFIG);

  const { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "default",
  });

  Assert.deepEqual(
    engines.map(e => e.name),
    ["lycos", "altavista", "aol", "excite"],
    "Should have obtained the correct data from the database."
  );
  Assert.ok(listenerSpy.notCalled, "Should not have called the listener");
});

add_task(async function test_selector_get_reentry() {
  const listenerSpy = sinon.spy();
  const engineSelector = new SearchEngineSelector(listenerSpy);
  let promise = Promise.withResolvers();
  getStub.resetHistory();
  getStub.onFirstCall().returns(promise.promise);
  delete engineSelector._configuration;

  let firstResult;
  let secondResult;

  const firstCallPromise = engineSelector
    .fetchEngineConfiguration({
      locale: "en-US",
      region: "default",
    })
    .then(result => (firstResult = result.engines));

  const secondCallPromise = engineSelector
    .fetchEngineConfiguration({
      locale: "en-US",
      region: "default",
    })
    .then(result => (secondResult = result.engines));

  Assert.strictEqual(
    firstResult,
    undefined,
    "Should not have returned the first result yet."
  );

  Assert.strictEqual(
    secondResult,
    undefined,
    "Should not have returned the second result yet."
  );

  promise.resolve(TEST_CONFIG);

  await Promise.all([firstCallPromise, secondCallPromise]);
  Assert.deepEqual(
    firstResult.map(e => e.name),
    ["lycos", "altavista", "aol", "excite"],
    "Should have returned the correct data to the first call"
  );

  Assert.deepEqual(
    secondResult.map(e => e.name),
    ["lycos", "altavista", "aol", "excite"],
    "Should have returned the correct data to the second call"
  );
  Assert.ok(listenerSpy.notCalled, "Should not have called the listener");
});

add_task(async function test_selector_config_update() {
  const listenerSpy = sinon.spy();
  const engineSelector = new SearchEngineSelector(listenerSpy);
  getStub.resetHistory();
  getStub.onFirstCall().returns(TEST_CONFIG);

  const { engines } = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "default",
  });

  Assert.deepEqual(
    engines.map(e => e.name),
    ["lycos", "altavista", "aol", "excite"],
    "Should have got the correct configuration"
  );

  Assert.ok(listenerSpy.notCalled, "Should not have called the listener yet");

  const NEW_DATA = [
    {
      recordType: "engine",
      identifier: "askjeeves",
      base: { name: "askjeeves" },
      variants: [{ environment: { allRegionsAndLocales: "true" } }],
    },
    {
      recordType: "defaultEngines",
      globalDefault: "askjeeves",
      specificDefaults: [],
    },
    {
      orders: [],
      recordType: "engineOrders",
    },
  ];

  getStub.resetHistory();
  getStub.onFirstCall().returns(NEW_DATA);
  await RemoteSettings(SearchUtils.SETTINGS_KEY).emit("sync", {
    data: {
      current: NEW_DATA,
    },
  });

  Assert.ok(listenerSpy.called, "Should have called the listener");

  const result = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "default",
  });

  Assert.deepEqual(
    result.engines.map(e => e.name),
    ["askjeeves"],
    "Should have updated the configuration with the new data"
  );
});

add_task(async function test_selector_db_modification() {
  const engineSelector = new SearchEngineSelector();
  // Fill the database with some values that we can use to test that it is cleared.
  const db = RemoteSettings(SearchUtils.SETTINGS_KEY).db;
  await db.importChanges(
    {},
    Date.now(),
    [
      {
        recordType: "engine",
        id: "b70edfdd-1c3f-4b7b-ab55-38cb048636c0",
        identifier: "askjeeves",
        base: { name: "askjeeves" },
        variants: [{ environment: { allRegionsAndLocales: "true" } }],
      },
    ],
    { clear: true }
  );

  // Stub the get() so that the first call simulates a signature error, and
  // the second simulates success reading from the dump.
  getStub.resetHistory();
  getStub
    .onFirstCall()
    .rejects(new RemoteSettingsClient.InvalidSignatureError("abc"));
  getStub.onSecondCall().returns(TEST_CONFIG);

  let result = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "default",
  });

  Assert.ok(
    getStub.calledTwice,
    "Should have called the get() function twice."
  );

  const databaseEntries = await db.list();
  Assert.equal(databaseEntries.length, 0, "Should have cleared the database.");

  Assert.deepEqual(
    result.engines.map(e => e.name),
    ["lycos", "altavista", "aol", "excite"],
    "Should have returned the correct data."
  );
});

add_task(async function test_selector_db_modification_never_succeeds() {
  const engineSelector = new SearchEngineSelector();
  // Fill the database with some values that we can use to test that it is cleared.
  const db = RemoteSettings(SearchUtils.SETTINGS_KEY).db;
  await db.importChanges(
    {},
    Date.now(),
    [
      {
        recordType: "engine",
        id: "b70edfdd-1c3f-4b7b-ab55-38cb048636c0",
        identifier: "askjeeves",
        base: { name: "askjeeves" },
        variants: [{ environment: { allRegionsAndLocales: "true" } }],
      },
      {
        recordType: "defaultEngines",
        id: "b70edfdd-1c3f-4b7b-ab55-38cb048636c1",
        globalDefault: "lycos",
        specificDefaults: [],
      },
      {
        id: "b70edfdd-1c3f-4b7b-ab55-38cb048636c2",
        orders: [],
        recordType: "engineOrders",
      },
    ],
    {
      clear: true,
    }
  );

  // Now simulate the condition where for some reason we never get a
  // valid result.
  getStub.reset();
  getStub.rejects(new RemoteSettingsClient.InvalidSignatureError("abc"));

  await Assert.rejects(
    engineSelector.fetchEngineConfiguration({
      locale: "en-US",
      region: "default",
    }),
    ex => ex.result == Cr.NS_ERROR_UNEXPECTED,
    "Should have rejected loading the engine configuration"
  );

  Assert.ok(
    getStub.calledTwice,
    "Should have called the get() function twice."
  );

  const databaseEntries = await db.list();
  Assert.equal(databaseEntries.length, 0, "Should have cleared the database.");
});

add_task(async function test_empty_results() {
  // Check that returning an empty result re-tries.
  const engineSelector = new SearchEngineSelector();
  // Fill the database with some values that we can use to test that it is cleared.
  const db = RemoteSettings(SearchUtils.SETTINGS_KEY).db;
  await db.importChanges(
    {},
    Date.now(),
    [
      {
        recordType: "engine",
        id: "b70edfdd-1c3f-4b7b-ab55-38cb048636c0",
        identifier: "askjeeves",
        base: { name: "askjeeves" },
        variants: [{ environment: { allRegionsAndLocales: "true" } }],
      },
    ],
    {
      clear: true,
    }
  );

  // Stub the get() so that the first call simulates an empty database, and
  // the second simulates success reading from the dump.
  getStub.resetHistory();
  getStub.onFirstCall().returns([]);
  getStub.onSecondCall().returns(TEST_CONFIG);

  let result = await engineSelector.fetchEngineConfiguration({
    locale: "en-US",
    region: "default",
  });

  Assert.ok(
    getStub.calledTwice,
    "Should have called the get() function twice."
  );

  const databaseEntries = await db.list();
  Assert.equal(databaseEntries.length, 0, "Should have cleared the database.");

  Assert.deepEqual(
    result.engines.map(e => e.name),
    ["lycos", "altavista", "aol", "excite"],
    "Should have returned the correct data."
  );
});
