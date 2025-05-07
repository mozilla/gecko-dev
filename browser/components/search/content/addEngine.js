/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals AdjustableTitle */

// This is the dialog that is displayed when adding or editing a search engine
// in about:preferences, or when adding a search engine via the context menu of
// an HTML form. Depending on the scenario where it is used, different arguments
// must be supplied in an object in `window.arguments[0]`:
// - `mode`  [required] - The type of dialog: NEW, EDIT or FORM.
// - `title` [optional] - Whether to display a title in the window element.
// - all arguments required by the constructor of the dialog class

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

// Set the appropriate l10n id before the dialog's connectedCallback.
let mode = window.arguments[0].mode == "EDIT" ? "edit" : "add";
document.l10n.setAttributes(
  document.querySelector("dialog"),
  mode + "-engine-dialog"
);
document.l10n.setAttributes(
  document.querySelector("window"),
  mode + "-engine-window"
);

let loadedResolvers = Promise.withResolvers();
document.mozSubdialogReady = loadedResolvers.promise;

/** @type {?EngineDialog} */
let gAddEngineDialog = null;
/** @type {?Map<string, string>} */
let l10nCache = null;

/**
 * The abstract base class for all types of user search engine dialogs.
 * All subclasses must implement the abstract method `onAddEngine`.
 */
class EngineDialog {
  constructor() {
    this._dialog = document.querySelector("dialog");

    this._form = document.getElementById("addEngineForm");
    this._name = document.getElementById("engineName");
    this._alias = document.getElementById("engineAlias");
    this._url = document.getElementById("engineUrl");
    this._postData = document.getElementById("enginePostData");
    this._suggestUrl = document.getElementById("suggestUrl");

    this._form.addEventListener("input", e => this.validateInput(e.target));
    document.addEventListener("dialogaccept", this.onAccept.bind(this));
  }

  onAccept() {
    throw new Error("abstract");
  }

  validateName() {
    let name = this._name.value.trim();
    if (!name) {
      this.setValidity(this._name, "add-engine-no-name");
      return;
    }

    let existingEngine = Services.search.getEngineByName(name);
    if (existingEngine && !this.allowedNames.includes(name)) {
      this.setValidity(this._name, "add-engine-name-exists");
    } else {
      this.setValidity(this._name, null);
    }
  }

  async validateAlias() {
    let alias = this._alias.value.trim();
    if (!alias) {
      this.setValidity(this._alias, null);
      return;
    }

    let existingEngine = await Services.search.getEngineByAlias(alias);
    if (existingEngine && !this.allowedAliases.includes(alias)) {
      this.setValidity(this._alias, "add-engine-keyword-exists");
    } else {
      this.setValidity(this._alias, null);
    }
  }

  /**
   * Validates the passed URL input element and updates error messages.
   *
   * @param {HTMLInputElement} urlInput
   *   The URL input to validate.
   * @param {boolean} required
   *   Whether the input is required or optional.
   */
  validateUrlInput(urlInput, required) {
    let urlString = urlInput.value.trim();
    if (!urlString) {
      if (required) {
        this.setValidity(urlInput, "add-engine-no-url");
        return;
      }
      this.setValidity(urlInput, null);
      return;
    }

    let url = URL.parse(urlString);
    if (!url) {
      this.setValidity(urlInput, "add-engine-invalid-url");
      return;
    }

    if (url.protocol == "http:" || url.protocol == "https:") {
      this.setValidity(urlInput, null);
    } else {
      this.setValidity(urlInput, "add-engine-invalid-protocol");
    }
  }

  /**
   * Validates the passed input element and updates error messages.
   *
   * @param {HTMLInputElement} input
   *   The input element to validate.
   */
  async validateInput(input) {
    switch (input.id) {
      case this._name.id:
        this.validateName();
        break;
      case this._alias.id:
        await this.validateAlias();
        break;
      case this._url.id:
        this.validateUrlInput(this._url, true);
        break;
      case this._suggestUrl.id:
        this.validateUrlInput(this._suggestUrl, false);
        break;
    }
  }

