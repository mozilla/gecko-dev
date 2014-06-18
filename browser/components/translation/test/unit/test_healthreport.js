/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Metrics.jsm", this);
Cu.import("resource:///modules/translation/Translation.jsm", this);
Cu.import("resource://testing-common/services/healthreport/utils.jsm", this);

// At end of test, restore original state.
const ORIGINAL_TELEMETRY_ENABLED = Services.prefs.getBoolPref("toolkit.telemetry.enabled");

function run_test() {
  run_next_test();
}

add_test(function setup() {
  do_get_profile();
  Services.prefs.setBoolPref("toolkit.telemetry.enabled", true);

  run_next_test();
});

do_register_cleanup(function() {
  Services.prefs.setBoolPref("toolkit.telemetry.enabled",
                             ORIGINAL_TELEMETRY_ENABLED);
});

add_task(function test_constructor() {
  let provider = new TranslationProvider();
});

// Provider can initialize and de-initialize properly.
add_task(function* test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new TranslationProvider();
  yield provider.init(storage);
  yield provider.shutdown();
  yield storage.close();
});

// Test recording translation opportunities.
add_task(function* test_translation_opportunity() {
  let storage = yield Metrics.Storage("opportunity");
  let provider = new TranslationProvider();
  yield provider.init(storage);

  // Initially nothing should be configured.
  let now = new Date();
  let m = provider.getMeasurement("translation", 1);
  let values = yield m.getValues();
  Assert.equal(values.days.size, 0);
  Assert.ok(!values.days.hasDay(now));

  // Record an opportunity.
  yield provider.recordTranslationOpportunity("fr", now);

  values = yield m.getValues();
  Assert.equal(values.days.size, 1);
  Assert.ok(values.days.hasDay(now));
  let day = values.days.getDay(now);
  Assert.ok(day.has("translationOpportunityCount"));
  Assert.equal(day.get("translationOpportunityCount"), 1);

  Assert.ok(day.has("translationOpportunityCountsByLanguage"));
  let countsByLanguage = JSON.parse(day.get("translationOpportunityCountsByLanguage"));
  Assert.equal(countsByLanguage["fr"], 1);

  // Record more opportunities.
  yield provider.recordTranslationOpportunity("fr", now);
  yield provider.recordTranslationOpportunity("fr", now);
  yield provider.recordTranslationOpportunity("es", now);

  values = yield m.getValues();
  let day = values.days.getDay(now);
  Assert.ok(day.has("translationOpportunityCount"));
  Assert.equal(day.get("translationOpportunityCount"), 4);

  Assert.ok(day.has("translationOpportunityCountsByLanguage"));
  countsByLanguage = JSON.parse(day.get("translationOpportunityCountsByLanguage"));
  Assert.equal(countsByLanguage["fr"], 3);
  Assert.equal(countsByLanguage["es"], 1);

  yield provider.shutdown();
  yield storage.close();
});

