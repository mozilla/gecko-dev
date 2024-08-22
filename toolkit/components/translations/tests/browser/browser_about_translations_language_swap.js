/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test the language swap behaviour.
 */
add_task(async function test_about_translations_language_swap() {
  const { runInPage, cleanup, resolveDownloads } = await openAboutTranslations({
    languagePairs: [
      { fromLang: "en", toLang: "fr" },
      { fromLang: "en", toLang: "it" },
      { fromLang: "fr", toLang: "en" },
    ],
    autoDownloadFromRemoteSettings: false,
  });

  await runInPage(async ({ selectors }) => {
    const { document, window } = content;
    Cu.waiveXrays(window).DEBOUNCE_DELAY = 5; // Make the timer run faster for tests.

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
    /** @type {HTMLButtonElement} */
    const swapButton = document.querySelector(selectors.languageSwapButton);

    // default option -> default option
    is(fromSelect.value, "detect");
    is(toSelect.value, "");
    is(swapButton.disabled, true, "The language swap button is disabled"); // disabled because from-language is equivalent to to-language

    fromSelect.value = "en";
    fromSelect.dispatchEvent(new Event("input"));

    await ContentTaskUtils.waitForCondition(
      () => fromSelect.value === "en",
      "en selected in fromSelect",
      100,
      200
    );

    // en -> default option
    is(fromSelect.value, "en");
    is(toSelect.value, "");
    is(swapButton.disabled, false, "The language swap button is enabled");

    translationFrom.value = "Translation text number 1.";
    translationFrom.dispatchEvent(new Event("input"));

    // default option (detect: en) -> default option
    await ContentTaskUtils.waitForCondition(
      () => swapButton.disabled === true,
      "The language swap button is disabled",
      100,
      200
    );
    is(swapButton.disabled, true, "The language swap button is disabled"); // disabled because swapping would wipe the input

    translationFrom.value = "";
    translationFrom.dispatchEvent(new Event("input"));

    await ContentTaskUtils.waitForCondition(
      () => swapButton.disabled === false,
      "The language swap button is enabled",
      100,
      200
    ); // re-enabled after the input is cleared

    swapButton.dispatchEvent(new Event("click"));

    // default option -> en
    is(fromSelect.value, "detect");
    is(toSelect.value, "en");
    await ContentTaskUtils.waitForCondition(
      () => swapButton.disabled === false,
      "The language swap button is enabled",
      100,
      200
    );
    is(swapButton.disabled, false, "The language swap button is enabled");

    translationFrom.value = "Translation text number 1.";
    translationFrom.dispatchEvent(new Event("input"));

    // default option (detect: en) -> en
    await ContentTaskUtils.waitForCondition(
      () => swapButton.disabled === true,
      "The language swap button is disabled",
      100,
      200
    );
    is(swapButton.disabled, true, "The language swap button is disabled"); // disabled because from-language (detected as en) is equivalent to to-language

    fromSelect.value = "fr";
    fromSelect.dispatchEvent(new Event("input"));

    await ContentTaskUtils.waitForCondition(
      () => fromSelect.value === "fr",
      "fr selected in fromSelect",
      100,
      200
    );

    translationFrom.value = "";
    translationFrom.dispatchEvent(new Event("input"));

    await ContentTaskUtils.waitForCondition(
      () => swapButton.disabled === false,
      "The language swap button is enabled",
      100,
      200
    );
    is(swapButton.disabled, false, "The language swap button is enabled"); // both sides are empty
  });

  await resolveDownloads(1); // fr -> en

  await runInPage(async ({ selectors }) => {
    const { document, window } = content;
    Cu.waiveXrays(window).DEBOUNCE_DELAY = 5;

    /** @type {HTMLSelectElement} */
    const fromSelect = document.querySelector(selectors.fromLanguageSelect);
    /** @type {HTMLSelectElement} */
    const toSelect = document.querySelector(selectors.toLanguageSelect);
    /** @type {HTMLTextAreaElement} */
    const translationFrom = document.querySelector(
      selectors.translationTextarea
    );
    /** @type {HTMLDivElement} */
    const translationTo = document.querySelector(selectors.translationResult);
    /** @type {HTMLButtonElement} */
    const swapButton = document.querySelector(selectors.languageSwapButton);

    translationFrom.value = "Translation text number 1.";
    translationFrom.dispatchEvent(new Event("input"));

    await ContentTaskUtils.waitForCondition(
      () => translationTo.innerText === "TRANSLATION TEXT NUMBER 1. [fr to en]",
      "translation from fr to en is complete",
      100,
      200
    );

    // fr -> en
    is(fromSelect.value, "fr");
    is(toSelect.value, "en");
    is(translationFrom.value, "Translation text number 1.");
    is(translationTo.innerText, "TRANSLATION TEXT NUMBER 1. [fr to en]");
    is(swapButton.disabled, false, "The language swap button is enabled");

    swapButton.dispatchEvent(new Event("click"));

    await ContentTaskUtils.waitForCondition(
      () => swapButton.disabled === true,
      "The language swap button is disabled",
      100,
      200
    );
    is(swapButton.disabled, true, "The language swap button is disabled"); // after the swap, the input is not empty while the output is
  });

  await resolveDownloads(1); // en -> fr

  await runInPage(async ({ selectors }) => {
    const { document, window } = content;
    Cu.waiveXrays(window).DEBOUNCE_DELAY = 5;

    /** @type {HTMLSelectElement} */
    const fromSelect = document.querySelector(selectors.fromLanguageSelect);
    /** @type {HTMLSelectElement} */
    const toSelect = document.querySelector(selectors.toLanguageSelect);
    /** @type {HTMLTextAreaElement} */
    const translationFrom = document.querySelector(
      selectors.translationTextarea
    );
    /** @type {HTMLDivElement} */
    const translationTo = document.querySelector(selectors.translationResult);
    /** @type {HTMLButtonElement} */
    const swapButton = document.querySelector(selectors.languageSwapButton);
    await ContentTaskUtils.waitForCondition(
      () =>
        translationTo.innerText ===
        "TRANSLATION TEXT NUMBER 1. [FR TO EN] [en to fr]",
      "translation from en to fr is complete",
      100,
      200
    );

    // en -> fr
    is(fromSelect.value, "en");
    is(toSelect.value, "fr");
    is(translationFrom.value, "TRANSLATION TEXT NUMBER 1. [fr to en]");
    is(
      translationTo.innerText,
      "TRANSLATION TEXT NUMBER 1. [FR TO EN] [en to fr]"
    ); // translating the original fr-to-en translation back to fr
    is(swapButton.disabled, false, "The language swap button is enabled");

    toSelect.value = "it";
    toSelect.dispatchEvent(new Event("input"));

    await ContentTaskUtils.waitForCondition(
      () => toSelect.value === "it",
      "it selected in toSelect",
      100,
      200
    );

    is(swapButton.disabled, true, "The language swap button is disabled"); // after the toLanguage change, the input is not empty while the output is
  });

  await resolveDownloads(1); // en -> it

  await runInPage(async ({ selectors }) => {
    const { document, window } = content;
    Cu.waiveXrays(window).DEBOUNCE_DELAY = 5;

    /** @type {HTMLSelectElement} */
    const fromSelect = document.querySelector(selectors.fromLanguageSelect);
    /** @type {HTMLSelectElement} */
    const toSelect = document.querySelector(selectors.toLanguageSelect);
    /** @type {HTMLTextAreaElement} */
    const translationFrom = document.querySelector(
      selectors.translationTextarea
    );
    /** @type {HTMLDivElement} */
    const translationTo = document.querySelector(selectors.translationResult);
    /** @type {HTMLButtonElement} */
    const swapButton = document.querySelector(selectors.languageSwapButton);

    await ContentTaskUtils.waitForCondition(
      () =>
        translationTo.innerText ===
        "TRANSLATION TEXT NUMBER 1. [FR TO EN] [en to it]",
      "translation from en to it is complete",
      100,
      200
    );

    // en -> it
    is(fromSelect.value, "en");
    is(toSelect.value, "it");
    is(translationFrom.value, "TRANSLATION TEXT NUMBER 1. [fr to en]");
    is(
      translationTo.innerText,
      "TRANSLATION TEXT NUMBER 1. [FR TO EN] [en to it]"
    );
    is(swapButton.disabled, true, "The language swap button is disabled"); // disabled because to-language (it) is not a valid from-language
  });

  await cleanup();
});