  async validateAll() {
    for (let input of this._form.elements) {
      await this.validateInput(input);
    }
  }

  /**
   * Sets the validity of the passed input element to the string belonging
   * to the passed l10n id. Also updates the input's error label and
   * the accept button.
   *
   * @param {HTMLInputElement} inputElement
   * @param {string} l10nId
   *   The l10n id of the string to use as validity.
   *   Must be a key of `l10nCache`.
   */
  setValidity(inputElement, l10nId) {
    if (l10nId) {
      inputElement.setCustomValidity(l10nCache.get(l10nId));
    } else {
      inputElement.setCustomValidity("");
    }

    let errorLabel = inputElement.parentElement.querySelector(".error-label");
    let validationMessage = inputElement.validationMessage;

    // If valid, set the error label to "valid" to ensure the layout doesn't shift.
    // The CSS already hides the error label based on the validity of `inputElement`.
    errorLabel.textContent = validationMessage || "valid";

    this._dialog.setAttribute(
      "buttondisabledaccept",
      !this._form.checkValidity()
    );
  }

  /**
   * Engine names that always are allowed, even if they are already in use.
   * This is needed for the edit engine dialog.
   *
   * @type {string[]}
   */
  get allowedNames() {
    return [];
  }

  /**
   * Engine aliases that always are allowed, even if they are already in use.
   * This is needed for the edit engine dialog.
   *
   * @type {string[]}
   */
  get allowedAliases() {
    return [];
  }
}

/**
 * This dialog is opened when adding a new search engine in preferences.
 */
class NewEngineDialog extends EngineDialog {
  constructor() {
    super();
    document.l10n.setAttributes(this._name, "add-engine-name-placeholder");
    document.l10n.setAttributes(this._url, "add-engine-url-placeholder");
    document.l10n.setAttributes(this._alias, "add-engine-keyword-placeholder");

    document.getElementById("enginePostDataRow").remove();
    this._postData = null;

    this.validateAll();
  }

  onAccept() {
    Services.search.addUserEngine({
      name: this._name.value.trim(),
      url: this._url.value.trim().replace(/%s/, "{searchTerms}"),
      suggestUrl: this._suggestUrl.value.trim().replace(/%s/, "{searchTerms}"),
      alias: this._alias.value.trim(),
    });
  }
}

/**
 * This dialog is opened when editing a user search engine in preferences.
 */
class EditEngineDialog extends EngineDialog {
  #engine;
  /**
   * Initializes the dialog with information from a user search engine.
   *
   * @param {object} args
   *   The arguments.
   * @param {nsISearchEngine} args.engine
   *   The search engine to edit. Must be a UserSearchEngine.
   */
  constructor({ engine }) {
    super();
    this.#engine = engine;
    this._name.value = engine.name;
    this._alias.value = engine.alias ?? "";

    let [url, postData] = this.getSubmissionTemplate(
      lazy.SearchUtils.URL_TYPE.SEARCH
    );
    this._url.value = url;
    if (postData) {
      this._postData.value = postData;
    } else {
      document.getElementById("enginePostDataRow").hidden = true;
    }

    let [suggestUrl] = this.getSubmissionTemplate(
      lazy.SearchUtils.URL_TYPE.SUGGEST_JSON
    );
    if (suggestUrl) {
      this._suggestUrl.value = suggestUrl;
    }

    this.validateAll();
  }

  onAccept() {
    this.#engine.wrappedJSObject.rename(this._name.value.trim());
    this.#engine.alias = this._alias.value.trim();

    let newURL = this._url.value.trim();
    let newPostData = this._postData.value.trim();

    // UserSearchEngine.changeUrl() does not check whether the URL has actually changed.
    let [prevURL, prevPostData] = this.getSubmissionTemplate(
      lazy.SearchUtils.URL_TYPE.SEARCH
    );
    if (newURL != prevURL || (prevPostData && prevPostData != newPostData)) {
      this.#engine.wrappedJSObject.changeUrl(
        lazy.SearchUtils.URL_TYPE.SEARCH,
        newURL.replace(/%s/, "{searchTerms}"),
        prevPostData ? newPostData.replace(/%s/, "{searchTerms}") : null
      );
    }

