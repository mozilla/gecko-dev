/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_translations_settings_pane_elements() {
  const {
    cleanup,
    elements: { settingsButton },
  } = await setupAboutPreferences(LANGUAGE_PAIRS, {
    prefs: [["browser.translations.newSettingsUI.enable", true]],
  });

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: { settingsButton },
  });

  const {
    backButton,
    header,
    translationsSettingsDescription,
    translateAlwaysHeader,
    translateNeverHeader,
    translateAlwaysMenuList,
    translateNeverMenuList,
    translateNeverSiteHeader,
    translateNeverSiteDesc,
    translateDownloadLanguagesHeader,
    translateDownloadLanguagesLearnMore,
  } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  assertVisibility({
    message: "Expect paneTranslations elements to be visible.",
    visible: {
      backButton,
      header,
      translationsSettingsDescription,
      translateAlwaysHeader,
      translateNeverHeader,
      translateAlwaysMenuList,
      translateNeverMenuList,
      translateNeverSiteHeader,
      translateNeverSiteDesc,
      translateDownloadLanguagesHeader,
      translateDownloadLanguagesLearnMore,
    },
    hidden: {
      settingsButton,
    },
  });

  const paneEvent = BrowserTestUtils.waitForEvent(
    document,
    "paneshown",
    false,
    event => event.detail.category === "paneGeneral"
  );

  click(backButton);
  await paneEvent;

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: {
      settingsButton,
    },
    hidden: {
      backButton,
      header,
      translationsSettingsDescription,
      translateAlwaysHeader,
      translateNeverHeader,
      translateAlwaysMenuList,
      translateNeverMenuList,
      translateNeverSiteHeader,
      translateNeverSiteDesc,
      translateDownloadLanguagesHeader,
      translateDownloadLanguagesLearnMore,
    },
  });
  await cleanup();
});

add_task(async function test_translations_settings_always_translate() {
  const {
    cleanup,
    elements: { settingsButton },
  } = await setupAboutPreferences(LANGUAGE_PAIRS, {
    prefs: [["browser.translations.newSettingsUI.enable", true]],
  });

  const document = gBrowser.selectedBrowser.contentDocument;

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: { settingsButton },
  });

  const { translateAlwaysMenuList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  let alwaysTranslateSection = document.getElementById(
    "translations-settings-always-translate-section"
  );
  await testLanguageList(alwaysTranslateSection, translateAlwaysMenuList);
  await testLanguageListWithPref(alwaysTranslateSection);

  await cleanup();
});

add_task(async function test_translations_settings_never_translate() {
  const {
    cleanup,
    elements: { settingsButton },
  } = await setupAboutPreferences(LANGUAGE_PAIRS, {
    prefs: [["browser.translations.newSettingsUI.enable", true]],
  });

  const document = gBrowser.selectedBrowser.contentDocument;
  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: { settingsButton },
  });

  const { translateNeverMenuList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  let neverTranslateSection = document.getElementById(
    "translations-settings-never-translate-section"
  );
  await testLanguageList(neverTranslateSection, translateNeverMenuList);
  await testLanguageListWithPref(neverTranslateSection);
  await cleanup();
});
function getLangsFromPref(pref) {
  let rawLangs = Services.prefs.getCharPref(pref);
  if (!rawLangs) {
    return [];
  }
  let langArr = rawLangs.split(",");
  return langArr;
}

