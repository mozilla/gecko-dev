/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { LABS_STATE, LinkPreview } = ChromeUtils.importESModule(
  "moz-src:///browser/components/genai/LinkPreview.sys.mjs"
);
const { NimbusTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

/**
 * Test nimbus experiment sets link preview enabled prefs.
 */
add_task(async function test_nimbus_link_preview() {
  is(
    Services.prefs.getBoolPref("browser.ml.linkPreview.enabled"),
    false,
    "default false"
  );
  is(LinkPreview.canShowLegacy, false, "not legacy yet");
  is(
    Services.prefs.prefHasUserValue("browser.ml.linkPreview.labs"),
    false,
    "not yet enrolled in labs"
  );

  let cleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "linkPreviews",
    value: { enabled: true },
  });

  ok(
    Services.prefs.getBoolPref("browser.ml.linkPreview.enabled"),
    "nimbus setPref enabled"
  );
  ok(LinkPreview.canShowLegacy, "now legacy");
  is(
    Services.prefs.getIntPref("browser.ml.linkPreview.labs"),
    LABS_STATE.ENROLLED,
    "indicates enrolled in labs"
  );

  await cleanup();

  ok(
    Services.prefs.getBoolPref("browser.ml.linkPreview.enabled"),
    "still enabled"
  );
  ok(LinkPreview.canShowLegacy, "still legacy");
  is(
    Services.prefs.getIntPref("browser.ml.linkPreview.labs"),
    LABS_STATE.ROLLOUT_ENDED,
    "transitioned from labs"
  );

  ok(
    Services.prefs.getStringPref("browser.ml.linkPreview.nimbus"),
    "nimbus slug set"
  );

  Services.prefs.clearUserPref("browser.ml.linkPreview.enabled");
  Services.prefs.clearUserPref("browser.ml.linkPreview.labs");
  Services.prefs.clearUserPref("browser.ml.linkPreview.nimbus");
});

/**
 * Check that enrolling into linkPreviews experiments sets user prefs.
 */
add_task(async function test_nimbus_user_prefs() {
  const foo = "browser.ml.linkPreview.foo";
  const nimbus = "browser.ml.linkPreview.nimbus";
  const blockListEnabled = "browser.ml.linkPreview.blockListEnabled";
  ok(!Services.prefs.prefHasUserValue(foo), "No user foo pref yet");
  ok(!Services.prefs.prefHasUserValue(nimbus), "No user nimbus pref yet");

  let cleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "linkPreviews",
    value: {
      prefs: {
        foo: { value: "bar" },
      },
    },
  });

  is(Services.prefs.getStringPref(foo), "bar", "Set user pref with experiment");
  ok(Services.prefs.prefHasUserValue(foo), "foo pref has user value");
  const nimbusValue = Services.prefs.getStringPref(nimbus);
  ok(nimbusValue, "Set some nimbus slug");
  ok(Services.prefs.prefHasUserValue(nimbus), "nimbus pref has user value");

  await cleanup();

  is(Services.prefs.getStringPref(foo), "bar", "foo pref still set");
  is(
    Services.prefs.getStringPref(nimbus),
    nimbusValue,
    "nimbus pref still set"
  );
  ok(
    Services.prefs.getBoolPref(blockListEnabled),
    "blockListEnabled is default value"
  );
  ok(
    !Services.prefs.prefHasUserValue(blockListEnabled),
    "sidebar used default value"
  );

  cleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "linkPreviews",
    value: {
      prefs: {
        foo: {},
        blockListEnabled: { value: false },
      },
    },
  });

  ok(!Services.prefs.prefHasUserValue(foo), "foo pref no longer set");
  isnot(
    Services.prefs.getStringPref(nimbus),
    nimbusValue,
    "nimbus pref changed"
  );
  ok(
    !Services.prefs.getBoolPref(blockListEnabled),
    "blockListEnabled set to false"
  );
  ok(
    Services.prefs.prefHasUserValue(blockListEnabled),
    "blockListEnabled pref has user value"
  );

  await cleanup();
  Services.prefs.clearUserPref(nimbus);
  Services.prefs.clearUserPref(blockListEnabled);
});

/**
 * Check that enrolling into linkPreviews experiments sets default prefs.
 */
add_task(async function test_nimbus_default_prefs() {
  const pref = "browser.ml.linkPreview.blockListEnabled";
  ok(Services.prefs.getBoolPref(pref), "blockListEnabled is default value");
  ok(
    !Services.prefs.prefHasUserValue(pref),
    "blockListEnabled used default value"
  );

  const cleanup = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "linkPreviews",
    value: {
      prefs: {
        blockListEnabled: { branch: "default", value: false },
      },
    },
  });

  ok(!Services.prefs.getBoolPref(pref), "blockListEnabled set to false");
  ok(
    !Services.prefs.prefHasUserValue(pref),
    "blockListEnabled still is default value"
  );

  await cleanup();
  Services.prefs.clearUserPref("browser.ml.linkPreview.nimbus");
  Services.prefs.getDefaultBranch("").setBoolPref(pref, true);
});

/**
 * Check that rollout sets prefs then prefer experiment.
 */
add_task(async function test_nimbus_rollout_experiment() {
  const foo = "browser.ml.linkPreview.foo";
  const nimbus = "browser.ml.linkPreview.nimbus";
  const cleanRollout = await NimbusTestUtils.enrollWithFeatureConfig(
    {
      featureId: "linkPreviews",
      value: {
        prefs: {
          foo: { value: "roll" },
        },
      },
    },
    { isRollout: true }
  );

  is(Services.prefs.getStringPref(foo), "roll", "Set user pref with rollout");
  const nimbusValue = Services.prefs.getStringPref(nimbus);
  ok(nimbusValue, "Set some nimbus slug");

  const cleanExperiment = await NimbusTestUtils.enrollWithFeatureConfig({
    featureId: "linkPreviews",
    value: {
      prefs: {
        foo: { value: "exp" },
      },
    },
  });

  is(Services.prefs.getStringPref(foo), "exp", "Set user pref with experiment");
  isnot(
    Services.prefs.getStringPref(nimbus),
    nimbusValue,
    "nimbus pref changed by experiment"
  );

  await cleanRollout();
  await cleanExperiment();
  Services.prefs.clearUserPref(nimbus);
  Services.prefs.clearUserPref(foo);
});
