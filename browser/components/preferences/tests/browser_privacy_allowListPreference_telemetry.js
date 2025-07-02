/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

const BASELINE_PREF = "privacy.trackingprotection.allow_list.baseline.enabled";
const CONVENIENCE_PREF =
  "privacy.trackingprotection.allow_list.convenience.enabled";
const CB_CATEGORY_PREF = "browser.contentblocking.category";

const ETP_STANDARD_ID = "standardRadio";
const ETP_STRICT_ID = "strictRadio";
const ETP_CUSTOM_ID = "customRadio";

const STRICT_BASELINE_CHECKBOX_ID = "contentBlockingBaselineExceptionsStrict";
const STRICT_CONVENIENCE_CHECKBOX_ID =
  "contentBlockingConvenienceExceptionsStrict";
const CUSTOM_BASELINE_CHECKBOX_ID = "contentBlockingBaselineExceptionsCustom";
const CUSTOM_CONVENIENCE_CHECKBOX_ID =
  "contentBlockingConvenienceExceptionsCustom";

async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [[CB_CATEGORY_PREF, "standard"]],
  });
  Assert.ok(
    Services.prefs.getBoolPref(BASELINE_PREF),
    "The baseline preference should be initially true."
  );
  Assert.ok(
    Services.prefs.getBoolPref(CONVENIENCE_PREF),
    "The convenience preference should be initially true."
  );
}

add_task(async function test_baseline_telemetry() {
  const exceptionListServiceObserver = Cc[
    "@mozilla.org/url-classifier/exception-list-service;1"
  ].getService(Ci.nsIObserver);

  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  let value = Glean.contentblocking.tpAllowlistBaselineEnabled.testGetValue();
  Assert.equal(
    value,
    true,
    "Baseline telemetry should be true when ETP standard mode is selected"
  );

  doc.getElementById(ETP_STRICT_ID).click();

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  value = Glean.contentblocking.tpAllowlistBaselineEnabled.testGetValue();
  Assert.equal(
    value,
    true,
    "Baseline telemetry should be true when ETP strict mode is selected"
  );

  await clickCheckboxAndWaitForPrefChange(
    doc,
    STRICT_BASELINE_CHECKBOX_ID,
    BASELINE_PREF,
    false
  );

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  value = Glean.contentblocking.tpAllowlistBaselineEnabled.testGetValue();
  Assert.equal(
    value,
    false,
    "Baseline telemetry should be false when strict baseline checkbox is unchecked"
  );

  doc.getElementById(ETP_CUSTOM_ID).click();

  value = Glean.contentblocking.tpAllowlistBaselineEnabled.testGetValue();
  Assert.equal(
    value,
    false,
    "Baseline telemetry should be false when switching to custom mode after unchecking strict baseline"
  );

  await clickCheckboxAndWaitForPrefChange(
    doc,
    CUSTOM_BASELINE_CHECKBOX_ID,
    BASELINE_PREF,
    true
  );

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  value = Glean.contentblocking.tpAllowlistBaselineEnabled.testGetValue();
  Assert.equal(
    value,
    true,
    "Baseline telemetry should be true when custom baseline checkbox is checked"
  );

  Services.fog.testResetFOG();
  await SpecialPowers.popPrefEnv();
  gBrowser.removeCurrentTab();
});

add_task(async function test_convenience_telemetry() {
  const exceptionListServiceObserver = Cc[
    "@mozilla.org/url-classifier/exception-list-service;1"
  ].getService(Ci.nsIObserver);
  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });

  let doc = gBrowser.contentDocument;

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  let value =
    Glean.contentblocking.tpAllowlistConvenienceEnabled.testGetValue();
  Assert.equal(
    value,
    true,
    "Convenience telemetry should be true by default when ETP standard mode is selected"
  );

  await doc.getElementById(ETP_STRICT_ID).click();

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  value = Glean.contentblocking.tpAllowlistConvenienceEnabled.testGetValue();
  Assert.equal(
    value,
    false,
    "Convenience telemetry should be false by default when ETP strict mode is selected"
  );

  await clickCheckboxAndWaitForPrefChange(
    doc,
    STRICT_CONVENIENCE_CHECKBOX_ID,
    CONVENIENCE_PREF,
    true
  );

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  value = Glean.contentblocking.tpAllowlistConvenienceEnabled.testGetValue();
  Assert.equal(
    value,
    true,
    "Convenience telemetry should be true when strict convenience checkbox is checked"
  );

  await clickCheckboxAndWaitForPrefChange(
    doc,
    STRICT_BASELINE_CHECKBOX_ID,
    BASELINE_PREF,
    false
  );

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  value = Glean.contentblocking.tpAllowlistConvenienceEnabled.testGetValue();
  Assert.equal(
    value,
    false,
    "Convenience telemetry be false when strict baseline checkbox is unchecked"
  );

  doc.getElementById(ETP_CUSTOM_ID).click();

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  value = Glean.contentblocking.tpAllowlistConvenienceEnabled.testGetValue();
  Assert.equal(
    value,
    false,
    "Convenience telemetry should remain false when switching to custom mode after unchecking strict convenience"
  );

  await clickCheckboxAndWaitForPrefChange(
    doc,
    CUSTOM_BASELINE_CHECKBOX_ID,
    BASELINE_PREF,
    true
  );

  await clickCheckboxAndWaitForPrefChange(
    doc,
    CUSTOM_CONVENIENCE_CHECKBOX_ID,
    CONVENIENCE_PREF,
    true
  );

  info("Sending idle-daily");
  exceptionListServiceObserver.observe(null, "idle-daily", null);
  info("Sent idle-daily");

  value = Glean.contentblocking.tpAllowlistConvenienceEnabled.testGetValue();
  Assert.equal(
    value,
    true,
    "Convenience telemetry should be true when custom convenience checkbox is checked"
  );

  Services.fog.testResetFOG();
  await SpecialPowers.popPrefEnv();
  gBrowser.removeCurrentTab();
});
