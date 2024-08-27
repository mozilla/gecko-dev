/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that if Nimbus provides invalid values for extraParams, the search
 * service can still initialize and run.
 * This is separate to test_getSubmission_params_prefNimbus so that initialization
 * can be properly tested.
 */

"use strict";

const { NimbusFeatures } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);

const baseURL = "https://example.com/search?";

let getVariableStub;
let updateStub;

const CONFIG = [
  {
    identifier: "preferenceEngine",
    base: {
      urls: {
        search: {
          base: "https://example.com/search",
          params: [
            {
              name: "code",
              experimentConfig: "code",
            },
            {
              name: "test",
              experimentConfig: "test",
            },
          ],
          searchTermParamName: "q",
        },
      },
    },
  },
];

add_setup(async function () {
  consoleAllowList.push("Failed to load nimbus variables for extraParams");

  updateStub = sinon.stub(NimbusFeatures.search, "onUpdate");
  getVariableStub = sinon.stub(NimbusFeatures.search, "getVariable");
  sinon.stub(NimbusFeatures.search, "ready").resolves();

  // The test engines used in this test need to be recognized as application
  // provided engines, or their MozParams will be ignored.
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
});

add_task(async function test_bad_nimbus_setting_on_init() {
  getVariableStub.withArgs("extraParams").returns({ foo: "bar" });

  await Services.search.init();

  Assert.ok(
    updateStub.called,
    "Should have called onUpdate to listen for future updates"
  );

  const engine = Services.search.getEngineById("preferenceEngine");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "q=foo",
    "Should have been able to get a submission URL"
  );
});

add_task(async function test_switch_to_good_nimbus_setting() {
  // Switching to a good structure should provide the parameter.
  getVariableStub.withArgs("extraParams").returns([
    {
      key: "code",
      // The & and = in this parameter are to check that they are correctly
      // encoded, and not treated as a separate parameter.
      value: "supergood&id=unique123456",
    },
  ]);

  updateStub.firstCall.args[0]();

  const engine = Services.search.getEngineById("preferenceEngine");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "code=supergood%26id%3Dunique123456&q=foo",
    "Should have got the submission URL with the updated code"
  );
});

add_task(async function test_switch_back_to_bad_nimbus_setting() {
  // Switching from a valid Nimbus setting to an invalid setting should fallback
  // to a valid submission URL.
  getVariableStub.withArgs("extraParams").returns({ bar: "foo" });

  updateStub.firstCall.args[0]();

  const engine = Services.search.getEngineById("preferenceEngine");
  Assert.equal(
    engine.getSubmission("foo").uri.spec,
    baseURL + "q=foo",
    "Should have not have the extra parameters on the submission URL after a bad update"
  );
});