// Test recording a translation.
add_task(function* test_record_translation() {
  let storage = yield Metrics.Storage("translation");
  let provider = new TranslationProvider();
  yield provider.init(storage);
  let now = new Date();

  // Record a translation.
  yield provider.recordTranslation("fr", "es", 1000, now);

  let m = provider.getMeasurement("translation", 1);
  let values = yield m.getValues();
  Assert.equal(values.days.size, 1);
  Assert.ok(values.days.hasDay(now));
  let day = values.days.getDay(now);
  Assert.ok(day.has("pageTranslatedCount"));
  Assert.equal(day.get("pageTranslatedCount"), 1);
  Assert.ok(day.has("charactersTranslatedCount"));
  Assert.equal(day.get("charactersTranslatedCount"), 1000);

  Assert.ok(day.has("pageTranslatedCountsByLanguage"));
  let countsByLanguage = JSON.parse(day.get("pageTranslatedCountsByLanguage"));
  Assert.ok("fr" in countsByLanguage);
  Assert.equal(countsByLanguage["fr"]["total"], 1);
  Assert.equal(countsByLanguage["fr"]["es"], 1);

  // Record more translations.
  yield provider.recordTranslation("fr", "es", 1, now);
  yield provider.recordTranslation("fr", "en", 2, now);
  yield provider.recordTranslation("es", "en", 4, now);

  values = yield m.getValues();
  let day = values.days.getDay(now);
  Assert.ok(day.has("pageTranslatedCount"));
  Assert.equal(day.get("pageTranslatedCount"), 4);
  Assert.ok(day.has("charactersTranslatedCount"));
  Assert.equal(day.get("charactersTranslatedCount"), 1007);

  Assert.ok(day.has("pageTranslatedCountsByLanguage"));
  let countsByLanguage = JSON.parse(day.get("pageTranslatedCountsByLanguage"));
  Assert.ok("fr" in countsByLanguage);
  Assert.equal(countsByLanguage["fr"]["total"], 3);
  Assert.equal(countsByLanguage["fr"]["es"], 2);
  Assert.equal(countsByLanguage["fr"]["en"], 1);
  Assert.ok("es" in countsByLanguage);
  Assert.equal(countsByLanguage["es"]["total"], 1);
  Assert.equal(countsByLanguage["es"]["en"], 1);

  yield provider.shutdown();
  yield storage.close();
});

// Test recording changing languages.
add_task(function* test_record_translation() {
  let storage = yield Metrics.Storage("translation");
  let provider = new TranslationProvider();
  yield provider.init(storage);
  let now = new Date();

  // Record a language change before translation.
  yield provider.recordLanguageChange(true);

  // Record two language changes after translation.
  yield provider.recordLanguageChange(false);
  yield provider.recordLanguageChange(false);


  let m = provider.getMeasurement("translation", 1);
  let values = yield m.getValues();
  Assert.equal(values.days.size, 1);
  Assert.ok(values.days.hasDay(now));
  let day = values.days.getDay(now);

  Assert.ok(day.has("detectedLanguageChangedBefore"));
  Assert.equal(day.get("detectedLanguageChangedBefore"), 1);
  Assert.ok(day.has("detectedLanguageChangedAfter"));
  Assert.equal(day.get("detectedLanguageChangedAfter"), 2);

  yield provider.shutdown();
  yield storage.close();
});

add_task(function* test_collect_daily() {
  let storage = yield Metrics.Storage("translation");
  let provider = new TranslationProvider();
  yield provider.init(storage);
  let now = new Date();

  // Set the prefs we test here to `false` initially.
  const kPrefDetectLanguage = "browser.translation.detectLanguage";
  const kPrefShowUI = "browser.translation.ui.show";
  Services.prefs.setBoolPref(kPrefDetectLanguage, false);
  Services.prefs.setBoolPref(kPrefShowUI, false);

  // Initially nothing should be configured.
  yield provider.collectDailyData();

  let m = provider.getMeasurement("translation", 1);
  let values = yield m.getValues();
  Assert.equal(values.days.size, 1);
  Assert.ok(values.days.hasDay(now));
  let day = values.days.getDay(now);
  Assert.ok(day.has("detectLanguageEnabled"));
  Assert.ok(day.has("showTranslationUI"));

  // Changes to the repective prefs should be picked up.
  Services.prefs.setBoolPref(kPrefDetectLanguage, true);
  Services.prefs.setBoolPref(kPrefShowUI, true);

  yield provider.collectDailyData();

  values = yield m.getValues();
  day = values.days.getDay(now);
  Assert.equal(day.get("detectLanguageEnabled"), 1);
  Assert.equal(day.get("showTranslationUI"), 1);

  yield provider.shutdown();
  yield storage.close();
});

