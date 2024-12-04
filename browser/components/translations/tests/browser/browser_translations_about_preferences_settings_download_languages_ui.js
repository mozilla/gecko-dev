/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_translations_settings_download_languages() {
  const {
    cleanup,
    remoteClients,
    elements: { settingsButton },
  } = await setupAboutPreferences(LANGUAGE_PAIRS, {
    prefs: [["browser.translations.newSettingsUI.enable", true]],
  });

  const frenchModels = [
    "lex.50.50.enfr.s2t.bin",
    "lex.50.50.fren.s2t.bin",
    "model.enfr.intgemm.alphas.bin",
    "model.fren.intgemm.alphas.bin",
    "vocab.enfr.spm",
    "vocab.fren.spm",
  ];

  const allModels = [
    "lex.50.50.enes.s2t.bin",
    "lex.50.50.enfr.s2t.bin",
    "lex.50.50.enuk.s2t.bin",
    "lex.50.50.esen.s2t.bin",
    "lex.50.50.fren.s2t.bin",
    "lex.50.50.uken.s2t.bin",
    "model.enes.intgemm.alphas.bin",
    "model.enfr.intgemm.alphas.bin",
    "model.enuk.intgemm.alphas.bin",
    "model.esen.intgemm.alphas.bin",
    "model.fren.intgemm.alphas.bin",
    "model.uken.intgemm.alphas.bin",
    "vocab.enes.spm",
    "vocab.enfr.spm",
    "vocab.enuk.spm",
    "vocab.esen.spm",
    "vocab.fren.spm",
    "vocab.uken.spm",
  ];

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: { settingsButton },
  });

  info(
    "Open translations settings page by clicking on translations settings button."
  );
  const { downloadLanguageList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  info("Test French language model install and uninstall function.");

  let langFr = Array.from(downloadLanguageList.querySelectorAll("label")).find(
    el => el.getAttribute("value") === "fr"
  );

  let clickButton = BrowserTestUtils.waitForEvent(
    langFr.parentNode.querySelector("moz-button"),
    "click"
  );
  langFr.parentNode.querySelector("moz-button").click();
  await clickButton;

  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      frenchModels.length
    ),
    frenchModels,
    "French models were downloaded."
  );

  await TranslationsSettingsTestUtils.downaloadButtonClick(
    langFr,
    "translations-settings-remove-icon",
    "Delete icon is visible on French button."
  );

  langFr.parentNode.querySelector("moz-button").click();
  await clickButton;

  await TranslationsSettingsTestUtils.downaloadButtonClick(
    langFr,
    "translations-settings-download-icon",
    "Download icon is visible on French Button."
  );

  info("Test 'All language' models install and uninstall function");

  // Download "All languages" is the first child
  let langAll = downloadLanguageList.children[0];

  let clickButtonAll = BrowserTestUtils.waitForEvent(
    langAll.querySelector("moz-button"),
    "click"
  );
  langAll.querySelector("moz-button").click();
  await clickButtonAll;

  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      allModels.length
    ),
    allModels,
    "All models were downloaded."
  );
  Assert.deepEqual(
    await remoteClients.translationsWasm.resolvePendingDownloads(1),
    ["bergamot-translator"],
    "Wasm was downloaded."
  );

  await TranslationsSettingsTestUtils.downaloadButtonClick(
    langAll,
    "translations-settings-remove-icon",
    "Delete icon is visible on 'All languages' button"
  );

  langAll.querySelector("moz-button").click();
  await clickButton;

  await TranslationsSettingsTestUtils.downaloadButtonClick(
    langAll,
    "translations-settings-download-icon",
    "Download icon is visible on 'All Language' button."
  );

  await cleanup();
});
