/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test of a search engine's telemetryId.
 */

"use strict";

add_setup(async function () {
  SearchTestUtils.setRemoteSettingsConfig([
    {
      identifier: "basic",
      base: {
        name: "enterprise-a",
      },
    },
    {
      identifier: "suffix",
      base: {
        name: "enterprise-b",
      },
      variants: [
        {
          environment: { allRegionsAndLocales: true },
          telemetrySuffix: "b",
        },
      ],
    },
  ]);

  const result = await Services.search.init();
  Assert.ok(
    Components.isSuccessCode(result),
    "Should have initialized the service"
  );

  useHttpServer();
});

function checkIdentifier(engineName, expectedIdentifier, expectedTelemetryId) {
  const engine = Services.search.getEngineByName(engineName);
  Assert.ok(
    engine instanceof Ci.nsISearchEngine,
    "Should be derived from nsISearchEngine"
  );

  Assert.equal(
    engine.telemetryId,
    expectedTelemetryId,
    "Should have the correct telemetry Id"
  );

  // TODO: Bug 1877721 - We have 3 forms of identifiers which causes confusion,
  // we can remove the identifier for nsISearchEngine.
  Assert.equal(
    engine.identifier,
    expectedIdentifier,
    "Should have the correct identifier"
  );
}

add_task(async function test_appProvided_basic() {
  checkIdentifier("enterprise-a", "basic", "basic");
});

add_task(async function test_appProvided_suffix() {
  checkIdentifier("enterprise-b", "suffix-b", "suffix-b");
});

add_task(async function test_opensearch() {
  await SearchTestUtils.installOpenSearchEngine({
    url: `${gHttpURL}/data/engine.xml`,
  });

  // An OpenSearch engine won't have a dedicated identifier because it's not
  // built-in.
  checkIdentifier(kTestEngineName, null, `other-${kTestEngineName}`);
});

add_task(async function test_webExtension() {
  await SearchTestUtils.installSearchExtension({
    id: "enterprise-c",
    name: "Enterprise C",
  });

  // A WebExtension engine won't have a dedicated identifier because it's not
  // built-in.
  checkIdentifier("Enterprise C", null, `other-Enterprise C`);
});
