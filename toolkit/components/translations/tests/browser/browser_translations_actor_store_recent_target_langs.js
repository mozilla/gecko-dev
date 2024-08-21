/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests storing a language tag to the mostRecentTargetLanguages pref,
 * asserting that the pref value matches what is expected both before and
 * after inserting the new language tag.
 *
 * @param {string} message - A description of the test case to log to info.
 * @param {object} config
 * @param {string} config.langTag - The BCP-47 language tag to insert into the pref.
 * @param {string} config.expectedBefore - The expected value of the pref before the insertion.
 * @param {string} config.expectedAfter - The expected value of the pref after the insertion.
 */
function testStoreMostRecentTargetLanguage(
  message,
  { langTag, expectedBefore, expectedAfter }
) {
  info(message);

  const actualBefore = Services.prefs.getCharPref(
    "browser.translations.mostRecentTargetLanguages"
  );
  is(
    actualBefore,
    expectedBefore,
    `The browser.translations.mostRecentTargetLanguages pref should match the expected value before storing "${langTag}"`
  );

  TranslationsParent.storeMostRecentTargetLanguage(langTag);

  const actualAfter = Services.prefs.getCharPref(
    "browser.translations.mostRecentTargetLanguages"
  );
  is(
    actualAfter,
    expectedAfter,
    `The browser.translations.mostRecentTargetLanguages pref should match the expected value after storing "${langTag}"`
  );
}

/**
 * Test cases for storing the most recent target language tag into the mostRecentTargetLanguages pref.
 */
add_task(async function test_store_recent_target_languages() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.translations.enable", true],
      ["browser.translations.mostRecentTargetLanguages", ""],
    ],
  });

  testStoreMostRecentTargetLanguage(
    "Insert langTag into pref with no langTags",
    {
      langTag: "es",
      expectedBefore: "",
      expectedAfter: "es",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert same langTag into pref with single langTag",
    {
      langTag: "es",
      expectedBefore: "es",
      expectedAfter: "es",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert new langTag into pref with single langTag",
    {
      langTag: "fr",
      expectedBefore: "es",
      expectedAfter: "es,fr",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert same final langTag into pref with two langTags",
    {
      langTag: "fr",
      expectedBefore: "es,fr",
      expectedAfter: "es,fr",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert same initial langTag into pref with two langTags",
    {
      langTag: "es",
      expectedBefore: "es,fr",
      expectedAfter: "fr,es",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert new langTag into pref with two langTags",
    {
      langTag: "uk",
      expectedBefore: "fr,es",
      expectedAfter: "fr,es,uk",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert same final langTag into pref with three langTags",
    {
      langTag: "uk",
      expectedBefore: "fr,es,uk",
      expectedAfter: "fr,es,uk",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert same middle langTag into pref with three langTags",
    {
      langTag: "es",
      expectedBefore: "fr,es,uk",
      expectedAfter: "fr,uk,es",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert same initial langTag into pref with three langTags",
    {
      langTag: "fr",
      expectedBefore: "fr,uk,es",
      expectedAfter: "uk,es,fr",
    }
  );

  testStoreMostRecentTargetLanguage(
    "Insert new langTag into pref with three langTags",
    {
      langTag: "de",
      expectedBefore: "uk,es,fr",
      expectedAfter: "es,fr,de",
    }
  );

  await SpecialPowers.popPrefEnv();
});
