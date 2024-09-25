/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

/**
 * The permission type to give to Services.perms for Translations.
 */
const TRANSLATIONS_PERMISSION = "translations";

/**
 * The list of BCP-47 language tags that will trigger auto-translate.
 */
const ALWAYS_TRANSLATE_LANGS_PREF =
  "browser.translations.alwaysTranslateLanguages";

/**
 * The list of BCP-47 language tags that will prevent auto-translate.
 */
const NEVER_TRANSLATE_LANGS_PREF =
  "browser.translations.neverTranslateLanguages";

let gTranslationsPane = {
  /**
   * List of languages set in the Always Translate Preferences
   * @type Array<string>
   */
  alwaysTranslateLanguages: [],

  /**
   * List of languages set in the Never Translate Preferences
   * @type Array<string>
   */
  neverTranslateLanguages: [],

  /**
   * List of languages set in the Never Translate Site Preferences
   * @type Array<string>
   */
  neverTranslateSites: [],

  /**
   * A mapping from the language tag to the current download phase for that language
   * and it's download size.
   * @type {Map<string, {downloadPhase: "downloaded" | "removed" | "loading", size: number}>}
   */
  downloadPhases: new Map(),

  /**
   * Object with details of languages supported by the browser namely
   * languagePairs, fromLanguages, toLanguages
   * @type {object} supportedLanguages
   */
  supportedLanguages: {},

  /**
   * List of languages names supported along with their tags (BCP 47 locale identifiers).
   * @type Array<{ langTag: string, displayName: string}>
   */
  supportedLanguageTagsNames: [],

  async init() {
    document
      .getElementById("translations-settings-back-button")
      .addEventListener("click", function () {
        gotoPref("general");
      });

    document
      .getElementById("translations-settings-always-translate-list")
      .addEventListener("command", this);

    document
      .getElementById("translations-settings-never-translate-list")
      .addEventListener("command", this);

    // Get the settings from the preferences into the translations.js
    this.supportedLanguages = await TranslationsParent.getSupportedLanguages();
    this.supportedLanguageTagsNames = TranslationsParent.getLanguageList(
      this.supportedLanguages
    );
    this.alwaysTranslateLanguages =
      TranslationsParent.getAlwaysTranslateLanguages();
    this.neverTranslateLanguages =
      TranslationsParent.getNeverTranslateLanguages();

    this.neverTranslateSites = TranslationsParent.listNeverTranslateSites();

    // Deploy observers
    Services.obs.addObserver(this, "perm-changed");
    Services.obs.addObserver(
      this,
      "translations:always-translate-languages-changed"
    );
    Services.obs.addObserver(
      this,
      "translations:never-translate-languages-changed"
    );
    window.addEventListener("unload", () => this.removeObservers());

    // Build the HTML elements
    this.buildLanguageDropDowns();
    this.populateLanguageList(ALWAYS_TRANSLATE_LANGS_PREF);
    this.populateLanguageList(NEVER_TRANSLATE_LANGS_PREF);
    this.populateSiteList();

    await this.initDownloadInfo();
    this.buildDownloadLanguageList();

    // The translations settings page takes a long time to initialize
    // This event can be used to wait until the initialization is done.
    document.dispatchEvent(
      new CustomEvent("translationsSettingsInit", {
        bubbles: true,
        cancelable: true,
      })
    );
  },

  /**
   * Populate the Drop down list in <menupopup> with the list of supported languages
   * for the user to choose languages to add to Always translate and
   * Never translate settings list.
   */
  buildLanguageDropDowns() {
    const { fromLanguages } = this.supportedLanguages;
    const alwaysLangPopup = document.getElementById(
      "translations-settings-always-translate-popup"
    );
    const neverLangPopup = document.getElementById(
      "translations-settings-never-translate-popup"
    );

    for (const { langTag, displayName } of fromLanguages) {
      const alwaysLang = document.createXULElement("menuitem");
      alwaysLang.setAttribute("value", langTag);
      alwaysLang.setAttribute("label", displayName);
      alwaysLangPopup.appendChild(alwaysLang);
      const neverLang = document.createXULElement("menuitem");
      neverLang.setAttribute("value", langTag);
      neverLang.setAttribute("label", displayName);
      neverLangPopup.appendChild(neverLang);
    }
  },

  /**
   * Initializes the downloadPhases by checking the download status of each language.
   *
   * @see gTranslationsPane.downloadPhases
   */
  async initDownloadInfo() {
    let downloadCount = 0;
    let allDownloadSize = 0;

    this.downloadPhases = new Map();
    for (const language of this.supportedLanguageTagsNames) {
      let downloadSize = await TranslationsParent.getLanguageSize(
        language.langTag
      );
      allDownloadSize += downloadSize;
      const hasAllFilesForLanguage =
        await TranslationsParent.hasAllFilesForLanguage(language.langTag);
      const downloadPhase = hasAllFilesForLanguage ? "downloaded" : "removed";
      this.downloadPhases.set(language.langTag, {
        downloadPhase,
        size: downloadSize,
      });
      downloadCount += downloadPhase === "downloaded" ? 1 : 0;
    }
    const allDownloadPhase =
      downloadCount === this.supportedLanguageTagsNames.length
        ? "downloaded"
        : "removed";
    this.downloadPhases.set("all", {
      downloadPhase: allDownloadPhase,
      size: allDownloadSize,
    });
  },

  /**
   * Show a list of languages for the user to be able to download
   * and remove language models for local translation.
   */
  buildDownloadLanguageList() {
    const downloadList = document.querySelector(
      "#translations-settings-download-section .translations-settings-language-list"
    );

    function createSizeElement(downloadSize) {
      const languageSize = document.createElement("span");
      languageSize.classList.add("translations-settings-download-size");
      const [size, units] = DownloadUtils.convertByteUnits(downloadSize);

      document.l10n.setAttributes(
        languageSize,
        "translations-settings-download-size",
        {
          size: size + " " + units,
        }
      );
      return languageSize;
    }

    // The option to download "All languages" is added in xhtml.
    // Here the option to download individual languages is dynamically added
    // based on the supported language list
    const allLangElement = downloadList.children[0];
    let allLangButton = allLangElement.querySelector("moz-button");

    allLangButton.addEventListener("click", this);

    for (const language of this.supportedLanguageTagsNames) {
      const downloadSize = this.downloadPhases.get(language.langTag).size;

      const languageSize = createSizeElement(downloadSize);

      const languageLabel = this.createLangLabel(
        language.displayName,
        language.langTag,
        "translations-settings-download-" + language.langTag
      );

      const isDownloaded =
        this.downloadPhases.get(language.langTag).downloadPhase ===
        "downloaded";

      const mozButton = isDownloaded
        ? this.createIconButton(
            [
              "translations-settings-remove-icon",
              "translations-settings-manage-downloaded-language-button",
            ],
            "translations-settings-remove-button",
            language.displayName
          )
        : this.createIconButton(
            [
              "translations-settings-download-icon",
              "translations-settings-manage-downloaded-language-button",
            ],
            "translations-settings-download-button",
            language.displayName
          );

      const languageElement = this.createLangElement([
        mozButton,
        languageLabel,
        languageSize,
      ]);
      downloadList.appendChild(languageElement);
    }

    // Updating "All Language" download button according to the state
    if (this.downloadPhases.get("all").downloadPhase === "downloaded") {
      this.changeButtonState({
        langButton: allLangButton,
        langTag: "all",
        langState: "downloaded",
      });
    }

    const allDownloadSize = this.downloadPhases.get("all").size;
    const languageSize = createSizeElement(allDownloadSize);

    allLangElement.appendChild(languageSize);
  },

  handleEvent(event) {
    const eventNode = event.target;
    const eventNodeParent = eventNode.parentNode;
    const eventNodeClassList = eventNode.classList;
    switch (event.type) {
      case "command":
        if (
          eventNodeParent.id === "translations-settings-always-translate-popup"
        ) {
          this.handleAddAlwaysTranslateLanguage(event);
        } else if (
          eventNodeParent.id === "translations-settings-never-translate-popup"
        ) {
          this.handleAddNeverTranslateLanguage(event);
        }
        break;
      case "click":
        if (eventNodeClassList.contains("translations-settings-site-button")) {
          this.handleRemoveNeverTranslateSite(event);
        } else if (
          eventNodeClassList.contains(
            "translations-settings-language-never-button"
          )
        ) {
          this.handleRemoveNeverTranslateLanguage(event);
        } else if (
          eventNodeClassList.contains(
            "translations-settings-language-always-button"
          )
        ) {
          this.handleRemoveAlwaysTranslateLanguage(event);
        } else if (
          eventNodeClassList.contains(
            "translations-settings-manage-downloaded-language-button"
          )
        ) {
          if (
            eventNodeClassList.contains("translations-settings-download-icon")
          ) {
            if (
              eventNodeParent.querySelector("label").id ===
              "translations-settings-download-all-languages"
            ) {
              this.handleDownloadAllLanguages(event);
            } else {
              this.handleDownloadLanguage(event);
            }
          } else if (
            eventNodeClassList.contains("translations-settings-remove-icon")
          ) {
            if (
              eventNodeParent.querySelector("label").id ===
              "translations-settings-download-all-languages"
            ) {
              this.handleRemoveAllLanguages(event);
            } else {
              this.handleRemoveLanguage(event);
            }
          }
        }
        break;
    }
  },

  /**
   * Event handler when the user wants to add a language to
   * Always translate settings preferences list.
   * @param {Event} event
   */
  async handleAddAlwaysTranslateLanguage(event) {
    // After a language is selected the menulist button display will be set to the
    // selected langauge. After processing the button event the
    // data-l10n-id of the menulist button is restored to "Add Language"
    const menuList = document
      .getElementById("translations-settings-always-translate-section")
      .querySelector("menulist");

    TranslationsParent.addLangTagToPref(
      event.target.getAttribute("value"),
      ALWAYS_TRANSLATE_LANGS_PREF
    );
    await document.l10n.translateElements([menuList]);
  },

  /**
   * Event handler when the user wants to add a language to
   * Never translate settings preferences list.
   * @param {Event} event
   */
  async handleAddNeverTranslateLanguage(event) {
    // After a language is selected the menulist button display will be set to the
    // selected langauge. After processing the button event the
    // data-l10n-id of the menulist button is restored to "Add Language"
    const menuList = document
      .getElementById("translations-settings-never-translate-section")
      .querySelector("menulist");

    TranslationsParent.addLangTagToPref(
      event.target.getAttribute("value"),
      NEVER_TRANSLATE_LANGS_PREF
    );
    await document.l10n.translateElements([menuList]);
  },

  /**
   * Finds the langauges added and/or removed in the
   * Always/Never translate lists.
   * @param {Array<string>} currentSet
   * @param {Array<string>} newSet
   * @returns {Object} {Array<string>, Array<string>}
   */
  setDifference(currentSet, newSet) {
    const added = newSet.filter(lang => !currentSet.includes(lang));
    const removed = currentSet.filter(lang => !newSet.includes(lang));
    return { added, removed };
  },

  /**
   * Builds HTML elements for the Always/Never translate list
   * According to the preference setting
   * @param {string} pref name of the preference for which the HTML is built
   */
  populateLanguageList(pref) {
    // sectionId: HTML ID of the section, Always/Never translate to be populated
    // curLangTags: List of Language tag set in the the preference, Always/Never translate to be populated
    // otherPref: name of the preference other than "pref" Never/Always
    // when a language is added to "pref" remove the same from otherPref(if it exists)
    const { sectionId, curLangTags, otherPref } =
      pref === NEVER_TRANSLATE_LANGS_PREF
        ? {
            sectionId: "translations-settings-never-translate-section",
            curLangTags: Array.from(this.neverTranslateLanguages),
            otherPref: ALWAYS_TRANSLATE_LANGS_PREF,
          }
        : {
            sectionId: "translations-settings-always-translate-section",
            curLangTags: Array.from(this.alwaysTranslateLanguages),
            otherPref: NEVER_TRANSLATE_LANGS_PREF,
          };

    const updatedLangTags =
      pref === NEVER_TRANSLATE_LANGS_PREF
        ? Array.from(TranslationsParent.getNeverTranslateLanguages())
        : Array.from(TranslationsParent.getAlwaysTranslateLanguages());

    const { added, removed } = this.setDifference(curLangTags, updatedLangTags);

    const translateSection = document.getElementById(sectionId);
    const languageList = translateSection.querySelector(
      ".translations-settings-language-list label"
    );

    for (const lang of removed) {
      this.removeLanguage(lang, sectionId);
    }

    // if a language is added to Always translate list,
    // remove it from Never translate list and vice-versa
    if (languageList) {
      for (const lang of added) {
        this.addLanguage(lang, sectionId);
        // Remove from other list
        TranslationsParent.removeLangTagFromPref(lang, otherPref);
      }
    } else {
      // if languageList does not exist then this is the initialization
      // phase. Add all languages from the latest preferences
      for (const lang of updatedLangTags) {
        this.addLanguage(lang, sectionId);
        // Remove from other list
        TranslationsParent.removeLangTagFromPref(lang, otherPref);
      }
    }
    // Update state for neverTranslateLanguages/alwaysTranslateLanguages
    if (pref === NEVER_TRANSLATE_LANGS_PREF) {
      this.neverTranslateLanguages = updatedLangTags;
    } else {
      this.alwaysTranslateLanguages = updatedLangTags;
    }
  },

  /**
   * Adds a site to Never translate site list
   * @param {string} site
   */
  addSite(site) {
    const translateSection = document.getElementById(
      "translations-settings-never-sites-section"
    );
    let languageList = translateSection.querySelector(
      ".translations-settings-language-list"
    );

    // While adding the first language, add the Header and language List div
    if (!languageList) {
      languageList = this.addLanguageList(translateSection, languageList);
    }

    // Label and textContent of the added site element is the same
    const languageLabel = this.createLangLabel(
      site,
      site,
      "translations-settings-" + site
    );

    const mozButton = this.createIconButton(
      [
        "translations-settings-remove-icon",
        "translations-settings-site-button",
      ],
      "translations-settings-remove-site-button-2",
      site
    );

    const languageElement = this.createLangElement([mozButton, languageLabel]);
    languageList.insertBefore(languageElement, languageList.firstChild);
  },

  /**
   * Removes a site from Never translate site list
   * @param {string} site
   */
  removeSite(site) {
    const translateSection = document.getElementById(
      "translations-settings-never-sites-section"
    );
    const languageList = translateSection.querySelector(
      ".translations-settings-language-list"
    );

    const langSite = languageList.querySelector(`label[value="${site}"]`);

    langSite.parentNode.remove();
    if (!languageList.childElementCount) {
      // If there is no language in the list remove the
      // Language Header and language list div
      languageList.parentNode.remove();
    }
  },

  /**
   * Builds HTML elements for the Never translate Site list
   * According to the permissions setting
   */
  populateSiteList() {
    const siteList = TranslationsParent.listNeverTranslateSites();
    for (const site of siteList) {
      this.addSite(site);
    }
    this.neverTranslateSites = siteList;
  },

  /**
   * Oberver
   * @param {string} subject Notification specific interface pointer.
   * @param {string} topic nsPref:changed/perm-changed
   * @param {string} data cleared/changed/added/deleted
   */
  observe(subject, topic, data) {
    if (topic === "perm-changed") {
      if (data === "cleared") {
        const translateSection = document.getElementById(
          "translations-settings-never-sites-section"
        );
        const languageList = translateSection.querySelector(
          ".translations-settings-language-list"
        );
        this.neverTranslateSites = [];
        if (languageList) {
          languageList.parentNode.remove();
        }
      } else {
        const perm = subject.QueryInterface(Ci.nsIPermission);
        if (perm.type != TRANSLATIONS_PERMISSION) {
          // The updated permission was not for Translations, nothing to do.
          return;
        }
        if (data === "added") {
          if (perm.capability != Services.perms.DENY_ACTION) {
            // We are only showing data for sites we should never translate.
            // If the permission is not DENY_ACTION, we don't care about it here.
            return;
          }
          this.neverTranslateSites =
            TranslationsParent.listNeverTranslateSites();
          this.addSite(perm.principal.origin);
        } else if (data === "deleted") {
          this.neverTranslateSites =
            TranslationsParent.listNeverTranslateSites();
          this.removeSite(perm.principal.origin);
        }
      }
    } else if (topic === "translations:never-translate-languages-changed") {
      this.populateLanguageList(NEVER_TRANSLATE_LANGS_PREF);
    } else if (topic === "translations:always-translate-languages-changed") {
      this.populateLanguageList(ALWAYS_TRANSLATE_LANGS_PREF);
    }
  },

  /**
   * Removes Observers
   */
  removeObservers() {
    Services.obs.removeObserver(this, "perm-changed");
    Services.obs.removeObserver(
      this,
      "translations:always-translate-languages-changed"
    );
    Services.obs.removeObserver(
      this,
      "translations:never-translate-languages-changed"
    );
  },

  /**
   * Create a div HTML element representing a language.
   * @param {Array} langChildren
   * @returns {Element} div HTML element
   */
  createLangElement(langChildren) {
    const languageElement = document.createElement("div");
    languageElement.classList.add("translations-settings-language");
    for (const child of langChildren) {
      languageElement.appendChild(child);
    }
    return languageElement;
  },

  /**
   * Creates a moz-button element as icon
   * @param {string} classNames classes added to the moz-button element
   * @param {string} buttonFluentID Fluent ID for the aria-label
   * @param {string} accessibleName  "name" variable value of the aria-label
   * @returns {Element} HTML element of type Moz-Button
   */
  createIconButton(classNames, buttonFluentID, accessibleName) {
    const mozButton = document.createElement("moz-button");

    for (const className of classNames) {
      mozButton.classList.add(className);
    }
    mozButton.setAttribute("type", "ghost icon");
    // Note: aria-labelledby cannot be used as the id is not available for the shadow DOM element
    document.l10n.setAttributes(mozButton, buttonFluentID, {
      name: accessibleName,
    });
    mozButton.addEventListener("click", this);
    return mozButton;
  },

  /**
   * Adds a language selected by the user to the list of
   * Always/Never translate settings list in the HTML.
   * @param {string} langTag
   * @param {string} sectionId
   */
  addLanguage(langTag, sectionId) {
    const translatePrefix =
      sectionId === "translations-settings-never-translate-section"
        ? "never"
        : "always";
    const translateSection = document.getElementById(sectionId);
    let languageList = translateSection.querySelector(
      ".translations-settings-language-list"
    );

    // While adding the first language, add the Header and language List div
    if (!languageList) {
      languageList = this.addLanguageList(translateSection, languageList);
    }

    const languageDisplayName =
      TranslationsParent.getLanguageDisplayName(langTag);
    const languageLabel = this.createLangLabel(
      languageDisplayName,
      langTag,
      "translations-settings-language-" + translatePrefix + "-" + langTag
    );

    const mozButton = this.createIconButton(
      [
        "translations-settings-remove-icon",
        "translations-settings-language-" + translatePrefix + "-button",
      ],
      "translations-settings-remove-language-button-2",
      languageDisplayName
    );

    const languageElement = this.createLangElement([mozButton, languageLabel]);
    // Add the language after the Language Header
    languageList.insertBefore(languageElement, languageList.firstChild);
  },

  /**
   * Creates a label HTML element representing
   * a language
   * @param {string} textContent
   * @param {string} value
   * @param {string} id
   * @returns {Element} HTML element of type label
   */
  createLangLabel(textContent, value, id) {
    const languageLabel = document.createElement("label");
    languageLabel.textContent = textContent;
    languageLabel.setAttribute("value", value);
    languageLabel.id = id;
    return languageLabel;
  },

  /**
   * The language list in different sections of the translations setting is
   * added only when languages/sites are added in the respective setting.
   * This function creates and returns the language list element a HTML section
   *
   * @param {string} translateSection
   * @param {Array} languageList
   * @returns {Element} Language list element a HTML section
   */
  addLanguageList(translateSection, languageList) {
    const languageCard = document.createElement("div");
    languageCard.classList.add("translations-settings-languages-card");
    translateSection.appendChild(languageCard);

    const languageHeader = document.createElement("h3");
    languageCard.appendChild(languageHeader);
    languageHeader.setAttribute(
      "data-l10n-id",
      "translations-settings-language-header"
    );
    languageHeader.classList.add("translations-settings-language-header");

    languageList = document.createElement("div");
    languageList.classList.add("translations-settings-language-list");
    languageCard.appendChild(languageList);
    return languageList;
  },

  /**
   * Removes a language currently in the always/never translate language list
   * from the DOM. Invoked in response to changes in the relevant preferences.
   * @param {string} lang
   * @param {string} sectionId
   */
  removeLanguage(lang, sectionId) {
    const translateSection = document.getElementById(sectionId);
    const languageList = translateSection.querySelector(
      ".translations-settings-language-list"
    );
    if (languageList) {
      const langElem = languageList.querySelector(`label[value=${lang}]`);
      if (langElem) {
        langElem.parentNode.remove();
        if (!languageList.childElementCount) {
          // If there is no language in the list remove the
          // Language Header and language list div
          languageList.parentNode.remove();
        }
      }
    }
  },

  /**
   * Event Handler to remove a language selected by the user from the list of
   * Always translate settings list in Preferences.
   * @param {Event} event
   */
  handleRemoveAlwaysTranslateLanguage(event) {
    TranslationsParent.removeLangTagFromPref(
      event.target.parentNode.querySelector("label").getAttribute("value"),
      ALWAYS_TRANSLATE_LANGS_PREF
    );
  },

  /**
   * Event Handler to remove a language selected by the user from the list of
   * Never translate settings list in Preferences.
   * @param {Event} event
   */
  handleRemoveNeverTranslateLanguage(event) {
    TranslationsParent.removeLangTagFromPref(
      event.target.parentNode.querySelector("label").getAttribute("value"),
      NEVER_TRANSLATE_LANGS_PREF
    );
  },

  /**
   * Removes the site chosen by the user in the HTML
   * from the Never Translate Site Permission
   * @param {Event} event
   */
  handleRemoveNeverTranslateSite(event) {
    TranslationsParent.setNeverTranslateSiteByOrigin(
      false,
      event.target.parentNode.querySelector("label").getAttribute("value")
    );
  },
  /**
   * Record the download phase downloaded/loading/removed for
   * given language in the local data.
   * @param {string} langTag
   * @param {string} downloadPhase
   */
  updateDownloadPhase(langTag, downloadPhase) {
    if (!this.downloadPhases.has(langTag)) {
      console.error(
        `Expected downloadPhases entry for ${langTag}, but found none.`
      );
    } else {
      this.downloadPhases.get(langTag).downloadPhase = downloadPhase;
    }
  },
  /**
   * Remove the existing download language elements and rebuild
   * the download language elements in the HTML by getting the download
   * status of all languages from the browser records.
   */
  reloadDownloadPhases() {
    // buildDownloadLanguageList will reset the download phases
    const downloadList = document.querySelector(
      "#translations-settings-download-section .translations-settings-language-list"
    );
    while (downloadList.firstElementChild) {
      downloadList.firstElementChild.remove();
    }
    this.buildDownloadLanguageList();
  },

  /**
   * Event Handler to download a language model selected by the user through HTML
   * @param {Event} event
   */
  async handleDownloadLanguage(event) {
    let eventButton = event.target;
    const langTag = eventButton.parentNode
      .querySelector("label")
      .getAttribute("value");

    this.changeButtonState({
      langButton: eventButton,
      langTag,
      langState: "loading",
    });

    // TODO (1907591): Implement error handling
    // The correct state for the download button in case of a download error
    // needs to be determined and implemented.
    try {
      await TranslationsParent.downloadLanguageFiles(langTag);
    } catch (error) {
      this.changeButtonState({
        langButton: eventButton,
        langTag,
        langState: "removed",
      });
      console.error(error);
      return;
    }
    this.changeButtonState({
      langButton: eventButton,
      langTag,
      langState: "downloaded",
    });

    // If all languages are downloaded, change "All Languages" to downloaded
    const haveRemovedItem = [...this.downloadPhases].some(
      ([k, v]) => v.downloadPhase != "downloaded" && k != "all"
    );
    if (
      !haveRemovedItem &&
      this.downloadPhases.get("all").downloadPhase !== "downloaded"
    ) {
      this.changeButtonState({
        langButton:
          event.target.parentNode.parentNode.children[0].querySelector(
            "moz-button"
          ),
        langTag: "all",
        langState: "downloaded",
      });
    }
  },

  /**
   * Event Handler to remove a language model selected by the user through HTML
   * @param {Event} event
   */
  async handleRemoveLanguage(event) {
    let eventButton = event.target;
    const langTag = eventButton.parentNode
      .querySelector("label")
      .getAttribute("value");

    this.changeButtonState({
      langButton: eventButton,
      langTag,
      langState: "loading",
    });

    // TODO (1907591): Implement error handling
    // The correct state for the download button in case of a remove error
    // needs to be determined and implemented.
    try {
      await TranslationsParent.deleteLanguageFiles(langTag);
    } catch (error) {
      // The download phases are invalidated with the error and must be reloaded.
      this.changeButtonState({
        langButton: eventButton,
        langTag,
        langState: "downloaded",
      });
      console.error(error);
      return;
    }

    this.changeButtonState({
      langButton: eventButton,
      langTag,
      langState: "removed",
    });

    // If >=1 languages are removed change "All Languages" state to removed
    if (this.downloadPhases.get("all").downloadPhase === "downloaded") {
      this.changeButtonState({
        langButton:
          event.target.parentNode.parentNode.children[0].querySelector(
            "moz-button"
          ),
        langTag: "all",
        langState: "removed",
      });
    }
  },

  /**
   * Event Handler to download all language models
   * @param {Event} event
   */
  async handleDownloadAllLanguages(event) {
    // Disable all buttons and show loading icon
    this.disableDownloadButtons();
    let eventButton = event.target;
    this.changeButtonState({
      langButton: eventButton,
      langTag: "all",
      langState: "loading",
    });

    // TODO (1907591): Implement error handling
    // The correct state for the download button in case of a download error
    // needs to be determined and implemented.
    try {
      await TranslationsParent.downloadAllFiles();
      this.changeButtonState({
        langButton: eventButton,
        langTag: "all",
        langState: "downloaded",
      });
      this.updateAllLanguageDownloadButtons("downloaded");
    } catch (error) {
      await this.reloadDownloadPhases();
      console.error(error);
    }
  },

  /**
   * Event Handler to remove all language models
   * @param {Event} event
   */
  async handleRemoveAllLanguages(event) {
    let eventButton = event.target;
    this.disableDownloadButtons();
    this.changeButtonState({
      langButton: eventButton,
      langTag: "all",
      langState: "loading",
    });

    // TODO (1907591): Implement error handling
    // The correct state for the download button in case of a remove error
    // needs to be determined and implemented.
    try {
      await TranslationsParent.deleteAllLanguageFiles();
      this.changeButtonState({
        langButton: eventButton,
        langTag: "all",
        langState: "removed",
      });
      this.updateAllLanguageDownloadButtons("removed");
    } catch (error) {
      await this.reloadDownloadPhases();
      console.error(error);
    }
  },

  /**
   * Disables the buttons to download/remove inidividual languages
   * when "all languages" are downloaded/removed.
   * This is done to ensure that no individual languages are downloaded/removed
   * when the download/remove operations for "all languages" is progress.
   */
  disableDownloadButtons() {
    const downloadList = document.querySelector(
      "#translations-settings-download-section .translations-settings-language-list"
    );
    // Disable all elements except the first one which is "All langauges"
    for (const langElem of downloadList.querySelectorAll(
      ".translations-settings-language:not(:first-child)"
    )) {
      const langButton = langElem.querySelector("moz-button");
      langButton.setAttribute("disabled", "true");
    }
  },

  /**
   * Changes the state of all individual language buttons as downloaded/removed
   * based on the download state of "All Language" status
   * changes the icon of individual language buttons:
   * from "download" icon to "remove" icon if "All Language" is downloaded.
   * from "remove" icon to "download" icon if "All Language" is removed.
   * @param {string} allLanguageDownloadStatus "All Language" status: downloaded/removed
   */
  updateAllLanguageDownloadButtons(allLanguageDownloadStatus) {
    const downloadList = document.querySelector(
      "#translations-settings-download-section .translations-settings-language-list"
    );

    // Change the state of all individual language buttons except the first one which is "All langauges"
    for (const langElem of downloadList.querySelectorAll(
      ".translations-settings-language:not(:first-child)"
    )) {
      let langButton = langElem.querySelector("moz-button");
      const langLabel = langElem.querySelector("label");
      const downloadPhase = this.downloadPhases.get(
        langLabel.getAttribute("value")
      ).downloadPhase;

      langButton.removeAttribute("disabled");

      if (
        downloadPhase !== "downloaded" &&
        allLanguageDownloadStatus === "downloaded"
      ) {
        // In case of "All languages" downloaded
        this.changeButtonState({
          langButton,
          langTag: langLabel.getAttribute("value"),
          langState: "downloaded",
        });
      } else if (
        downloadPhase === "downloaded" &&
        allLanguageDownloadStatus === "removed"
      ) {
        // In case of "All languages" removed
        this.changeButtonState({
          langButton,
          langTag: langLabel.getAttribute("value"),
          langState: "removed",
        });
      }
    }
  },

  /**
   *  Updates the state of a language download button.
   *
   * This function changes the button's appearance and behavior based on the current language state
   * (e.g., "download", "loading", or "removed"). The button's icon and CSS class are updated to reflect
   * the state, and the appropriate event handler is set for downloading or removing the language.
   * The aria-label for accessibility is also updated using the Fluent string.
   *
   * @param {object} options -
   * @param {Element} options.langButton - The HTML button element representing the language action (download/remove).
   * @param {string} options.langTag - The BCP-47 language tag for the language associated with the button.
   * @param {string} options.langState - The current state of the language, which can be "downloaded", "loading", or "removed".
   */
  changeButtonState({ langButton, langTag, langState }) {
    // Remove any icon by removing it's respective CSS class
    langButton.classList.remove(
      "translations-settings-download-icon",
      "translations-settings-loading-icon",
      "translations-settings-remove-icon"
    );
    // Set new icon based on the state of the language model
    switch (langState) {
      case "downloaded":
        // If language is downloaded show 'remove icon' as an option
        // for the user to remove the downloaded language model.
        langButton.classList.add("translations-settings-remove-icon");
        // The respective aria-label for accessibility is updated with correct Fluent string.
        if (langTag === "all") {
          document.l10n.setAttributes(
            langButton,
            "translations-settings-remove-all-button"
          );
        } else {
          document.l10n.setAttributes(
            langButton,
            "translations-settings-remove-button",
            {
              name: document.l10n.getAttributes(langButton).args.name,
            }
          );
        }
        break;
      case "removed":
        // If language is removed show 'download icon' as an option
        // for the user to download the language model.
        langButton.classList.add("translations-settings-download-icon");
        // The respective aria-label for accessibility is updated with correct Fluent string.
        if (langTag === "all") {
          document.l10n.setAttributes(
            langButton,
            "translations-settings-download-all-button"
          );
        } else {
          document.l10n.setAttributes(
            langButton,
            "translations-settings-download-button",
            {
              name: document.l10n.getAttributes(langButton).args.name,
            }
          );
        }
        break;
      case "loading":
        // While processing the download or remove language model
        // show 'loading icon' to the user
        langButton.classList.add("translations-settings-loading-icon");
        // The respective aria-label for accessibility is updated with correct Fluent string.
        if (langTag === "all") {
          document.l10n.setAttributes(
            langButton,
            "translations-settings-loading-all-button"
          );
        } else {
          document.l10n.setAttributes(
            langButton,
            "translations-settings-loading-button",
            {
              name: document.l10n.getAttributes(langButton).args.name,
            }
          );
        }
        break;
    }
    this.updateDownloadPhase(langTag, langState);
  },
};
