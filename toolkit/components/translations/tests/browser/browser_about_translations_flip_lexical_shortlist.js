/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that flipping the useLexicalShortlist pref creates a new translator.
 */
add_task(async function test_about_translations_language_swap() {
  const { runInPage, cleanup, resolveDownloads } = await openAboutTranslations({
    languagePairs: [{ fromLang: "en", toLang: "fr" }],
    prefs: [["browser.translations.useLexicalShortlist", false]],
  });

  info("Triggering a translation from English to French.");
  await runInPage(async ({ selectors }) => {
    const { document } = content;

    await ContentTaskUtils.waitForCondition(
      () => {
        return document.body.hasAttribute("ready");
      },
      "Waiting for the document to be ready.",
      100,
      200
    );

    /** @type {HTMLSelectElement} */
    const fromSelect = document.querySelector(selectors.fromLanguageSelect);
    /** @type {HTMLSelectElement} */
    const toSelect = document.querySelector(selectors.toLanguageSelect);
    /** @type {HTMLTextAreaElement} */
    const translationFrom = document.querySelector(
      selectors.translationTextarea
    );
    /** @type {HTMLDivElement} */
    const translationResultsPlaceholder = document.querySelector(
      selectors.translationResultsPlaceholder
    );

    fromSelect.value = "en";
    fromSelect.dispatchEvent(new Event("input"));

    translationFrom.value = "Text to translate.";
    translationFrom.dispatchEvent(new Event("input"));

    toSelect.value = "fr";
    toSelect.dispatchEvent(new Event("input"));

    await ContentTaskUtils.waitForCondition(
      () => translationResultsPlaceholder.innerText === "Translating…",
      "Showing translating text",
      100,
      200
    );
  });

  info("Waiting for model files to download.");
  await resolveDownloads(1);

  await runInPage(async ({ selectors }) => {
    const { document } = content;

    /** @type {HTMLDivElement} */
    const translationTo = document.querySelector(selectors.translationResult);

    await ContentTaskUtils.waitForCondition(
      () => translationTo.innerText === "TEXT TO TRANSLATE. [en to fr]",
      "translation from fr to en is complete",
      100,
      200
    );

    is(translationTo.innerText, "TEXT TO TRANSLATE. [en to fr]");
  });

  info('Flipping "browser.translations.useLexicalShortlist" to true.');
  await waitForTranslationsPrefChanged(() => {
    Services.prefs.setBoolPref(
      "browser.translations.useLexicalShortlist",
      true
    );
  });

  info("Ensuring re-translation is triggered.");
  await runInPage(async ({ selectors }) => {
    const { document } = content;

    /** @type {HTMLDivElement} */
    const translationResultsPlaceholder = document.querySelector(
      selectors.translationResultsPlaceholder
    );

    await ContentTaskUtils.waitForCondition(
      () => translationResultsPlaceholder.innerText === "Translating…",
      "Showing translating text",
      100,
      200
    );
  });

  info("Waiting for model files to download.");
  await resolveDownloads(1);

  info("Ensuring text is translated.");
  await runInPage(async ({ selectors }) => {
    const { document } = content;

    /** @type {HTMLDivElement} */
    const translationTo = document.querySelector(selectors.translationResult);

    await ContentTaskUtils.waitForCondition(
      () => translationTo.innerText === "TEXT TO TRANSLATE. [en to fr]",
      "translation from fr to en is complete",
      100,
      200
    );

    is(translationTo.innerText, "TEXT TO TRANSLATE. [en to fr]");
  });

  info('Flipping "browser.translations.useLexicalShortlist" to false.');
  await waitForTranslationsPrefChanged(() => {
    Services.prefs.setBoolPref(
      "browser.translations.useLexicalShortlist",
      false
    );
  });

  info("Ensuring re-translation is triggered.");
  await runInPage(async ({ selectors }) => {
    const { document } = content;

    /** @type {HTMLDivElement} */
    const translationResultsPlaceholder = document.querySelector(
      selectors.translationResultsPlaceholder
    );

    await ContentTaskUtils.waitForCondition(
      () => translationResultsPlaceholder.innerText === "Translating…",
      "Showing translating text",
      100,
      200
    );
  });

  info("Waiting for model files to download.");
  await resolveDownloads(1);

  info("Ensuring text is translated.");
  await runInPage(async ({ selectors }) => {
    const { document } = content;

    /** @type {HTMLDivElement} */
    const translationTo = document.querySelector(selectors.translationResult);

    await ContentTaskUtils.waitForCondition(
      () => translationTo.innerText === "TEXT TO TRANSLATE. [en to fr]",
      "translation from fr to en is complete",
      100,
      200
    );

    is(translationTo.innerText, "TEXT TO TRANSLATE. [en to fr]");
  });

  await cleanup();
});