// Test the payload after recording with telemetry enabled.
add_task(function* test_healthreporter_json() {
  Services.prefs.setBoolPref("toolkit.telemetry.enabled", true);

  let reporter = yield getHealthReporter("healthreporter_json");
  yield reporter.init();
  try {
    let now = new Date();
    let provider = new TranslationProvider();
    yield reporter._providerManager.registerProvider(provider);

    yield provider.recordTranslationOpportunity("fr", now);
    yield provider.recordLanguageChange(true);
    yield provider.recordTranslation("fr", "en", 1000, now);
    yield provider.recordLanguageChange(false);

    yield provider.recordTranslationOpportunity("es", now);
    yield provider.recordTranslation("es", "en", 1000, now);

    yield reporter.collectMeasurements();
    let payload = yield reporter.getJSONPayload(true);
    let today = reporter._formatDate(now);

    Assert.ok(today in payload.data.days);
    let day = payload.data.days[today];

    Assert.ok("org.mozilla.translation.translation" in day);

    let translations = day["org.mozilla.translation.translation"];

    Assert.equal(translations["translationOpportunityCount"], 2);
    Assert.equal(translations["pageTranslatedCount"], 2);
    Assert.equal(translations["charactersTranslatedCount"], 2000);

    Assert.ok("translationOpportunityCountsByLanguage" in translations);
    Assert.equal(translations["translationOpportunityCountsByLanguage"]["fr"], 1);
    Assert.equal(translations["translationOpportunityCountsByLanguage"]["es"], 1);

    Assert.ok("pageTranslatedCountsByLanguage" in translations);
    Assert.ok("fr" in translations["pageTranslatedCountsByLanguage"]);
    Assert.equal(translations["pageTranslatedCountsByLanguage"]["fr"]["total"], 1);
    Assert.equal(translations["pageTranslatedCountsByLanguage"]["fr"]["en"], 1);

    Assert.ok("es" in translations["pageTranslatedCountsByLanguage"]);
    Assert.equal(translations["pageTranslatedCountsByLanguage"]["es"]["total"], 1);
    Assert.equal(translations["pageTranslatedCountsByLanguage"]["es"]["en"], 1);

    Assert.ok("detectedLanguageChangedBefore" in translations);
    Assert.equal(translations["detectedLanguageChangedBefore"], 1);
    Assert.ok("detectedLanguageChangedAfter" in translations);
    Assert.equal(translations["detectedLanguageChangedAfter"], 1);
  } finally {
    reporter._shutdown();
  }
});

// Test the payload after recording with telemetry disabled.
add_task(function* test_healthreporter_json2() {
  Services.prefs.setBoolPref("toolkit.telemetry.enabled", false);

  let reporter = yield getHealthReporter("healthreporter_json");
  yield reporter.init();
  try {
    let now = new Date();
    let provider = new TranslationProvider();
    yield reporter._providerManager.registerProvider(provider);

    yield provider.recordTranslationOpportunity("fr", now);
    yield provider.recordLanguageChange(true);
    yield provider.recordTranslation("fr", "en", 1000, now);
    yield provider.recordLanguageChange(false);

    yield provider.recordTranslationOpportunity("es", now);
    yield provider.recordTranslation("es", "en", 1000, now);

    yield reporter.collectMeasurements();
    let payload = yield reporter.getJSONPayload(true);
    let today = reporter._formatDate(now);

    Assert.ok(today in payload.data.days);
    let day = payload.data.days[today];

    Assert.ok("org.mozilla.translation.translation" in day);

    let translations = day["org.mozilla.translation.translation"];

    Assert.ok(!("translationOpportunityCount" in translations));
    Assert.ok(!("pageTranslatedCount" in translations));
    Assert.ok(!("charactersTranslatedCount" in translations));
    Assert.ok(!("translationOpportunityCountsByLanguage" in translations));
    Assert.ok(!("pageTranslatedCountsByLanguage" in translations));
    Assert.ok(!("detectedLanguageChangedBefore" in translations));
    Assert.ok(!("detectedLanguageChangedAfter" in translations));
  } finally {
    reporter._shutdown();
  }
});
