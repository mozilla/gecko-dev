/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test case tests translations in the FullPageTranslationsPanel with the
 * useLexicalShortlist pref initially set to false. It then toggles it to true,
 * and then back to false, ensuring that the correct models are downloaded, and
 * that the translations succeed with each flip of the pref.
 */
add_task(
  async function test_full_page_translations_panel_lexical_shortlist_starting_false() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.useLexicalShortlist", false]],
    });

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await FullPageTranslationsTestUtils.openPanel({
      expectedFromLanguage: "es",
      expectedToLanguage: "en",
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      downloadHandler: resolveDownloads,
    });
    await FullPageTranslationsTestUtils.assertPageIsTranslated({
      fromLanguage: "es",
      toLanguage: "en",
      runInPage,
    });

    await waitForTranslationsPrefChanged(() => {
      Services.prefs.setBoolPref(
        "browser.translations.useLexicalShortlist",
        true
      );
    });
    await FullPageTranslationsTestUtils.openPanel({
      expectedToLanguage: "en",
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
    });
    await FullPageTranslationsTestUtils.changeSelectedToLanguage({
      langTag: "fr",
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      pivotTranslation: true,
      downloadHandler: resolveDownloads,
    });
    await FullPageTranslationsTestUtils.assertPageIsTranslated({
      fromLanguage: "es",
      toLanguage: "fr",
      runInPage,
    });

    await waitForTranslationsPrefChanged(() => {
      Services.prefs.setBoolPref(
        "browser.translations.useLexicalShortlist",
        false
      );
    });
    await FullPageTranslationsTestUtils.openPanel({
      expectedToLanguage: "en",
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
    });
    await FullPageTranslationsTestUtils.changeSelectedToLanguage({
      langTag: "uk",
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      pivotTranslation: true,
      downloadHandler: resolveDownloads,
    });
    await FullPageTranslationsTestUtils.assertPageIsTranslated({
      fromLanguage: "es",
      toLanguage: "uk",
      runInPage,
    });

    await cleanup();
  }
);

/**
 * This test case tests translations in the FullPageTranslationsPanel with the
 * useLexicalShortlist pref initially set to true. It then toggles it to false,
 * and then back to true, ensuring that the correct models are downloaded, and
 * that the translations succeed with each flip of the pref.
 */
add_task(
  async function test_full_page_translations_panel_lexical_shortlist_starting_true() {
    const { cleanup, resolveDownloads, runInPage } = await loadTestPage({
      page: SPANISH_PAGE_URL,
      languagePairs: LANGUAGE_PAIRS,
      prefs: [["browser.translations.useLexicalShortlist", true]],
    });

    await FullPageTranslationsTestUtils.assertPageIsUntranslated(runInPage);

    await FullPageTranslationsTestUtils.openPanel({
      expectedFromLanguage: "es",
      expectedToLanguage: "en",
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewDefault,
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      downloadHandler: resolveDownloads,
    });
    await FullPageTranslationsTestUtils.assertPageIsTranslated({
      fromLanguage: "es",
      toLanguage: "en",
      runInPage,
    });

    await waitForTranslationsPrefChanged(() => {
      Services.prefs.setBoolPref(
        "browser.translations.useLexicalShortlist",
        false
      );
    });
    await FullPageTranslationsTestUtils.openPanel({
      expectedToLanguage: "en",
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
    });
    await FullPageTranslationsTestUtils.changeSelectedToLanguage({
      langTag: "fr",
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      pivotTranslation: true,
      downloadHandler: resolveDownloads,
    });
    await FullPageTranslationsTestUtils.assertPageIsTranslated({
      fromLanguage: "es",
      toLanguage: "fr",
      runInPage,
    });

    await waitForTranslationsPrefChanged(() => {
      Services.prefs.setBoolPref(
        "browser.translations.useLexicalShortlist",
        true
      );
    });
    await FullPageTranslationsTestUtils.openPanel({
      expectedToLanguage: "en",
      onOpenPanel: FullPageTranslationsTestUtils.assertPanelViewRevisit,
    });
    await FullPageTranslationsTestUtils.changeSelectedToLanguage({
      langTag: "uk",
    });

    await FullPageTranslationsTestUtils.clickTranslateButton({
      pivotTranslation: true,
      downloadHandler: resolveDownloads,
    });
    await FullPageTranslationsTestUtils.assertPageIsTranslated({
      fromLanguage: "es",
      toLanguage: "uk",
      runInPage,
    });

    await cleanup();
  }
);
