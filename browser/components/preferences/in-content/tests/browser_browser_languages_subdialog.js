/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm");


AddonTestUtils.initMochitest(this);

const BROWSER_LANGUAGES_URL = "chrome://browser/content/preferences/browserLanguages.xul";
const DICTIONARY_ID_PL = "pl@dictionaries.addons.mozilla.org";

function langpackId(locale) {
  return `langpack-${locale}@firefox.mozilla.org`;
}

function getManifestData(locale, version = "2.0") {
  return {
    langpack_id: locale,
    name: `${locale} Language Pack`,
    description: `${locale} Language pack`,
    languages: {
      [locale]: {
        chrome_resources: {
          "branding": `browser/chrome/${locale}/locale/branding/`,
        },
        version: "1",
      },
    },
    applications: {
      gecko: {
        strict_min_version: AppConstants.MOZ_APP_VERSION,
        id: langpackId(locale),
        strict_max_version: AppConstants.MOZ_APP_VERSION,
      },
    },
    version,
    manifest_version: 2,
    sources: {
      browser: {
        base_path: "browser/",
      },
    },
    author: "Mozilla",
  };
}

let testLocales = ["fr", "pl", "he"];
let testLangpacks;

function createLangpack(locale, version) {
    return AddonTestUtils.createTempXPIFile({
      "manifest.json": getManifestData(locale, version),
      [`browser/${locale}/branding/brand.ftl`]: "-brand-short-name = Firefox",
    });
}

function createTestLangpacks() {
  if (!testLangpacks) {
    testLangpacks = Promise.all(testLocales.map(async locale => [
      locale,
      await createLangpack(locale),
    ]));
  }
  return testLangpacks;
}

function createLocaleResult(target_locale, url) {
  return {
    guid: langpackId(target_locale),
    type: "language",
    target_locale,
    current_compatible_version: {
      files: [{
        platform: "all",
        url,
      }],
    },
  };
}

async function createLanguageToolsFile() {
  let langpacks = await createTestLangpacks();
  let results = langpacks.map(([locale, file]) =>
    createLocaleResult(locale, Services.io.newFileURI(file).spec));

  let filename = "language-tools.json";
  let files = {[filename]: {results}};
  let tempdir = AddonTestUtils.tempDir.clone();
  let dir = await AddonTestUtils.promiseWriteFilesToDir(tempdir.path, files);
  dir.append(filename);

  return dir;
}

async function createDictionaryBrowseResults() {
  let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
  let dictionaryPath = testDir + "/addons/pl-dictionary.xpi";
  let filename = "dictionaries.json";
  let response = {
    page_size: 25,
    page_count: 1,
    count: 1,
    results: [{
      current_version: {
        id: 1823648,
        compatibility: {
          firefox: {max: "9999", min: "4.0"},
        },
        files: [{
          platform: "all",
          url: dictionaryPath,
        }],
        version: "1.0.20160228",
      },
      default_locale: "pl",
      description: "Polish spell-check",
      guid: DICTIONARY_ID_PL,
      name: "Polish Dictionary",
      slug: "polish-spellchecker-dictionary",
      status: "public",
      summary: "Polish dictionary",
      type: "dictionary",
    }],
  };

  let files = {[filename]: response};
  let dir = await AddonTestUtils.promiseWriteFilesToDir(
    AddonTestUtils.tempDir.path, files);
  dir.append(filename);

  return dir;
}

function assertLocaleOrder(list, locales) {
  is(list.itemCount, locales.split(",").length,
     "The right number of locales are selected");
  is(Array.from(list.children).map(child => child.value).join(","),
     locales, "The selected locales are in order");
}

function assertAvailableLocales(list, locales) {
  let items = Array.from(list.firstElementChild.children);
  let listLocales = items
    .filter(item => item.value && item.value != "search");
  is(listLocales.length, locales.length, "The right number of locales are available");
  is(listLocales.map(item => item.value).sort(),
     locales.sort().join(","), "The available locales match");
  is(items[0].getAttribute("class"), "label-item", "The first row is a label");
}