    let newSuggestURL = this._suggestUrl.value.trim() || null;
    let [prevSuggestUrl] = this.getSubmissionTemplate(
      lazy.SearchUtils.URL_TYPE.SUGGEST_JSON
    );
    if (newSuggestURL != prevSuggestUrl) {
      this.#engine.wrappedJSObject.changeUrl(
        lazy.SearchUtils.URL_TYPE.SUGGEST_JSON,
        newSuggestURL.replace(/%s/, "{searchTerms}"),
        null
      );
    }
  }

  get allowedAliases() {
    return [this.#engine.alias];
  }

  get allowedNames() {
    return [this.#engine.name];
  }

  /**
   * Returns url and post data templates of the requested type.
   * Both contain %s in place of the search terms.
   *
   * If no url of the requested type exists, both are null.
   * If the url is a GET url, the post data is null.
   *
   * @param {string} urlType
   *   The `SearchUtils.URL_TYPE`.
   * @returns {[?string, ?string]}
   *   Array of the url and post data.
   */
  getSubmissionTemplate(urlType) {
    let submission = this.#engine.getSubmission("searchTerms", urlType);
    if (!submission) {
      return [null, null];
    }
    let postData = null;
    if (submission.postData) {
      let binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
        Ci.nsIBinaryInputStream
      );
      binaryStream.setInputStream(submission.postData.data);

      postData = binaryStream
        .readBytes(binaryStream.available())
        .replace("searchTerms", "%s");
    }
    let url = submission.uri.spec.replace("searchTerms", "%s");
    return [url, postData];
  }
}

/**
 * This dialog is opened via the context menu of an input and lets the
 * user choose a name and an alias for an engine. Unlike the other two
 * dialogs, it does not add or change an engine in the search service,
 * and instead returns the user input to the caller.
 *
 * The chosen name and alias are returned via `window.arguments[0].engineInfo`.
 * If the user chooses to not save the engine, it's undefined.
 */
class NewEngineFromFormDialog extends EngineDialog {
  /**
   * Initializes the dialog.
   *
   * @param {object} args
   *   The arguments.
   * @param {string} args.nameTemplate
   *   The initial value of the name input.
   */
  constructor({ nameTemplate }) {
    super();
    document.getElementById("engineUrlRow").remove();
    this._url = null;
    document.getElementById("suggestUrlRow").remove();
    this._suggestUrl = null;
    document.getElementById("enginePostDataRow").remove();
    this._postData = null;

    this._name.value = nameTemplate;
    this.validateAll();
  }

  onAccept() {
    // Return the input to the caller.
    window.arguments[0].engineInfo = {
      name: this._name.value.trim(),
      // Empty string means no alias.
      alias: this._alias.value.trim(),
    };
  }
}

async function initL10nCache() {
  const errorIds = [
    "add-engine-name-exists",
    "add-engine-keyword-exists",
    "add-engine-no-name",
    "add-engine-no-url",
    "add-engine-invalid-protocol",
    "add-engine-invalid-url",
  ];

  let msgs = await document.l10n.formatValues(errorIds.map(id => ({ id })));
  l10nCache = new Map();

  for (let i = 0; i < errorIds.length; i++) {
    l10nCache.set(errorIds[i], msgs[i]);
  }
}

window.addEventListener("DOMContentLoaded", async () => {
  try {
    if (window.arguments[0].title) {
      document.documentElement.setAttribute(
        "headertitle",
        JSON.stringify({ raw: document.title })
      );
    } else {
      AdjustableTitle.hide();
    }

    await initL10nCache();

    switch (window.arguments[0].mode) {
      case "NEW":
        gAddEngineDialog = new NewEngineDialog();
        break;
      case "EDIT":
        gAddEngineDialog = new EditEngineDialog(window.arguments[0]);
        break;
      case "FORM":
        gAddEngineDialog = new NewEngineFromFormDialog(window.arguments[0]);
        break;
      default:
        throw new Error("Mode not supported for addEngine dialog.");
    }
  } finally {
    loadedResolvers.resolve();
  }
});