async function testLanguageList(translateSection, menuList) {
  const { sectionName, pref } =
    translateSection.id === "translations-settings-always-translate-section"
      ? { sectionName: "Always", pref: ALWAYS_TRANSLATE_LANGS_PREF }
      : { sectionName: "Never", pref: NEVER_TRANSLATE_LANGS_PREF };

  is(
    translateSection.querySelector(".translations-settings-languages-card"),
    null,
    `Language list not present in ${sectionName} Translate list`
  );

  const menuOptions = menuList.querySelector("menupopup").children;
  for (const option of menuOptions) {
    let clickMenu = BrowserTestUtils.waitForEvent(option, "command");
    option.doCommand();
    await clickMenu;

    /** Languages are always added on the top, so check the firstChild
     * for newly added languages.
     * the firstChild.querySelector("label").innerText is the language display name
     * which is compared with the menulist display name that is selected
     */
    let langElem = translateSection.querySelector(
      ".translations-settings-language-list"
    ).firstChild;
    const displayName = getIntlDisplayName(option.value);
    is(
      langElem.querySelector("label").innerText,
      displayName,
      `Language list has element ${displayName}`
    );

    const langTag = langElem.querySelector("label").getAttribute("value");
    ok(
      getLangsFromPref(pref).includes(langTag),
      `Perferences contains ${langTag}`
    );
  }
  /** The test cases has 4 languages, so check if 4 languages are added to the list */
  let langNum = translateSection.querySelector(
    ".translations-settings-language-list"
  ).childElementCount;
  is(langNum, 4, "Number of languages added is 4");

  const languagelist = translateSection.querySelector(
    ".translations-settings-language-list"
  );

  for (let i = 0; i < langNum; i++) {
    // Delete the first language in the list
    let langElem = languagelist.children[0];
    let langName = langElem.querySelector("label").innerText;
    const langTag = langElem.querySelector("label").getAttribute("value");
    let langButton = langElem.querySelector("moz-button");
    let clickButton = BrowserTestUtils.waitForEvent(langButton, "click");
    langButton.click();
    await clickButton;

    ok(
      !getLangsFromPref(pref).includes(langTag),
      `Perferences does not contain ${langTag}`
    );

    if (i < langNum - 1) {
      is(
        languagelist.childElementCount,
        langNum - i - 1,
        `${langName} removed from ${sectionName}  Translate`
      );
    } else {
      /** Check if the language list card is removed after removing the last language */
      is(
        translateSection.querySelector(".translations-settings-languages-card"),
        null,
        `${langName} removed from ${sectionName} Translate`
      );
    }
  }
}

async function testLanguageListWithPref(translateSection) {
  const langs = [
    "fr",
    "de",
    "en",
    "es",
    "fr,de",
    "fr,en",
    "fr,es",
    "de,en",
    "de,en,es",
    "es,fr,en",
    "en,es,fr,de",
  ];
  const { sectionName, pref } =
    translateSection.id === "translations-settings-always-translate-section"
      ? { sectionName: "Always", pref: ALWAYS_TRANSLATE_LANGS_PREF }
      : { sectionName: "Never", pref: NEVER_TRANSLATE_LANGS_PREF };

  is(
    translateSection.querySelector(".translations-settings-languages-card"),
    null,
    `Language list not present in ${sectionName} Translate list`
  );

  for (const langOptions of langs) {
    Services.prefs.setCharPref(pref, langOptions);

    /** Languages are always added on the top, so check the firstChild
     * for newly added languages.
     * the firstChild.querySelector("label").innerText is the language display name
     * which is compared with the menulist display name that is selected
     */

    const langsAdded = langOptions.split(",");
    is(
      translateSection.querySelector(".translations-settings-language-list")
        .childElementCount,
      langsAdded.length,
      `Language list has ${langsAdded.length} elements `
    );

    let langsAddedHtml = Array.from(
      translateSection
        .querySelector(".translations-settings-language-list")
        .querySelectorAll("label")
    );

    for (const lang of langsAdded) {
      const langFind = langsAddedHtml
        .find(el => el.getAttribute("value") === lang)
        .getAttribute("value");
      is(langFind, lang, `Language list has element ${lang}`);
    }
  }

  Services.prefs.setCharPref(pref, "");
  is(
    translateSection.querySelector(".translations-settings-languages-card"),
    null,
    `All removed from ${sectionName} Translate`
  );
}

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

  const { translateDownloadLanguagesList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  let langList = translateDownloadLanguagesList.querySelector(
    ".translations-settings-language-list"
  );

  // Test French language model install and uninstall function

  let langFr = Array.from(langList.querySelectorAll("label")).find(
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

  if (
    !langFr.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon")
  ) {
    await BrowserTestUtils.waitForMutationCondition(
      langFr.parentNode.querySelector("moz-button"),
      { attributes: true, attributeFilter: ["class"] },
      () =>
        langFr.parentNode
          .querySelector("moz-button")
          .classList.contains("translations-settings-remove-icon")
    );
  }

  ok(
    langFr.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon"),
    "Delete icon is visible"
  );

  langFr.parentNode.querySelector("moz-button").click();
  await clickButton;

  if (
    !langFr.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-download-icon")
  ) {
    await BrowserTestUtils.waitForMutationCondition(
      langFr.parentNode.querySelector("moz-button"),
      { attributes: true, attributeFilter: ["class"] },
      () =>
        langFr.parentNode
          .querySelector("moz-button")
          .classList.contains("translations-settings-download-icon")
    );
  }
  ok(
    langFr.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-download-icon"),
    "Download icon is visible"
  );

  // Test "All language" models install and uninstall function
  // Download "All languages" is the first child
  let langAll = langList.children[0];

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

  if (
    !langAll
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon")
  ) {
    await BrowserTestUtils.waitForMutationCondition(
      langAll.querySelector("moz-button"),
      { attributes: true, attributeFilter: ["class"] },
      () =>
        langAll
          .querySelector("moz-button")
          .classList.contains("translations-settings-remove-icon")
    );
  }

  ok(
    langAll
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon"),
    "Delete icon is visible"
  );

  langAll.querySelector("moz-button").click();
  await clickButton;

  if (
    !langAll
      .querySelector("moz-button")
      .classList.contains("translations-settings-download-icon")
  ) {
    await BrowserTestUtils.waitForMutationCondition(
      langAll.querySelector("moz-button"),
      { attributes: true, attributeFilter: ["class"] },
      () =>
        langAll
          .querySelector("moz-button")
          .classList.contains("translations-settings-download-icon")
    );
  }
  ok(
    langAll
      .querySelector("moz-button")
      .classList.contains("translations-settings-download-icon"),
    "Download icon is visible"
  );

  await cleanup();
});

