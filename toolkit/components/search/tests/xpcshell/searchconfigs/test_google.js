/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

const { EnterprisePolicyTesting } = ChromeUtils.importESModule(
  "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
);

const test = new SearchConfigTest({
  identifier: "google",
  aliases: ["@google"],
  default: {
    // Included everywhere apart from the exclusions below. These are basically
    // just excluding what Yandex and Baidu include.
    excluded: [
      {
        regions: ["cn"],
        locales: ["zh-CN"],
      },
    ],
  },
  available: {
    excluded: [
      // Should be available everywhere.
    ],
  },
  details: [
    {
      included: [{ regions: ["us"] }],
      domain: "google.com",
      telemetryId:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr"
          ? "google-b-1-e"
          : "google-b-1-d",
      searchUrlCode:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr"
          ? "client=firefox-b-1-e"
          : "client=firefox-b-1-d",
    },
    {
      excluded: [{ regions: ["us", "by", "kz", "ru", "tr"] }],
      included: [{}],
      domain: "google.com",
      telemetryId:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "google-b-e" : "google-b-d",
      searchUrlCode:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr"
          ? "client=firefox-b-e"
          : "client=firefox-b-d",
    },
    {
      included: [{ regions: ["by", "kz", "ru", "tr"] }],
      domain: "google.com",
      telemetryId: "google-com-nocodes",
    },
  ],
});

add_setup(async function () {
  sinon.spy(NimbusFeatures.searchConfiguration, "onUpdate");
  sinon.stub(NimbusFeatures.searchConfiguration, "ready").resolves();
  await test.setup();

  // This is needed to make sure the search settings can be loaded
  // when the search service is initialized.
  do_get_profile();

  registerCleanupFunction(async () => {
    sinon.restore();
  });
});

add_task(async function test_searchConfig_google() {
  await test.run();
});

add_task(async function test_searchConfig_google_with_pref_param() {
  // Test a couple of configurations with a preference parameter set up.
  const TEST_DATA = [
    {
      locale: "en-US",
      region: "US",
      pref: "google_channel_us",
      // On ESR, the channel parameter is always `entpr`
      expected:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "entpr" : "us_param",
    },
    {
      locale: "en-US",
      region: "GB",
      pref: "google_channel_row",
      // On ESR, the channel parameter is always `entpr`
      expected:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "entpr" : "row_param",
    },
  ];

  const defaultBranch = Services.prefs.getDefaultBranch(
    SearchUtils.BROWSER_SEARCH_PREF
  );
  for (const testData of TEST_DATA) {
    defaultBranch.setCharPref("param." + testData.pref, testData.expected);
  }

  for (const testData of TEST_DATA) {
    info(`Checking region ${testData.region}, locale ${testData.locale}`);
    const { engines } = await test._getEngines(
      testData.region,
      testData.locale
    );

    Assert.ok(
      engines[0].identifier.startsWith("google"),
      "Should have the correct engine"
    );
    console.log(engines[0]);

    const submission = engines[0].getSubmission("test", URLTYPE_SEARCH_HTML);
    Assert.ok(
      submission.uri.query.split("&").includes("channel=" + testData.expected),
      "Should be including the correct preference parameter for the engine"
    );
  }

  // Reset the pref values for next tests
  for (const testData of TEST_DATA) {
    defaultBranch.setCharPref("param." + testData.pref, "");
  }
});

add_task(async function test_searchConfig_google_with_nimbus() {
  let sandbox = sinon.createSandbox();
  // Test a couple of configurations with a preference parameter set up.
  const TEST_DATA = [
    {
      locale: "en-US",
      region: "US",
      // On ESR, the channel parameter is always `entpr`
      expected:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr" ? "entpr" : "nimbus_us_param",
    },
    {
      locale: "en-US",
      region: "GB",
      // On ESR, the channel parameter is always `entpr`
      expected:
        SearchUtils.MODIFIED_APP_CHANNEL == "esr"
          ? "entpr"
          : "nimbus_row_param",
    },
  ];

  Assert.ok(
    NimbusFeatures.searchConfiguration.onUpdate.called,
    "Should register an update listener for Nimbus experiments"
  );
  // Stub getVariable to populate the cache with our expected data
  sandbox.stub(NimbusFeatures.searchConfiguration, "getVariable").returns([
    { key: "google_channel_us", value: "nimbus_us_param" },
    { key: "google_channel_row", value: "nimbus_row_param" },
  ]);
  // Set the pref cache with Nimbus values
  NimbusFeatures.searchConfiguration.onUpdate.firstCall.args[0]();

  for (const testData of TEST_DATA) {
    info(`Checking region ${testData.region}, locale ${testData.locale}`);
    const { engines } = await test._getEngines(
      testData.region,
      testData.locale
    );

    Assert.ok(
      engines[0].identifier.startsWith("google"),
      "Should have the correct engine"
    );
    console.log(engines[0]);

    const submission = engines[0].getSubmission("test", URLTYPE_SEARCH_HTML);
    Assert.ok(
      NimbusFeatures.searchConfiguration.ready.called,
      "Should wait for Nimbus to get ready"
    );
    Assert.ok(
      NimbusFeatures.searchConfiguration.getVariable,
      "Should call NimbusFeatures.searchConfiguration.getVariable to populate the cache"
    );
    Assert.ok(
      submission.uri.query.split("&").includes("channel=" + testData.expected),
      "Should be including the correct preference parameter for the engine"
    );
  }

  sandbox.restore();
});

add_task(async function test_searchConfig_google_enterprise() {
  const TEST_DATA = [
    {
      locale: "en-US",
      region: "US",
    },
    {
      locale: "en-US",
      region: "GB",
    },
  ];

  Services.search.wrappedJSObject.reset();
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      BlockAboutSupport: true,
    },
  });
  await Services.search.init();

  for (const testData of TEST_DATA) {
    info(`Checking region ${testData.region}, locale ${testData.locale}`);
    const { engines } = await test._getEngines(
      testData.region,
      testData.locale
    );
    Assert.ok(
      engines[0].identifier.startsWith("google"),
      "Should have the correct engine"
    );
    const submission = engines[0].getSubmission("test", URLTYPE_SEARCH_HTML);
    info(submission.uri.query);
    Assert.ok(
      submission.uri.query.split("&").includes("channel=entpr"),
      "Should be including the enterprise parameter for the engine"
    );
  }
});
