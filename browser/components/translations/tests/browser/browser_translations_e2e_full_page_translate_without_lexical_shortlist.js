/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that Full Page Translations work from Spanish to English work when
 * loading pre-downloaded model and WASM artifacts from the file system with
 * the useLexicalShortlist pref turned off.
 */
add_task(
  async function test_full_page_translate_end_to_end_without_lexical_shortlist() {
    const { cleanup, runInPage } = await loadTestPage({
      endToEndTest: true,
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.useLexicalShortlist", false]],
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

    await FullPageTranslationsTestUtils.clickTranslateButton();
    await FullPageTranslationsTestUtils.assertPageIsTranslated({
      endToEndTest: true,
      fromLanguage: "es",
      toLanguage: "en",
      runInPage,
    });

    await cleanup();
  }
);
