/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * The engine inside of Gecko has some mitigation to clean text for certain behavior
 * that happens in Bergamot. The full engine needs to be run to exercise this code.
 */
add_task(async function test_text_cleaning() {
  await autoTranslatePage({
    page: TEXT_CLEANING_URL,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async TranslationsTest => {
      const selectors = TranslationsTest.getSelectors();

      await TranslationsTest.assertTranslationResult(
        "Whitespace before and after is preserved in the translation.",
        selectors.getTextCleaningWhitespace,
        [
          " EL [es to en, html] ",
          "",
          "       PERRO",
          "       GRANDE Y [es to en, html]",
          "    ",
          " ROJO [es to en, html] ",
        ].join("\n")
      );

      await TranslationsTest.assertTranslationResult(
        "Soft hyphens are stripped from the translation.",
        selectors.getTextCleaningSoftHyphens,
        "LONG WORDS SOMETIMES HAVE SOFT-HYPHENS [es to en, html]"
      );
    },
  });
});
