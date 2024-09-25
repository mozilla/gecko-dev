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

  info(
    "Open translations settings page by clicking on translations settings button."
  );
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

  info(
    "In translations settings page, click on back button to go back to main preferences page."
  );
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

  info(
    "Open translations settings page by clicking on translations settings button."
  );
  const { translateAlwaysMenuList } =
    await TranslationsSettingsTestUtils.openAboutPreferencesTranslationsSettingsPane(
      settingsButton
    );

  info("Testing the Always translate langauge settings");
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

  info(
    "Open translations settings page by clicking on translations settings button."
  );
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

  info("Testing the Never translate langauge settings");
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

  info("Ensure the Always/Never list is empty initially.");

  is(
    translateSection.querySelector(".translations-settings-languages-card"),
    null,
    `Language list not present in ${sectionName} Translate list`
  );

  const menuOptions = menuList.querySelector("menupopup").children;

  info(
    "Click each language on the menulist to add it into the Always/Never list."
  );
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

  info(
    "Remove each language from the Always/Never list that we added initially."
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
      info(
        "Check if the language list card is removed after removing the last language from the Always/Never list"
      );
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

  info("Ensure the Always/Never list is empty initially.");
  is(
    translateSection.querySelector(".translations-settings-languages-card"),
    null,
    `Language list not present in ${sectionName} Translate list`
  );

  info(
    "Add languages to the Always/Never list in translations setting by setting the ALWAYS_TRANSLATE_LANGS_PREF/NEVER_TRANSLATE_LANGS_PREF."
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

  info(
    "Open translations settings page by clicking on translations settings button."
  );
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

  info(
    "Ensure that the Never translate sites in permissions settings are reflected in Never translate sites section of translations settings page"
  );

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

  info(
    "Delete each site by clicking the button in Never translate sites section of translations settings page and check if it is removed in the Never translate sites in permissions settings"
  );
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
      info(
        "Check if the language list card is removed after removing the last language."
      );
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