add_task(
  async function test_translations_settings_download_languages_error_handling() {
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

    const spainishModels = [
      "lex.50.50.enes.s2t.bin",
      "lex.50.50.esen.s2t.bin",
      "model.enes.intgemm.alphas.bin",
      "model.esen.intgemm.alphas.bin",
      "vocab.enes.spm",
      "vocab.esen.spm",
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

    const { translateDownloadLanguagesList } =
      await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
        settingsButton
      );

    let langList = translateDownloadLanguagesList.querySelector(
      ".translations-settings-language-list"
    );

    info("Test French language model for download error");

    let langFr = Array.from(langList.querySelectorAll("label")).find(
      el => el.getAttribute("value") === "fr"
    );

    let clickButton = BrowserTestUtils.waitForEvent(
      langFr.parentNode.querySelector("moz-button"),
      "click"
    );
    langFr.parentNode.querySelector("moz-button").click();
    await clickButton;

    await captureTranslationsError(() =>
      remoteClients.translationModels.rejectPendingDownloads(
        frenchModels.length
      )
    );

    const errorElement = gBrowser.selectedBrowser.contentDocument.querySelector(
      ".translations-settings-language-error"
    );

    assertVisibility({
      message: "Moz-message-bar with error message is visible",
      visible: { errorElement },
    });
    is(
      document.l10n.getAttributes(errorElement).id,
      "translations-settings-language-download-error",
      "Error message correctly shows download error"
    );
    is(
      document.l10n.getAttributes(errorElement).args.name,
      "French",
      "Error message correctly shows download error for French language"
    );

    if (
      !langFr.parentNode
        .querySelector("moz-button")
        .classList.contains("translations-settings-download-icon")
    ) {
      await BrowserTestUtils.waitForMutationCondition(
        langFr.parentNode.querySelector("moz-button"),
        { attributes: true, attributeFilter: ["class"] },
        () =>
          langFr.parentNode
            .querySelector("moz-button")
            .classList.contains("translations-settings-download-icon")
      );
    }
    ok(
      langFr.parentNode
        .querySelector("moz-button")
        .classList.contains("translations-settings-download-icon"),
      "Download icon is visible"
    );

    remoteClients.translationsWasm.assertNoNewDownloads();

    info("Download Spanish language model successfully.");

    let langEs = Array.from(langList.querySelectorAll("label")).find(
      el => el.getAttribute("value") === "es"
    );

    clickButton = BrowserTestUtils.waitForEvent(
      langEs.parentNode.querySelector("moz-button"),
      "click"
    );
    langEs.parentNode.querySelector("moz-button").click();
    await clickButton;

    const errorElementEs =
      gBrowser.selectedBrowser.contentDocument.querySelector(
        ".translations-settings-language-error"
      );

    ok(
      !errorElementEs,
      "Previous error is remove when new action occured, i.e. click download Spanish button"
    );

    Assert.deepEqual(
      await remoteClients.translationModels.resolvePendingDownloads(
        spainishModels.length
      ),
      spainishModels,
      "Spanish models were downloaded."
    );

    if (
      !langEs.parentNode
        .querySelector("moz-button")
        .classList.contains("translations-settings-remove-icon")
    ) {
      await BrowserTestUtils.waitForMutationCondition(
        langEs.parentNode.querySelector("moz-button"),
        { attributes: true, attributeFilter: ["class"] },
        () =>
          langEs.parentNode
            .querySelector("moz-button")
            .classList.contains("translations-settings-remove-icon")
      );
    }

    ok(
      langEs.parentNode
        .querySelector("moz-button")
        .classList.contains("translations-settings-remove-icon"),
      "Delete icon is visible for Spanish language hence downloaded"
    );

    info("Test All language models download error");
    // Download "All languages" is the first child
    let langAll = langList.children[0];

    let clickButtonAll = BrowserTestUtils.waitForEvent(
      langAll.querySelector("moz-button"),
      "click"
    );
    langAll.querySelector("moz-button").click();
    await clickButtonAll;

    await captureTranslationsError(() =>
      remoteClients.translationModels.rejectPendingDownloads(allModels.length)
    );

    await captureTranslationsError(() =>
      remoteClients.translationsWasm.rejectPendingDownloads(allModels.length)
    );

    remoteClients.translationsWasm.assertNoNewDownloads();

    if (
      !langAll
        .querySelector("moz-button")
        .classList.contains("translations-settings-download-icon")
    ) {
      await BrowserTestUtils.waitForMutationCondition(
        langAll.querySelector("moz-button"),
        { attributes: true, attributeFilter: ["class"] },
        () =>
          langAll
            .querySelector("moz-button")
            .classList.contains("translations-settings-download-icon")
      );
    }
    langAll.querySelector("moz-button").classList;
    ok(
      langAll
        .querySelector("moz-button")
        .classList.contains("translations-settings-download-icon"),
      "Download icon is visible for 'all languages'"
    );

    const errorElementAll =
      gBrowser.selectedBrowser.contentDocument.querySelector(
        ".translations-settings-language-error"
      );

    assertVisibility({
      message: "Moz-message-bar with error message is visible",
      visible: { errorElementAll },
    });
    is(
      document.l10n.getAttributes(errorElementAll).id,
      "translations-settings-language-download-error",
      "Error message correctly shows download error"
    );
    is(
      document.l10n.getAttributes(errorElementAll).args.name,
      "all",
      "Error message correctly shows download error for all language"
    );

    await cleanup();
  }
);

