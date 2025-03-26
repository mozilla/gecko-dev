/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals AdjustableTitle */

// This is the dialog that is displayed when adding or editing a search engine
// in about:preferences, or when adding a search engine via the context menu of
// an HTML form. Depending on the scenario where it is used, different arguments
// must be supplied in an object in `window.arguments[0]`:
// - `mode`  [required] - The type of dialog: NEW, EDIT or FORM.
// - `title` [optional] - To display a title in the window element.
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
    this._suggestUrl = document.getElementById("suggestUrl");

    this._name.addEventListener("input", this.onNameInput.bind(this));
    this._alias.addEventListener("input", this.onAliasInput.bind(this));
    this._url.addEventListener("input", this.onFormInput.bind(this));
    document.addEventListener("dialogaccept", this.onAddEngine.bind(this));
  }

  onAddEngine() {
    throw new Error("abstract");
  }

  isNameValid(name) {
    if (!name) {
      return false;
    }
    return !Services.search.getEngineByName(name);
  }

  onNameInput() {
    let name = this._name.value.trim();
    let validity = this.isNameValid(name)
      ? ""
      : document.getElementById("engineNameExists").textContent;
    this._name.setCustomValidity(validity);
    this.onFormInput();
  }

  async isAliasValid(alias) {
    if (!alias) {
      return true;
    }
    return !(await Services.search.getEngineByAlias(alias));
  }

  async onAliasInput() {
    let alias = this._alias.value.trim();
    let validity = (await this.isAliasValid(alias))
      ? ""
      : document.getElementById("engineAliasExists").textContent;
    this._alias.setCustomValidity(validity);
    this.onFormInput();
  }

  // This function is not set as a listener but called directly because it
  // depends on the output of `isAliasValid`, but `isAliasValid` contains an
  // await, so it would finish after this function.
  onFormInput() {
    this._dialog.setAttribute(
      "buttondisabledaccept",
      !this._form.checkValidity()
    );
  }
}

/**
 * This dialog is opened when adding a new search engine in preferences.
 */
class NewEngineDialog extends EngineDialog {
  onAddEngine() {
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
    this._postData = document.getElementById("enginePostData");

    this.#engine = engine;
    this._name.value = engine.name;
    this._alias.value = engine.alias ?? "";

    let [url, postData] = this.getSubmissionTemplate(
      lazy.SearchUtils.URL_TYPE.SEARCH
    );
    this._url.value = url;
    if (postData) {
      document.getElementById("enginePostDataRow").hidden = false;
      this._postData.value = postData;
    }

    let [suggestUrl] = this.getSubmissionTemplate(
      lazy.SearchUtils.URL_TYPE.SUGGEST_JSON
    );
    this._suggestUrl.value = suggestUrl ?? "";

    this.onNameInput();
    this.onAliasInput();
  }

  onAddEngine() {
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

  isNameValid(name) {
    if (!name) {
      return false;
    }
    let engine = Services.search.getEngineByName(this._name.value);
    return !engine || engine.id == this.#engine.id;
  }

  async isAliasValid(alias) {
    if (!alias) {
      return true;
    }
    let engine = await Services.search.getEngineByAlias(this._alias.value);
    return !engine || engine.id == this.#engine.id;
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
    this._name.value = nameTemplate;
    this.onNameInput();
    this.onAliasInput();

    document.getElementById("engineUrlRow").remove();
    this._url = null;
    document.getElementById("suggestUrlRow").remove();
    this._suggestUrl = null;

    let title = { raw: document.title };
    document.documentElement.setAttribute("headertitle", JSON.stringify(title));
    document.documentElement.style.setProperty(
      "--icon-url",
      'url("chrome://browser/skin/preferences/category-search.svg")'
    );
  }

  onAddEngine() {
    window.arguments[0].engineInfo = {
      name: this._name.value.trim(),
      // Empty string means no alias.
      alias: this._alias.value.trim(),
    };
  }
}

let loadedResolvers = Promise.withResolvers();
document.mozSubdialogReady = loadedResolvers.promise;

let gAddEngineDialog = null;
window.addEventListener("DOMContentLoaded", () => {
  if (!window.arguments[0].title) {
    AdjustableTitle.hide();
  }

  try {
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
