/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

/**
 * Check that enrolling into sidebar experiments sets user prefs
 */
add_task(async function test_nimbus_user_prefs() {
  const main = "sidebar.main.tools";
  const nimbus = "sidebar.nimbus";
  const vertical = "sidebar.verticalTabs";

  Assert.ok(!Services.prefs.prefHasUserValue(main), "No user main pref yet");
  Assert.ok(
    !Services.prefs.prefHasUserValue(nimbus),
    "No user nimbus pref yet"
  );

  let cleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "sidebar",
    value: {
      "main.tools": "bar",
    },
  });

  Assert.equal(
    Services.prefs.getStringPref(main),
    "bar",
    "Set user pref with experiment"
  );
  Assert.ok(Services.prefs.prefHasUserValue(main), "main pref has user value");
  const nimbusValue = Services.prefs.getStringPref(nimbus);
  Assert.ok(nimbusValue, "Set some nimbus slug");
  Assert.ok(
    Services.prefs.prefHasUserValue(nimbus),
    "nimbus pref has user value"
  );

  cleanup();

  Assert.equal(
    Services.prefs.getStringPref(main),
    "bar",
    "main pref still set"
  );
  Assert.equal(
    Services.prefs.getStringPref(nimbus),
    nimbusValue,
    "nimbus pref still set"
  );
  Assert.ok(!Services.prefs.getBoolPref(vertical), "vertical is default value");
  Assert.ok(
    !Services.prefs.prefHasUserValue(vertical),
    "vertical used default value"
  );

  cleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "sidebar",
    value: {
      "main.tools": "aichat,syncedtabs,history",
      verticalTabs: true,
    },
  });

  Assert.ok(!Services.prefs.prefHasUserValue(main), "main pref no longer set");
  Assert.notEqual(
    Services.prefs.getStringPref(nimbus),
    nimbusValue,
    "nimbus pref changed"
  );
  Assert.ok(Services.prefs.getBoolPref(vertical), "vertical set to true");
  Assert.ok(
    Services.prefs.prefHasUserValue(vertical),
    "vertical pref has user value"
  );

  cleanup();
  Services.prefs.clearUserPref(nimbus);
  Services.prefs.clearUserPref(vertical);
});

/**
 * Check that rollout sets prefs then prefer experiment
 */
add_task(async function test_nimbus_rollout_experiment() {
  const revamp = "sidebar.revamp";
  const nimbus = "sidebar.nimbus";
  await SpecialPowers.pushPrefEnv({ clear: [[revamp]] });

  const cleanRollout = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: "sidebar",
      value: { revamp: true },
    },
    { isRollout: true }
  );

  Assert.ok(Services.prefs.getBoolPref(revamp), "Set user pref with rollout");
  const nimbusValue = Services.prefs.getStringPref(nimbus);
  Assert.ok(nimbusValue, "Set some nimbus slug");

  const cleanExperiment = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "sidebar",
    value: { revamp: false },
  });

  Assert.ok(
    !Services.prefs.getBoolPref(revamp),
    "revamp pref flipped by experiment"
  );
  Assert.notEqual(
    Services.prefs.getStringPref(nimbus),
    nimbusValue,
    "nimbus pref changed by experiment"
  );

  cleanRollout();
  cleanExperiment();
  Services.prefs.clearUserPref(nimbus);
});

/**
 * Check that multi-feature sidebar and chatbot sets prefs
 */
add_task(async function test_nimbus_multi_feature() {
  const chatbot = "browser.ml.chat.enabled";
  const sidebar = "sidebar.main.tools";
  Assert.ok(!Services.prefs.prefHasUserValue(chatbot), "chatbot is default");
  Assert.ok(!Services.prefs.prefHasUserValue(sidebar), "sidebar is default");

  const cleanup = await ExperimentFakes.enrollmentHelper(
    ExperimentFakes.recipe("foo", {
      branches: [
        {
          slug: "variant",
          features: [
            {
              featureId: "sidebar",
              value: { "main.tools": "syncedtabs,history" },
            },
            {
              featureId: "chatbot",
              value: { prefs: { enabled: { value: true } } },
            },
          ],
        },
      ],
    })
  );

  Assert.ok(Services.prefs.prefHasUserValue(chatbot), "chatbot user pref set");
  Assert.ok(Services.prefs.prefHasUserValue(sidebar), "sidebar user pref set");

  cleanup();

  Assert.ok(Services.prefs.prefHasUserValue(chatbot), "chatbot pref still set");
  Assert.ok(Services.prefs.prefHasUserValue(sidebar), "sidebar pref still set");

  Services.prefs.clearUserPref(chatbot);
  Services.prefs.clearUserPref(sidebar);
  Services.prefs.clearUserPref("browser.ml.chat.nimbus");
  Services.prefs.clearUserPref("sidebar.nimbus");
});
