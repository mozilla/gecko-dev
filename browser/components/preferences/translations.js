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

  /**
   * Add Lazy getter for document elements
   */
  elements: undefined,

  async init() {
    if (!this.elements) {
      this._defineLazyElements(document, {
        downloadLanguageSection: "translations-settings-download-section",
        alwaysTranslateMenuList: "translations-settings-always-translate-list",
        neverTranslateMenuList: "translations-settings-never-translate-list",
        alwaysTranslateMenuPopup:
          "translations-settings-always-translate-popup",
        neverTranslateMenuPopup: "translations-settings-never-translate-popup",
        downloadLanguageList: "translations-settings-download-language-list",
        alwaysTranslateLanguageList:
          "translations-settings-always-translate-language-list",
        neverTranslateLanguageList:
          "translations-settings-never-translate-language-list",
        neverTranslateSiteList:
          "translations-settings-never-translate-site-list",
        translationsSettingsBackButton: "translations-settings-back-button",
        translationsSettingsHeader: "translations-settings-header",
        translationsSettingsDescription: "translations-settings-description",
        translateAlwaysHeader: "translations-settings-always-translate",
        translateNeverHeader: "translations-settings-never-translate",
        translateNeverSiteHeader: "translations-settings-never-sites-header",
        translateNeverSiteDesc: "translations-settings-never-sites",
        translateDownloadLanguagesLearnMore: "download-languages-learn-more",
      });
    }
    this.elements.translationsSettingsBackButton.addEventListener(
      "click",
      function () {
        gotoPref("general");
      }
    );

    // Keyboard navigation support.
    this.elements.alwaysTranslateMenuList.addEventListener("keydown", this);
    this.elements.alwaysTranslateMenuPopup.addEventListener(
      "popuphidden",
      this
    );
    this.elements.neverTranslateMenuList.addEventListener("keydown", this);
    this.elements.neverTranslateMenuPopup.addEventListener("popuphidden", this);

    // Get the settings from the preferences into the translations.js
    this.supportedLanguages = await TranslationsParent.getSupportedLanguages();
    this.supportedLanguageTagsNames = TranslationsParent.getLanguageList(
      this.supportedLanguages
    );

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
    // Keyboard navigation support.
    this.elements.alwaysTranslateLanguageList.addEventListener("keydown", this);
    this.elements.neverTranslateLanguageList.addEventListener("keydown", this);
    this.elements.neverTranslateSiteList.addEventListener("keydown", this);
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

  _defineLazyElements(document, entries) {
    this.elements = {};
    for (const [name, elementId] of Object.entries(entries)) {
      ChromeUtils.defineLazyGetter(this.elements, name, () => {
        const element = document.getElementById(elementId);
        if (!element) {
          throw new Error(`Could not find "${name}" at "#${elementId}".`);
        }
        return element;
      });
    }
  },

  /**
   * Populate the Drop down list in <menupopup> with the list of supported languages
   * for the user to choose languages to add to Always translate and
   * Never translate settings list.
   */
  buildLanguageDropDowns() {
    const { fromLanguages } = this.supportedLanguages;
    const { alwaysTranslateMenuPopup, neverTranslateMenuPopup } = this.elements;

    for (const { langTag, displayName } of fromLanguages) {
      const alwaysLang = document.createXULElement("menuitem");
      alwaysLang.setAttribute("value", langTag);
      alwaysLang.setAttribute("label", displayName);
      alwaysTranslateMenuPopup.appendChild(alwaysLang);
      const neverLang = document.createXULElement("menuitem");
      neverLang.setAttribute("value", langTag);
      neverLang.setAttribute("label", displayName);
      neverTranslateMenuPopup.appendChild(neverLang);
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
    const { downloadLanguageList } = this.elements;

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
    const allLangElement = downloadLanguageList.firstElementChild;
    let allLangButton = allLangElement.querySelector("moz-button");

    // The first element is selected by default when keyboard navigation enters this list
    downloadLanguageList.setAttribute(
      "aria-activedescendant",
      allLangElement.id
    );
    // Keyboard navigation support.
    downloadLanguageList.addEventListener("keydown", this);
    allLangButton.addEventListener("click", this);
    allLangElement.addEventListener("keydown", this);

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

      const languageElement = this.createLangElement(
        [mozButton, languageLabel, languageSize],
        "translations-settings-download-" + language.langTag + "-language-id"
      );
      downloadLanguageList.appendChild(languageElement);
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
    for (const err of document.querySelectorAll(
      ".translations-settings-language-error"
    )) {
      this.removeError(err);
    }

    switch (event.type) {
      case "keydown":
        // Keyboard navigation support.
        this.handleKeys(event);
        break;
      case "popuphidden":
        // Handle Menulist selection through pointing device
        if (
          eventNodeParent.id === "translations-settings-always-translate-list"
        ) {
          this.handleAddAlwaysTranslateLanguage(
            event.target.parentNode.getAttribute("value")
          );
        } else if (
          eventNodeParent.id === "translations-settings-never-translate-list"
        ) {
          this.handleAddNeverTranslateLanguage(
            event.target.parentNode.getAttribute("value")
          );
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
              this.handleRemoveAllDownloadLanguages(event);
            } else {
              this.handleRemoveDownloadLanguage(event);
            }
          }
        }
        break;
    }
  },

  // Keyboard navigation support.
  handleKeys(event) {
    switch (event.key) {
      case "Enter":
        // Handle Menulist selection through keyboard
        if (event.target.id === "translations-settings-always-translate-list") {
          this.handleAddAlwaysTranslateLanguage(
            event.target.getAttribute("value")
          );
        } else if (
          event.target.id === "translations-settings-never-translate-list"
        ) {
          this.handleAddNeverTranslateLanguage(
            event.target.getAttribute("value")
          );
        }
        break;
      case "ArrowUp":
        if (
          event.target.classList.contains("translations-settings-language-list")
        ) {
          event.target.children[0].querySelector("moz-button").focus();
          // Update the selected element on the list according to the keyboard navigation by the user
          event.target.setAttribute(
            "aria-activedescendant",
            event.target.children[0].id
          );
        } else if (event.target.tagName === "moz-button") {
          if (event.target.parentNode.previousElementSibling) {
            event.target.parentNode.previousElementSibling
              .querySelector("moz-button")
              .focus();
            // Update the selected element on the list according to the keyboard navigation by the user
            event.target.parentNode.parentNode.setAttribute(
              "aria-activedescendant",
              event.target.parentNode.previousElementSibling.id
            );
            event.preventDefault();
          }
        }
        break;
      case "ArrowDown":
        if (
          event.target.classList.contains("translations-settings-language-list")
        ) {
          event.target.children[0].querySelector("moz-button").focus();
          // Update the selected element on the list according to the keyboard navigation by the user
          event.target.setAttribute(
            "aria-activedescendant",
            event.target.children[0].id
          );
        } else if (event.target.tagName === "moz-button") {
          if (event.target.parentNode.nextElementSibling) {
            event.target.parentNode.nextElementSibling
              .querySelector("moz-button")
              .focus();
            // Update the selected element on the list according to the keyboard navigation by the user
            event.target.parentNode.parentNode.setAttribute(
              "aria-activedescendant",
              event.target.parentNode.nextElementSibling.id
            );
            event.preventDefault();
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
  async handleAddAlwaysTranslateLanguage(langTag) {
    // After a language is selected the menulist button display will be set to the
    // selected langauge. After processing the button event the
    // data-l10n-id of the menulist button is restored to "Add Language"

    const { alwaysTranslateMenuList } = this.elements;
    TranslationsParent.addLangTagToPref(langTag, ALWAYS_TRANSLATE_LANGS_PREF);
    await document.l10n.translateElements([alwaysTranslateMenuList]);
  },

  /**
   * Event handler when the user wants to add a language to
   * Never translate settings preferences list.
   * @param {Event} event
   */
  async handleAddNeverTranslateLanguage(langTag) {
    // After a language is selected the menulist button display will be set to the
    // selected langauge. After processing the button event the
    // data-l10n-id of the menulist button is restored to "Add Language"

    const { neverTranslateMenuList } = this.elements;

    TranslationsParent.addLangTagToPref(langTag, NEVER_TRANSLATE_LANGS_PREF);
    await document.l10n.translateElements([neverTranslateMenuList]);
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
   * @param {string} pref - name of the preference for which the HTML is built
   *                      NEVER_TRANSLATE_LANGS_PREF / ALWAYS_TRANSLATE_LANGS_PREF
   */
  populateLanguageList(pref) {
    // languageList: <div> of the Always/Never translate section, which is a list of languages added by the user
    // curLangTags: List of Language tag set in the the preference, Always/Never translate to be populated
    // otherPref: name of the preference other than "pref" Never/Always
    //            when a language is added to "pref" remove the same from otherPref(if it exists)
    // prefix: "always"/"never" string used to create ids for the language HTML elements for respective lists.

    const { languageList, curLangTags, otherPref, prefix } =
      pref === NEVER_TRANSLATE_LANGS_PREF
        ? {
            languageList: this.elements.neverTranslateLanguageList,
            curLangTags: Array.from(this.neverTranslateLanguages),
            otherPref: ALWAYS_TRANSLATE_LANGS_PREF,
            prefix: "never",
          }
        : {
            languageList: this.elements.alwaysTranslateLanguageList,
            curLangTags: Array.from(this.alwaysTranslateLanguages),
            otherPref: NEVER_TRANSLATE_LANGS_PREF,
            prefix: "always",
          };

    const updatedLangTags =
      pref === NEVER_TRANSLATE_LANGS_PREF
        ? Array.from(TranslationsParent.getNeverTranslateLanguages())
        : Array.from(TranslationsParent.getAlwaysTranslateLanguages());

    const { added, removed } = this.setDifference(curLangTags, updatedLangTags);

    for (const lang of removed) {
      this.removeTranslateLanguage(lang, languageList);
    }

    // When the preferences is opened for the first time
    // the translations settings HTML page is initialized with
    // the exisitng settings by adding all languages from the latest preferences
    for (const lang of added) {
      this.addTranslateLanguage(lang, languageList, prefix);
      // if a language is added to Always translate list,
      // remove it from Never translate list and vice-versa
      TranslationsParent.removeLangTagFromPref(lang, otherPref);
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
    const { neverTranslateSiteList } = this.elements;

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

    // Create unique id using site name
    const languageElement = this.createLangElement(
      [mozButton, languageLabel],
      "translations-settings-" + site + "-id"
    );
    neverTranslateSiteList.insertBefore(
      languageElement,
      neverTranslateSiteList.firstElementChild
    );
    // The first element is selected by default when keyboard navigation enters this list
    neverTranslateSiteList.setAttribute(
      "aria-activedescendant",
      languageElement.id
    );
    if (neverTranslateSiteList.childElementCount) {
      neverTranslateSiteList.parentNode.hidden = false;
    }
  },

  /**
   * Removes a site from Never translate site list
   * @param {string} site
   */
  removeSite(site) {
    const { neverTranslateSiteList } = this.elements;

    const langSite = neverTranslateSiteList.querySelector(
      `label[value="${site}"]`
    );

    langSite.parentNode.remove();
    if (!neverTranslateSiteList.childElementCount) {
      neverTranslateSiteList.parentNode.hidden = true;
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
        const { neverTranslateSiteList } = this.elements;
        this.neverTranslateSites = [];
        for (const elem of neverTranslateSiteList.children) {
          elem.remove();
        }
        if (!neverTranslateSiteList.childElementCount) {
          neverTranslateSiteList.parentNode.hidden = true;
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
  createLangElement(langChildren, langId) {
    const languageElement = document.createElement("div");
    languageElement.classList.add("translations-settings-language");
    // Keyboard navigation support
    languageElement.setAttribute("role", "option");
    languageElement.id = langId;
    languageElement.addEventListener("keydown", this);

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
    // Keyboard navigation support. Do not select the buttons on the list using tab.
    // The buttons in the language lists are navigated using arrow buttons
    mozButton.setAttribute("tabindex", "-1");
    return mozButton;
  },

  /**
   * Adds a language selected by the user to the list of
   * Always/Never translate settings list in the HTML.
   * @param {string} langTag - The BCP-47 language tag for the language
   * @param {Element} languageList - HTML element for the list of the languages.
   * @param {string} translatePrefix - "never" / "always" prefix depending on the settings section
   */
  addTranslateLanguage(langTag, languageList, translatePrefix) {
    // While adding the first language, add the Header and language List div

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

    const languageElement = this.createLangElement(
      [mozButton, languageLabel],
      "translations-settings-language-" +
        translatePrefix +
        "-" +
        langTag +
        "-id"
    );
    // Add the language after the Language Header
    languageList.insertBefore(languageElement, languageList.firstElementChild);
    // The first element is selected by default when keyboard navigation enters this list
    languageList.setAttribute("aria-activedescendant", languageElement.id);
    if (languageList.childElementCount) {
      languageList.parentNode.hidden = false;
    }
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
   * Removes a language currently in the always/never translate language list
   * from the DOM. Invoked in response to changes in the relevant preferences.
   * @param {string} langTag The BCP-47 language tag for the language
   * @param {Element} languageList - HTML element for the list of the languages.
   */
  removeTranslateLanguage(langTag, languageList) {
    const langElem = languageList.querySelector(`label[value=${langTag}]`);
    if (langElem) {
      langElem.parentNode.remove();
    }
    if (!languageList.childElementCount) {
      languageList.parentNode.hidden = true;
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
   * Updates the button icons and its download states for the download language elements
   * in the HTML by getting the download status of all languages from the browser records.
   */
  async reloadDownloadPhases() {
    let downloadCount = 0;
    const { downloadLanguageList } = this.elements;

    const allLangElem = downloadLanguageList.firstElementChild;
    const allLangButton = allLangElem.querySelector("moz-button");

    const updatePromises = [];
    for (const langElem of downloadLanguageList.querySelectorAll(
      ".translations-settings-language:not(:first-child)"
    )) {
      const langLabel = langElem.querySelector("label");
      const langTag = langLabel.getAttribute("value");
      const langButton = langElem.querySelector("moz-button");

      updatePromises.push(
        TranslationsParent.hasAllFilesForLanguage(langTag).then(
          hasAllFilesForLanguage => {
            if (hasAllFilesForLanguage) {
              downloadCount += 1;
              this.changeButtonState({
                langButton,
                langTag,
                langState: "downloaded",
              });
            } else {
              this.changeButtonState({
                langButton,
                langTag,
                langState: "removed",
              });
            }
            langButton.removeAttribute("disabled");
          }
        )
      );
    }
    await Promise.allSettled(updatePromises);

    const allDownloaded =
      downloadCount === this.supportedLanguageTagsNames.length;
    if (allDownloaded) {
      this.changeButtonState({
        langButton: allLangButton,
        langTag: "all",
        langState: "downloaded",
      });
    } else {
      this.changeButtonState({
        langButton: allLangButton,
        langTag: "all",
        langState: "removed",
      });
    }
  },

  showErrorMessage(parentNode, fluentId, language) {
    const errorElement = document.createElement("moz-message-bar");
    errorElement.setAttribute("type", "error");
    document.l10n.setAttributes(errorElement, fluentId, {
      name: language,
    });
    errorElement.classList.add("translations-settings-language-error");
    parentNode.appendChild(errorElement);
  },

  removeError(errorNode) {
    errorNode?.remove();
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

    try {
      await TranslationsParent.downloadLanguageFiles(langTag);
    } catch (error) {
      console.error(error);

      this.showErrorMessage(
        eventButton.parentNode,
        "translations-settings-language-download-error",
        TranslationsParent.getLanguageDisplayName(langTag)
      );
      const hasAllFilesForLanguage =
        await TranslationsParent.hasAllFilesForLanguage(langTag);

      if (!hasAllFilesForLanguage) {
        this.changeButtonState({
          langButton: eventButton,
          langTag,
          langState: "removed",
        });
        return;
      }
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
          this.elements.downloadLanguageList.firstElementChild.querySelector(
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
  async handleRemoveDownloadLanguage(event) {
    let eventButton = event.target;
    const langTag = eventButton.parentNode
      .querySelector("label")
      .getAttribute("value");

    this.changeButtonState({
      langButton: eventButton,
      langTag,
      langState: "loading",
    });

    try {
      await TranslationsParent.deleteLanguageFiles(langTag);
    } catch (error) {
      // The download phases are invalidated with the error and must be reloaded.
      console.error(error);
      this.showErrorMessage(
        eventButton.parentNode,
        "translations-settings-language-remove-error",
        TranslationsParent.getLanguageDisplayName(langTag)
      );
      const hasAllFilesForLanguage =
        await TranslationsParent.hasAllFilesForLanguage(langTag);
      if (hasAllFilesForLanguage) {
        this.changeButtonState({
          langButton: eventButton,
          langTag,
          langState: "downloaded",
        });
        return;
      }
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
          this.elements.downloadLanguageList.firstElementChild.querySelector(
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

    try {
      await TranslationsParent.downloadAllFiles();
    } catch (error) {
      console.error(error);
      await this.reloadDownloadPhases();
      this.showErrorMessage(
        eventButton.parentNode,
        "translations-settings-language-download-error",
        "all"
      );
      return;
    }
    this.changeButtonState({
      langButton: eventButton,
      langTag: "all",
      langState: "downloaded",
    });
    this.updateAllLanguageDownloadButtons("downloaded");
  },

  /**
   * Event Handler to remove all language models
   * @param {Event} event
   */
  async handleRemoveAllDownloadLanguages(event) {
    let eventButton = event.target;
    this.disableDownloadButtons();
    this.changeButtonState({
      langButton: eventButton,
      langTag: "all",
      langState: "loading",
    });

    try {
      await TranslationsParent.deleteAllLanguageFiles();
    } catch (error) {
      console.error(error);
      await this.reloadDownloadPhases();
      this.showErrorMessage(
        eventButton.parentNode,
        "translations-settings-language-remove-error",
        "all"
      );
      return;
    }
    this.changeButtonState({
      langButton: eventButton,
      langTag: "all",
      langState: "removed",
    });
    this.updateAllLanguageDownloadButtons("removed");
  },

  /**
   * Disables the buttons to download/remove inidividual languages
   * when "all languages" are downloaded/removed.
   * This is done to ensure that no individual languages are downloaded/removed
   * when the download/remove operations for "all languages" is progress.
   */
  disableDownloadButtons() {
    const { downloadLanguageList } = this.elements;

    // Disable all elements except the first one which is "All langauges"
    for (const langElem of downloadLanguageList.querySelectorAll(
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
    const { downloadLanguageList } = this.elements;

    // Change the state of all individual language buttons except the first one which is "All langauges"
    for (const langElem of downloadLanguageList.querySelectorAll(
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
