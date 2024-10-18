/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that Full Page Translations work from Spanish to English work when
 * loading pre-downloaded model and WASM artifacts from the file system.
 */
add_task(async function test_full_page_translate_end_to_end() {
  const { cleanup, runInPage } = await loadTestPage({
    endToEndTest: true,
    page: SPANISH_PAGE_URL,
    languagePairs: LANGUAGE_PAIRS,
  });

  await FullPageTranslationsTestUtils.assertTranslationsButton(
    { button: true, circleArrows: false, locale: false, icon: true },
    "The button is available."
  );

  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await FullPageTranslationsTestUtils.openPanel({
    expectedFromLanguage: "es",
    expectedToLanguage: "en",
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
  });

  info("Ensure that the fr-en model is not available in end-to-end tests.");
  await FullPageTranslationsTestUtils.changeSelectedFromLanguage({
    langTag: "fr",
  });
  await FullPageTranslationsTestUtils.clickTranslateButton({
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewError,
  });

  info("Ensure that the es-en model is available in end-to-end tests.");
  await FullPageTranslationsTestUtils.changeSelectedFromLanguage({
    langTag: "es",
  });
  await FullPageTranslationsTestUtils.clickTranslateButton();

  await FullPageTranslationsTestUtils.assertPageIsTranslated({
    endToEndTest: true,
    fromLanguage: "es",
    toLanguage: "en",
    runInPage,
  });

  await FullPageTranslationsTestUtils.openPanel({
    expectedToLanguage: "en",
    onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
  });

  await FullPageTranslationsTestUtils.clickRestoreButton();
  await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

  await cleanup();
});