add_task(async function test_translations_settings_download_languages_all() {
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

  const spainishModels = [
    "lex.50.50.enes.s2t.bin",
    "lex.50.50.esen.s2t.bin",
    "model.enes.intgemm.alphas.bin",
    "model.esen.intgemm.alphas.bin",
    "vocab.enes.spm",
    "vocab.esen.spm",
  ];

  const ukrainianModels = [
    "lex.50.50.enuk.s2t.bin",
    "lex.50.50.uken.s2t.bin",
    "model.enuk.intgemm.alphas.bin",
    "model.uken.intgemm.alphas.bin",
    "vocab.enuk.spm",
    "vocab.uken.spm",
  ];

  assertVisibility({
    message: "Expect paneGeneral elements to be visible.",
    visible: { settingsButton },
  });

  const { translateDownloadLanguagesList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  let langList = translateDownloadLanguagesList.querySelector(
    ".translations-settings-language-list"
  );

  info(
    "Install each language French, Spanish and Ukrainian and check if All language state changes to 'all language downloaded' by changing the all language button icon to 'remove icon'"
  );

  info("Download French language model.");
  let langFr = Array.from(langList.querySelectorAll("label")).find(
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

  if (
    !langFr.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon")
  ) {
    await BrowserTestUtils.waitForMutationCondition(
      langFr.parentNode.querySelector("moz-button"),
      { attributes: true, attributeFilter: ["class"] },
      () =>
        langFr.parentNode
          .querySelector("moz-button")
          .classList.contains("translations-settings-remove-icon")
    );
  }

  ok(
    langFr.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon"),
    "Delete icon is visible for French language hence downloaded"
  );

  info("Download Spanish language model.");

  let langEs = Array.from(langList.querySelectorAll("label")).find(
    el => el.getAttribute("value") === "es"
  );

  clickButton = BrowserTestUtils.waitForEvent(
    langEs.parentNode.querySelector("moz-button"),
    "click"
  );
  langEs.parentNode.querySelector("moz-button").click();
  await clickButton;

  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      spainishModels.length
    ),
    spainishModels,
    "Spanish models were downloaded."
  );

  if (
    !langEs.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon")
  ) {
    await BrowserTestUtils.waitForMutationCondition(
      langEs.parentNode.querySelector("moz-button"),
      { attributes: true, attributeFilter: ["class"] },
      () =>
        langEs.parentNode
          .querySelector("moz-button")
          .classList.contains("translations-settings-remove-icon")
    );
  }

  ok(
    langEs.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon"),
    "Delete icon is visible for Spanish language hence downloaded"
  );

  info("Download Ukrainian language model.");

  let langUk = Array.from(langList.querySelectorAll("label")).find(
    el => el.getAttribute("value") === "uk"
  );

  clickButton = BrowserTestUtils.waitForEvent(
    langUk.parentNode.querySelector("moz-button"),
    "click"
  );
  langUk.parentNode.querySelector("moz-button").click();
  await clickButton;

  Assert.deepEqual(
    await remoteClients.translationModels.resolvePendingDownloads(
      ukrainianModels.length
    ),
    ukrainianModels,
    "Ukrainian models were downloaded."
  );

  if (
    !langUk.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon")
  ) {
    await BrowserTestUtils.waitForMutationCondition(
      langUk.parentNode.querySelector("moz-button"),
      { attributes: true, attributeFilter: ["class"] },
      () =>
        langUk.parentNode
          .querySelector("moz-button")
          .classList.contains("translations-settings-remove-icon")
    );
  }

  ok(
    langUk.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon"),
    "Delete icon is visible for Ukranian language hence downloaded."
  );

  // Download "All languages" is the first child
  let langAll = langList.children[0];

  ok(
    langAll
      .querySelector("moz-button")
      .classList.contains("translations-settings-remove-icon"),
    "Delete icon is visible for All Languages after all individual language models were downloaded."
  );

  info(
    "Remove one language ensure that All Languages change state changes to 'removed' to indicate that all languages are not downloaded."
  );

  info("Remove Spanish language model.");
  langEs.parentNode.querySelector("moz-button").click();
  await clickButton;

  if (
    !langEs.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-download-icon")
  ) {
    await BrowserTestUtils.waitForMutationCondition(
      langEs.parentNode.querySelector("moz-button"),
      { attributes: true, attributeFilter: ["class"] },
      () =>
        langEs.parentNode
          .querySelector("moz-button")
          .classList.contains("translations-settings-download-icon")
    );
  }
  ok(
    langEs.parentNode
      .querySelector("moz-button")
      .classList.contains("translations-settings-download-icon"),
    "Download icon is visible for Spanish language hence removed"
  );

  ok(
    langAll
      .querySelector("moz-button")
      .classList.contains("translations-settings-download-icon"),
    "Download icon is visible for all languages i.e. all languages are not downloaded since one language, Spainish was removed."
  );

  await cleanup();
});

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

