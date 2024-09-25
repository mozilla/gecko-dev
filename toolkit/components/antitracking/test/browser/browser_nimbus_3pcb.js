/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

add_task(async function test_3pcb_nimbus_feature() {
  await ExperimentAPI.ready();

  let original3pcbValue = Services.prefs.getBoolPref(
    "network.cookie.cookieBehavior.optInPartitioning"
  );
  let original3pcbPBMValue = Services.prefs.getBoolPref(
    "network.cookie.cookieBehavior.optInPartitioning.pbmode"
  );

  info("Enroll with the third party cookie blocking feature.");
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "thirdPartyCookieBlocking",
    value: {
      enabled: true,
      enabledPBM: true,
    },
  });

  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning"
    ),
    true,
    "The third party cookie blocking pref has been set correctly"
  );
  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning.pbmode"
    ),
    true,
    "The third party cookie blocking PBM pref has been set correctly"
  );

  is(
    Services.prefs
      .getDefaultBranch("")
      .getBoolPref("network.cookie.cookieBehavior.optInPartitioning"),
    true,
    "The third party cookie blocking pref has been set correctly to the default branch"
  );
  is(
    Services.prefs
      .getDefaultBranch("")
      .getBoolPref("network.cookie.cookieBehavior.optInPartitioning.pbmode"),
    true,
    "The third party cookie blocking PBM pref has been set correctly to the default branch"
  );

  doExperimentCleanup();

  info("Check the third party cookie blocking pref has been reset correctly.");
  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning"
    ),
    original3pcbValue,
    "The third party cookie blocking pref has been reset correctly"
  );
  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning.pbmode"
    ),
    original3pcbPBMValue,
    "The third party cookie blocking PBM pref has been reset correctly"
  );

  info(
    "Enroll with the third party cookie blocking feature with different settings."
  );
  doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "thirdPartyCookieBlocking",
    value: {
      enabled: false,
      enabledPBM: false,
    },
  });

  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning"
    ),
    false,
    "The third party cookie blocking pref has been set correctly"
  );
  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning.pbmode"
    ),
    false,
    "The third party cookie blocking PBM pref has been set correctly"
  );

  is(
    Services.prefs
      .getDefaultBranch("")
      .getBoolPref("network.cookie.cookieBehavior.optInPartitioning"),
    false,
    "The third party cookie blocking pref has been set correctly to the default branch"
  );
  is(
    Services.prefs
      .getDefaultBranch("")
      .getBoolPref("network.cookie.cookieBehavior.optInPartitioning.pbmode"),
    false,
    "The third party cookie blocking PBM pref has been set correctly to the default branch"
  );

  doExperimentCleanup();

  info("Check the third party cookie blocking pref has been reset correctly.");
  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning"
    ),
    original3pcbValue,
    "The third party cookie blocking pref has been reset correctly"
  );
  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning.pbmode"
    ),
    original3pcbPBMValue,
    "The third party cookie blocking PBM pref has been reset correctly"
  );
});
