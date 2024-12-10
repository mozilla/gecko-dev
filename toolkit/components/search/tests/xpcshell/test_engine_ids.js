/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests Search Engine IDs are created correctly.
 */

"use strict";

const CONFIG = [
  {
    identifier: "appDefault",
    base: { name: "Application Default" },
  },
];

add_setup(async function () {
  useHttpServer();
  SearchTestUtils.setRemoteSettingsConfig(CONFIG);
  await Services.search.init();
});

add_task(async function test_app_provided_engine_id() {
  let appDefault = Services.search.defaultEngine;

  Assert.equal(
    appDefault.name,
    "Application Default",
    "Should have installed the application engine as default."
  );
  Assert.equal(
    appDefault.id,
    "appDefault",
    "The application id should match the configuration."
  );
});

add_task(async function test_addon_engine_id() {
  await SearchTestUtils.installSearchExtension({
    name: "AddonEngine",
    id: "addon@tests.mozilla.org",
  });

  let addonEngine = Services.search.getEngineByName("AddonEngine");
  Assert.equal(
    addonEngine.id,
    "addon@tests.mozilla.orgdefault",
    "The Addon Search Engine id should be the webextension id + the locale."
  );
});

add_task(async function test_user_engine_id() {
  let promiseEngineAdded = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.ADDED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  await Services.search.addUserEngine(
    "user",
    "https://example.com/user?q={searchTerms}",
    "u"
  );

  await promiseEngineAdded;
  let userEngine = Services.search.getEngineByName("user");

  Assert.ok(userEngine, "Should have installed the User Search Engine.");
  Assert.ok(userEngine.id, "The User Search Engine should have an id.");
  Assert.equal(
    userEngine.id.length,
    36,
    "The User Search Engine id should be a 36 character uuid."
  );
});

add_task(async function test_open_search_engine_id() {
  let openSearchEngine = await SearchTestUtils.installOpenSearchEngine({
    url: `${gHttpURL}/opensearch/simple.xml`,
  });

  Assert.ok(openSearchEngine, "Should have installed the Open Search Engine.");
  Assert.ok(openSearchEngine.id, "The Open Search Engine should have an id.");
  Assert.equal(
    openSearchEngine.id.length,
    36,
    "The Open Search Engine id should be a 36 character uuid."
  );
});

add_task(async function test_enterprise_policy_engine_id() {
  await setupPolicyEngineWithJson({
    policies: {
      SearchEngines: {
        Add: [
          {
            Name: "policy",
            Description: "Test policy engine",
            IconURL: "data:image/gif;base64,R0lGODl",
            Alias: "p",
            URLTemplate: "https://example.com?q={searchTerms}",
            SuggestURLTemplate: "https://example.com/suggest/?q={searchTerms}",
          },
        ],
      },
    },
  });

  let policyEngine = Services.search.getEngineByName("policy");

  Assert.ok(policyEngine, "Should have installed the Policy Engine.");
  Assert.ok(policyEngine.id, "The Policy Engine should have an id.");
  Assert.equal(
    policyEngine.id,
    "policy-policy",
    "The Policy Engine id should be 'policy-' + 'the name of the policy engine'."
  );
});