add_task(async function test_translations_settings_never_translate_site() {
  const {
    cleanup,
    elements: { settingsButton },
  } = await setupAboutPreferences(LANGUAGE_PAIRS, {
    prefs: [["browser.translations.newSettingsUI.enable", true]],
  });

  const document = gBrowser.selectedBrowser.contentDocument;

  // const openTranslationsSettings =
  await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
    settingsButton
  );

  let neverTranslateSitesSection = document.getElementById(
    "translations-settings-never-sites-section"
  );
  let siteList = neverTranslateSitesSection.querySelector(
    ".translations-settings-language-list"
  );

  info("Ensuring the list of never-translate sites is empty");
  is(
    getNeverTranslateSitesFromPerms().length,
    0,
    "The list of never-translate sites is empty"
  );

  is(siteList, null, "The never-translate sites html list is empty");

  info("Adding sites to the neverTranslateSites perms");
  await PermissionTestUtils.add(
    "https://example.com",
    TRANSLATIONS_PERMISSION,
    Services.perms.DENY_ACTION
  );
  await PermissionTestUtils.add(
    "https://example.org",
    TRANSLATIONS_PERMISSION,
    Services.perms.DENY_ACTION
  );
  await PermissionTestUtils.add(
    "https://example.net",
    TRANSLATIONS_PERMISSION,
    Services.perms.DENY_ACTION
  );

  is(
    getNeverTranslateSitesFromPerms().length,
    3,
    "The list of never-translate sites has 3 elements"
  );

  siteList = neverTranslateSitesSection.querySelector(
    ".translations-settings-language-list"
  );

  is(
    siteList.childElementCount,
    3,
    "The never-translate sites html list has 3 elements"
  );

  const permissionsUrls = [
    "https://example.com",
    "https://example.org",
    "https://example.net",
  ];

  const siteNum = siteList.children.length;
  for (let i = siteNum; i > 0; i--) {
    is(
      siteList.children[i - 1].querySelector("label").textContent,
      permissionsUrls[permissionsUrls.length - i],
      `Never translate URL ${
        permissionsUrls[permissionsUrls.length - i]
      } is added`
    );
  }

  for (let i = 0; i < siteNum; i++) {
    // Delete the first site in the list
    let siteElem = siteList.children[0];
    // Delete the first language in the list
    let siteName = siteElem.querySelector("label").innerText;
    let siteButton = siteElem.querySelector("moz-button");

    ok(
      siteList.querySelector(`label[value="${siteName}"]`),
      `Site ${siteName} present in the Never transalate site list`
    );

    ok(
      getNeverTranslateSitesFromPerms().find(p => p.origin === siteName),
      `Site ${siteName} present in the Never transalate site permissions list`
    );

    let clickButton = BrowserTestUtils.waitForEvent(siteButton, "click");
    siteButton.click();
    await clickButton;

    ok(
      !siteList.querySelector(`label[value="${siteName}"]`),
      `Site ${siteName} removed successfully from the Never transalate site list`
    );

    ok(
      !getNeverTranslateSitesFromPerms().find(p => p.origin === siteName),
      `Site ${siteName} removed from successfully from the Never transalate site permissions list`
    );

    if (i < siteNum - 1) {
      is(
        siteList.childElementCount,
        siteNum - i - 1,
        `${siteName} removed from Never Translate Site`
      );
    } else {
      /** Check if the language list card is removed after removing the last language */
      is(
        neverTranslateSitesSection.querySelector(
          ".translations-settings-languages-card"
        ),
        null,
        `${siteName} removed from Never Translate Site`
      );
    }
    const siteLen = siteNum - i - 1;
    is(
      getNeverTranslateSitesFromPerms().length,
      siteLen,
      `There are ${siteLen} site in Never translate site`
    );
  }
  await cleanup();
});