function selectLocale(localeCode, available, dialogDoc) {
  let [locale] = Array.from(available.firstElementChild.children)
    .filter(item => item.value == localeCode);
  available.selectedItem = locale;
  dialogDoc.getElementById("add").doCommand();
}

async function openDialog(doc, search = false) {
  let dialogLoaded = promiseLoadSubDialog(BROWSER_LANGUAGES_URL);
  if (search) {
    doc.getElementById("defaultBrowserLanguageSearch").doCommand();
    doc.getElementById("defaultBrowserLanguage").firstElementChild.hidePopup();
  } else {
    doc.getElementById("manageBrowserLanguagesButton").doCommand();
  }
  let dialogWin = await dialogLoaded;
  let dialogDoc = dialogWin.document;
  return {
    dialog: dialogDoc.getElementById("BrowserLanguagesDialog"),
    dialogDoc,
    available: dialogDoc.getElementById("availableLocales"),
    selected: dialogDoc.getElementById("selectedLocales"),
  };
}

add_task(async function testDisabledBrowserLanguages() {
  let langpacksFile = await createLanguageToolsFile();
  let langpacksUrl = Services.io.newFileURI(langpacksFile).spec;

  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", true],
      ["intl.locale.requested", "en-US,pl,he,de"],
      ["extensions.langpacks.signatures.required", false],
      ["extensions.getAddons.langpacks.url", langpacksUrl],
    ],
  });

  // Install an old pl langpack.
  let oldLangpack = await createLangpack("pl", "1.0");
  await AddonTestUtils.promiseInstallFile(oldLangpack);

  // Install all the other available langpacks.
  let pl;
  let langpacks = await createTestLangpacks();
  let addons = await Promise.all(langpacks.map(async ([locale, file]) => {
    if (locale == "pl") {
      pl = await AddonManager.getAddonByID(langpackId("pl"));
      // Disable pl so it's removed from selected.
      await pl.disable();
      return pl;
    }
    let install = await AddonTestUtils.promiseInstallFile(file);
    return install.addon;
  }));


  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let {dialogDoc, available, selected} = await openDialog(doc);

  // pl is not selected since it's disabled.
  is(pl.userDisabled, true, "pl is disabled");
  is(pl.version, "1.0", "pl is the old 1.0 version");
  assertLocaleOrder(selected, "en-US,he");

  // Only fr is enabled and not selected, so it's the only locale available.
  assertAvailableLocales(available, ["fr"]);

  // Search for more languages.
  available.firstElementChild.lastElementChild.doCommand();
  available.firstElementChild.hidePopup();
  await waitForMutation(
    available.firstElementChild,
    {childList: true},
    target =>
      Array.from(available.firstElementChild.children)
        .some(locale => locale.value == "pl"));

  // pl is now available since it is available remotely.
  assertAvailableLocales(available, ["fr", "pl"]);

  // Add pl.
  selectLocale("pl", available, dialogDoc);

  // Wait for pl to be added, this should upgrade and enable the existing langpack.
  await waitForMutation(
    selected,
    {childList: true},
    target => selected.itemCount == 3);
  assertLocaleOrder(selected, "pl,en-US,he");

  // Find pl again since it's been upgraded.
  pl = await AddonManager.getAddonByID(langpackId("pl"));
  is(pl.userDisabled, false, "pl is now enabled");
  is(pl.version, "2.0", "pl is upgraded to version 2.0");

  await Promise.all(addons.map(addon => addon.uninstall()));
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testReorderingBrowserLanguages() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", true],
      ["intl.locale.requested", "en-US,pl,he,de"],
      ["extensions.langpacks.signatures.required", false],
    ],
  });

  // Install all the available langpacks.
  let langpacks = await createTestLangpacks();
  let addons = await Promise.all(langpacks.map(async ([locale, file]) => {
    let install = await AddonTestUtils.promiseInstallFile(file);
    return install.addon;
  }));

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let messageBar = doc.getElementById("confirmBrowserLanguage");
  is(messageBar.hidden, true, "The message bar is hidden at first");

  // Open the dialog.
  let {dialog, dialogDoc, selected} = await openDialog(doc);

  // The initial order is set by the pref, filtered by available.
  assertLocaleOrder(selected, "en-US,pl,he");

  // Moving pl down changes the order.
  selected.selectedItem = selected.querySelector("[value='pl']");
  dialogDoc.getElementById("down").doCommand();
  assertLocaleOrder(selected, "en-US,he,pl");

  // Accepting the change shows the confirm message bar.
  let dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;
  is(messageBar.hidden, false, "The message bar is now visible");
  is(messageBar.querySelector("button").getAttribute("locales"), "en-US,he,pl",
     "The locales are set on the message bar button");

  // Open the dialog again.
  let newDialog = await openDialog(doc);
  dialog = newDialog.dialog;
  dialogDoc = newDialog.dialogDoc;
  selected = newDialog.selected;

  // The initial order comes from the previous settings.
  assertLocaleOrder(selected, "en-US,he,pl");

  // Select pl in the list.
  selected.selectedItem = selected.querySelector("[value='pl']");
  // Move pl back up.
  dialogDoc.getElementById("up").doCommand();
  assertLocaleOrder(selected, "en-US,pl,he");

  // Accepting the change hides the confirm message bar.
  dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;
  is(messageBar.hidden, true, "The message bar is hidden again");

  await Promise.all(addons.map(addon => addon.uninstall()));

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testAddAndRemoveSelectedLanguages() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", true],
      ["intl.locale.requested", "en-US"],
      ["extensions.langpacks.signatures.required", false],
    ],
  });

  let langpacks = await createTestLangpacks();
  let addons = await Promise.all(langpacks.map(async ([locale, file]) => {
    let install = await AddonTestUtils.promiseInstallFile(file);
    return install.addon;
  }));

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let messageBar = doc.getElementById("confirmBrowserLanguage");
  is(messageBar.hidden, true, "The message bar is hidden at first");

  // Open the dialog.
  let {dialog, dialogDoc, available, selected} = await openDialog(doc);

  // The initial order is set by the pref.
  assertLocaleOrder(selected, "en-US");
  assertAvailableLocales(available, ["fr", "pl", "he"]);

  // Add pl and fr to selected.
  selectLocale("pl", available, dialogDoc);
  selectLocale("fr", available, dialogDoc);

  assertLocaleOrder(selected, "fr,pl,en-US");
  assertAvailableLocales(available, ["he"]);

  // Remove pl and fr from selected.
  dialogDoc.getElementById("remove").doCommand();
  dialogDoc.getElementById("remove").doCommand();
  assertLocaleOrder(selected, "en-US");
  assertAvailableLocales(available, ["fr", "pl", "he"]);

  // Add he to selected.
  selectLocale("he", available, dialogDoc);
  assertLocaleOrder(selected, "he,en-US");
  assertAvailableLocales(available, ["pl", "fr"]);

  // Accepting the change shows the confirm message bar.
  let dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;

  await waitForMutation(
    messageBar,
    {attributes: true, attributeFilter: ["hidden"]},
    target => !target.hidden);

  is(messageBar.hidden, false, "The message bar is now visible");
  is(messageBar.querySelector("button").getAttribute("locales"), "he,en-US",
    "The locales are set on the message bar button");

  await Promise.all(addons.map(addon => addon.uninstall()));

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testInstallFromAMO() {
  let langpacks = await AddonManager.getAddonsByTypes(["locale"]);
  is(langpacks.length, 0, "There are no langpacks installed");

  let langpacksFile = await createLanguageToolsFile();
  let langpacksUrl = Services.io.newFileURI(langpacksFile).spec;
  let dictionaryBrowseFile = await createDictionaryBrowseResults();
  let browseApiEndpoint = Services.io.newFileURI(dictionaryBrowseFile).spec;

  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", true],
      ["intl.locale.requested", "en-US"],
      ["extensions.getAddons.langpacks.url", langpacksUrl],
      ["extensions.langpacks.signatures.required", false],
      ["extensions.getAddons.get.url", browseApiEndpoint],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  let messageBar = doc.getElementById("confirmBrowserLanguage");
  is(messageBar.hidden, true, "The message bar is hidden at first");

  // Open the dialog.
  let {dialog, dialogDoc, available, selected} = await openDialog(doc, true);

  // Make sure the message bar is still hidden.
  is(messageBar.hidden, true, "The message bar is still hidden after searching");

  if (available.itemCount == 1) {
    await waitForMutation(
      available.firstElementChild,
      {childList: true},
      target => available.itemCount > 1);
  }

  // The initial order is set by the pref.
  assertLocaleOrder(selected, "en-US");
  assertAvailableLocales(available, ["fr", "he", "pl"]);
  is(Services.locale.availableLocales.join(","),
     "en-US", "There is only one installed locale");

  // Verify that there are no extra dictionaries.
  let dicts = await AddonManager.getAddonsByTypes(["dictionary"]);
  is(dicts.length, 0, "There are no installed dictionaries");

  // Add Polish, this will install the langpack.
  selectLocale("pl", available, dialogDoc);

  // Wait for the langpack to install and be added to the list.
  let selectedLocales = dialogDoc.getElementById("selectedLocales");
  await waitForMutation(
    selectedLocales,
    {childList: true},
    target => selectedLocales.itemCount == 2);

  // Verify the list is correct.
  assertLocaleOrder(selected, "pl,en-US");
  assertAvailableLocales(available, ["fr", "he"]);
  is(Services.locale.availableLocales.sort().join(","),
     "en-US,pl", "Polish is now installed");

  await BrowserTestUtils.waitForCondition(async () => {
    let newDicts = await AddonManager.getAddonsByTypes(["dictionary"]);
    let done = newDicts.length != 0;

    if (done) {
      is(newDicts[0].id, DICTIONARY_ID_PL, "The polish dictionary was installed");
    }

    return done;
  });

  // Move pl down the list, which prevents an error since it isn't valid.
  dialogDoc.getElementById("down").doCommand();
  assertLocaleOrder(selected, "en-US,pl");

  // Test that disabling the langpack removes it from the list.
  let dialogClosed = BrowserTestUtils.waitForEvent(dialogDoc.documentElement, "dialogclosing");
  dialog.acceptDialog();
  await dialogClosed;

  // Disable the Polish langpack.
  let langpack = await AddonManager.getAddonByID("langpack-pl@firefox.mozilla.org");
  await langpack.disable();

  ({dialogDoc, available, selected} = await openDialog(doc, true));

  // Wait for the available langpacks to load.
  if (available.itemCount == 1) {
    await waitForMutation(
      available.firstElementChild,
      {childList: true},
      target => available.itemCount > 1);
  }
  assertLocaleOrder(selected, "en-US");
  assertAvailableLocales(available, ["fr", "he", "pl"]);

  // Uninstall the langpack and dictionary.
  let installs = await AddonManager.getAddonsByTypes(["locale", "dictionary"]);
  is(installs.length, 2, "There is one langpack and one dictionary installed");
  await Promise.all(installs.map(item => item.uninstall()));

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

let hasSearchOption = popup => Array.from(popup.children).some(el => el.value == "search");

add_task(async function testDownloadEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let defaultMenulist = doc.getElementById("defaultBrowserLanguage");
  ok(hasSearchOption(defaultMenulist.firstChild), "There's a search option in the General pane");

  let { available } = await openDialog(doc, false);
  ok(hasSearchOption(available.firstChild), "There's a search option in the dialog");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});


add_task(async function testDownloadDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["intl.multilingual.enabled", true],
      ["intl.multilingual.downloadEnabled", false],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let defaultMenulist = doc.getElementById("defaultBrowserLanguage");
  ok(!hasSearchOption(defaultMenulist.firstChild), "There's no search option in the General pane");

  let { available } = await openDialog(doc, false);
  ok(!hasSearchOption(available.firstChild), "There's no search option in the dialog");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
