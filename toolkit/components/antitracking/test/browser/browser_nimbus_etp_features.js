/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);

const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);

add_task(async function test_etp_features() {
  await ExperimentAPI.ready();

  info("Set the ETP category to strict");
  await SpecialPowers.pushPrefEnv({
    set: [["browser.contentblocking.category", "strict"]],
  });

  // Enroll with the strict ETP features, and disable some features in the
  // enrollment.
  info("Enroll with the strict ETP features.");
  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "etpStrictFeatures",
    value: {
      features:
        "-tp,-tpPrivate,cookieBehavior0,cookieBehaviorPBM0,cm,fp,stp,emailTP,emailTPPrivate,lvl2,rp,rpTop,ocsp,qps,qpsPBM,fpp,fppPrivate,-3pcd",
    },
  });

  info("Check the strict ETP related prefs are set correctly.");
  is(
    Services.prefs.getCharPref("browser.contentblocking.features.strict"),
    "-tp,-tpPrivate,cookieBehavior0,cookieBehaviorPBM0,cm,fp,stp,emailTP,emailTPPrivate,lvl2,rp,rpTop,ocsp,qps,qpsPBM,fpp,fppPrivate,-3pcd",
    "The strict ETP features should be set correctly"
  );
  is(
    Services.prefs
      .getDefaultBranch("")
      .getCharPref("browser.contentblocking.features.strict"),
    "-tp,-tpPrivate,cookieBehavior0,cookieBehaviorPBM0,cm,fp,stp,emailTP,emailTPPrivate,lvl2,rp,rpTop,ocsp,qps,qpsPBM,fpp,fppPrivate,-3pcd",
    "The strict ETP features should be set correctly to the default branch"
  );
  is(
    Services.prefs.getBoolPref("privacy.trackingprotection.enabled"),
    false,
    "The tracking protection pref has been set correctly"
  );
  is(
    Services.prefs.getBoolPref("privacy.trackingprotection.enabled"),
    false,
    "The tracking protection PBM pref has been set correctly"
  );
  is(
    Services.prefs.getBoolPref(
      "network.cookie.cookieBehavior.optInPartitioning"
    ),
    false,
    "The 3pcd pref has been set correctly"
  );
  is(
    Services.prefs.getIntPref("network.cookie.cookieBehavior"),
    Ci.nsICookieService.BEHAVIOR_ACCEPT,
    "The cookieBehavior pref has been set correctly"
  );
  is(
    Services.prefs.getIntPref("network.cookie.cookieBehavior.pbmode"),
    Ci.nsICookieService.BEHAVIOR_ACCEPT,
    "The cookieBehavior PBM pref has been set correctly"
  );

  info("Ensure we still remain in strict mode.");
  is(
    Services.prefs.getCharPref("browser.contentblocking.category"),
    "strict",
    "The ETP category should remain strict"
  );

  doExperimentCleanup();
});
