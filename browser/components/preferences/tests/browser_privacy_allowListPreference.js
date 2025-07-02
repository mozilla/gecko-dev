/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

async function cleanUp() {
  await SpecialPowers.popPrefEnv();
  gBrowser.removeCurrentTab();
}

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

add_task(async function test_standard_mode_no_checkboxes_and_prefs_true() {
  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  doc.getElementById(ETP_STANDARD_ID).click();
  is_element_hidden(
    doc.getElementById(STRICT_BASELINE_CHECKBOX_ID),
    "Strict mode's baseline checkbox should be hidden when ETP Standard is selected."
  );
  is_element_hidden(
    doc.getElementById(STRICT_CONVENIENCE_CHECKBOX_ID),
    "Strict mode's convenience checkbox should be hidden when ETP Standard is selected."
  );
  is_element_hidden(
    doc.getElementById(CUSTOM_BASELINE_CHECKBOX_ID),
    "Custom mode's baseline checkbox should be hidden when ETP Standard is selected."
  );
  is_element_hidden(
    doc.getElementById(CUSTOM_CONVENIENCE_CHECKBOX_ID),
    "Custom mode's convenience checkbox should be hidden when ETP Standard is selected."
  );
  Assert.ok(
    Services.prefs.getBoolPref(BASELINE_PREF),
    "The baseline preference should be true in ETP Standard mode."
  );
  Assert.ok(
    Services.prefs.getBoolPref(CONVENIENCE_PREF),
    "The convenience preference should be true in ETP Standard mode."
  );
  await cleanUp();
});

add_task(async function test_strict_mode_checkboxes_and_default_prefs() {
  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  doc.getElementById(ETP_STRICT_ID).click();
  let baselineCheckbox = doc.getElementById(STRICT_BASELINE_CHECKBOX_ID);
  let convenienceCheckbox = doc.getElementById(STRICT_CONVENIENCE_CHECKBOX_ID);
  is_element_visible(
    baselineCheckbox,
    "The baseline checkbox should be visible in ETP Strict mode."
  );
  is_element_visible(
    convenienceCheckbox,
    "The convenience checkbox should be visible in ETP Strict mode."
  );
  is(
    baselineCheckbox.checked,
    true,
    "The baseline checkbox should be checked by default in ETP Strict mode."
  );
  is(
    convenienceCheckbox.checked,
    false,
    "The convenience checkbox should be unchecked by default in ETP Strict mode."
  );
  Assert.ok(
    Services.prefs.getBoolPref(BASELINE_PREF),
    "The baseline preference should be true by default in ETP Strict mode."
  );
  Assert.ok(
    !Services.prefs.getBoolPref(CONVENIENCE_PREF),
    "The convenience preference should be false by default in ETP Strict mode."
  );
  await cleanUp();
});

add_task(
  async function test_strict_mode_convenience_false_when_baseline_false() {
    await setup();
    await openPreferencesViaOpenPreferencesAPI("privacy", {
      leaveOpen: true,
    });
    let doc = gBrowser.contentDocument;
    doc.getElementById(ETP_STRICT_ID).click();

    await clickCheckboxAndWaitForPrefChange(
      doc,
      STRICT_BASELINE_CHECKBOX_ID,
      BASELINE_PREF,
      false
    );

    Assert.ok(
      !Services.prefs.getBoolPref(BASELINE_PREF),
      "The baseline pref should be false after unchecking its checkbox in ETP Strict mode."
    );
    Assert.ok(
      !Services.prefs.getBoolPref(CONVENIENCE_PREF),
      "The convenience pref should remain false when the baseline checkbox is unchecked in ETP Strict mode."
    );
    await cleanUp();
  }
);

add_task(async function test_custom_mode_checkboxes_and_ids() {
  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  await doc.getElementById(ETP_CUSTOM_ID).click();
  let baselineCheckbox = doc.getElementById(CUSTOM_BASELINE_CHECKBOX_ID);
  let convenienceCheckbox = doc.getElementById(CUSTOM_CONVENIENCE_CHECKBOX_ID);
  is_element_visible(
    baselineCheckbox,
    "The baseline checkbox should be visible in ETP Custom mode."
  );
  is_element_visible(
    convenienceCheckbox,
    "The convenience checkbox should be visible in ETP Custom mode."
  );
  await cleanUp();
});

add_task(async function test_custom_mode_inherits_last_mode() {
  await setup();
  // Test switching from Standard to Custom
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  doc.getElementById(ETP_STANDARD_ID).click();
  doc.getElementById(ETP_CUSTOM_ID).click();
  let baselineCheckbox = doc.getElementById(CUSTOM_BASELINE_CHECKBOX_ID);
  let convenienceCheckbox = doc.getElementById(CUSTOM_CONVENIENCE_CHECKBOX_ID);
  is(
    baselineCheckbox.checked,
    true,
    "Custom's baseline checkbox should be checked, inheriting state from Standard mode."
  );
  is(
    convenienceCheckbox.checked,
    true,
    "Custom's convenience checkbox should be checked, inheriting state from Standard mode."
  );
  await SpecialPowers.popPrefEnv();
  gBrowser.removeCurrentTab();

  // Test switching from Strict (with baseline unchecked) to Custom
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  doc = gBrowser.contentDocument;
  doc.getElementById(ETP_STRICT_ID).click();
  let convenienceCheckboxStrict = doc.getElementById(
    STRICT_CONVENIENCE_CHECKBOX_ID
  );
  is_element_visible(
    convenienceCheckboxStrict,
    "The convenience checkbox in Strict mode must be visible for this test."
  );
  await clickCheckboxAndWaitForPrefChange(
    doc,
    STRICT_BASELINE_CHECKBOX_ID,
    BASELINE_PREF,
    false
  );
  doc.getElementById(ETP_CUSTOM_ID).click();
  let baselineCheckboxCustom = doc.getElementById(CUSTOM_BASELINE_CHECKBOX_ID);
  let convenienceCheckboxCustom = doc.getElementById(
    CUSTOM_CONVENIENCE_CHECKBOX_ID
  );
  is(
    baselineCheckboxCustom.checked,
    false,
    "Custom's baseline checkbox should be unchecked, inheriting state from a modified Strict mode."
  );
  is(
    convenienceCheckboxCustom.checked,
    false,
    "Custom's convenience checkbox should be unchecked, inheriting state from a modified Strict mode."
  );
  await cleanUp();
});

add_task(async function test_checkbox_state_persists_after_reload() {
  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  doc.getElementById(ETP_STRICT_ID).click();
  await clickCheckboxAndWaitForPrefChange(
    doc,
    STRICT_BASELINE_CHECKBOX_ID,
    BASELINE_PREF,
    false
  );
  is(
    doc.getElementById(STRICT_BASELINE_CHECKBOX_ID).checked,
    false,
    "The Strict mode baseline checkbox should be unchecked before reload."
  );
  doc.defaultView.location.reload();
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  doc = gBrowser.contentDocument;
  let baselineCheckboxAfter = doc.getElementById(STRICT_BASELINE_CHECKBOX_ID);
  is(
    baselineCheckboxAfter.checked,
    false,
    "The Strict mode baseline checkbox should remain unchecked after page reload."
  );
  await cleanUp();
});

add_task(async function test_switching_modes_resets_checkboxes() {
  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  let standardRadio = doc.getElementById(ETP_STANDARD_ID);
  standardRadio.click();
  doc.getElementById(ETP_STRICT_ID).click();
  await clickCheckboxAndWaitForPrefChange(
    doc,
    STRICT_BASELINE_CHECKBOX_ID,
    BASELINE_PREF,
    false
  );
  is(
    doc.getElementById(STRICT_BASELINE_CHECKBOX_ID).checked,
    false,
    "The Strict mode baseline checkbox should be in its modified, unchecked state."
  );
  standardRadio.click();
  doc.getElementById(ETP_STRICT_ID).click();
  let baselineCheckbox = doc.getElementById(STRICT_BASELINE_CHECKBOX_ID);
  let convenienceCheckbox = doc.getElementById(STRICT_CONVENIENCE_CHECKBOX_ID);
  is(
    baselineCheckbox.checked,
    true,
    "The baseline checkbox should reset to its default checked state after toggling ETP modes."
  );
  is(
    convenienceCheckbox.checked,
    false,
    "The convenience checkbox should reset to its default unchecked state after toggling ETP modes."
  );
  await cleanUp();
});

add_task(async function test_convenience_cannot_be_enabled_if_baseline_false() {
  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  doc.getElementById(ETP_STRICT_ID).click();
  let convenienceCheckbox = doc.getElementById(STRICT_CONVENIENCE_CHECKBOX_ID);

  is(
    convenienceCheckbox.parentDisabled,
    false,
    "The convenience checkbox should be enabled when the baseline checkbox is checked."
  );

  await clickCheckboxAndWaitForPrefChange(
    doc,
    STRICT_BASELINE_CHECKBOX_ID,
    BASELINE_PREF,
    false
  );

  is(
    convenienceCheckbox.parentDisabled,
    true,
    "The convenience checkbox should be disabled when the baseline checkbox is unchecked."
  );

  convenienceCheckbox.click();

  is(
    convenienceCheckbox.checked,
    false,
    "Clicking the disabled convenience checkbox should not change its state."
  );
  await cleanUp();
});

add_task(async function test_prefs_update_when_toggling_checkboxes_in_custom() {
  await setup();
  await openPreferencesViaOpenPreferencesAPI("privacy", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;
  doc.getElementById(ETP_CUSTOM_ID).click();
  let baselineCheckbox = doc.getElementById(CUSTOM_BASELINE_CHECKBOX_ID);
  let convenienceCheckbox = doc.getElementById(CUSTOM_CONVENIENCE_CHECKBOX_ID);

  // Uncheck both checkboxes if they are checked, to establish a baseline.
  if (baselineCheckbox.checked) {
    await clickCheckboxAndWaitForPrefChange(
      doc,
      CUSTOM_BASELINE_CHECKBOX_ID,
      BASELINE_PREF,
      false
    );
  }
  if (convenienceCheckbox.checked) {
    await clickCheckboxAndWaitForPrefChange(
      doc,
      CUSTOM_CONVENIENCE_CHECKBOX_ID,
      CONVENIENCE_PREF,
      false
    );
  }

  Assert.ok(
    !Services.prefs.getBoolPref(BASELINE_PREF),
    "The baseline pref should be false after being unchecked in Custom mode."
  );
  Assert.ok(
    !Services.prefs.getBoolPref(CONVENIENCE_PREF),
    "The convenience pref should be false after being unchecked in Custom mode."
  );

  // Check both boxes and verify the preferences are updated.
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
  Assert.ok(
    Services.prefs.getBoolPref(BASELINE_PREF),
    "The baseline pref should be true after being checked in Custom mode."
  );
  Assert.ok(
    Services.prefs.getBoolPref(CONVENIENCE_PREF),
    "The convenience pref should be true after being checked in Custom mode."
  );
  await cleanUp();
});
