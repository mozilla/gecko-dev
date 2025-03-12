/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals AdjustableTitle */

let gAddEngineDialog = {
  _form: null,
  _name: null,
  _alias: null,
  loadedResolvers: Promise.withResolvers(),

  onLoad() {
    try {
      this.init();
    } finally {
      this.loadedResolvers.resolve();
    }
  },

  init() {
    this._dialog = document.querySelector("dialog");
    this._form = document.getElementById("addEngineForm");
    this._name = document.getElementById("engineName");
    this._alias = document.getElementById("engineAlias");

    // These arguments only exist if this dialog was opened via
    // "Add Search Engine" in the context menu.
    if (window.arguments?.[0]) {
      let { uri, formData, charset, method, icon } = window.arguments[0];
      this._formData = formData;
      this._charset = charset;
      this._method = method;
      this._icon = icon;
      this._uri = uri.spec;

      this._name.value = uri.host;
      this.onFormInput();

      document.getElementById("engineUrlRow").remove();
      document.getElementById("suggestUrlRow").remove();
      let title = { raw: document.title };
      document.documentElement.setAttribute(
        "headertitle",
        JSON.stringify(title)
      );
      document.documentElement.style.setProperty(
        "--icon-url",
        'url("chrome://browser/skin/preferences/category-search.svg")'
      );
    } else {
      AdjustableTitle.hide();
    }

    this._name.addEventListener("input", this.onNameInput.bind(this));
    this._alias.addEventListener("input", this.onAliasInput.bind(this));
    this._form.addEventListener("input", this.onFormInput.bind(this));

    document.addEventListener("dialogaccept", this.onAddEngine.bind(this));
  },

  onAddEngine() {
    let url =
      this._uri ||
      document.getElementById("engineUrl").value.replace(/%s/, "{searchTerms}");

    let suggestUrl = document
      .getElementById("suggestUrl")
      ?.value.replace(/%s/, "{searchTerms}");

    Services.search.addUserEngine({
      url,
      name: this._name.value,
      alias: this._alias.value,
      // The values below may be undefined.
      formData: this._formData,
      charset: this._charset,
      method: this._method,
      icon: this._icon,
      suggestUrl,
    });
  },

  onNameInput() {
    if (this._name.value) {
      let engine = Services.search.getEngineByName(this._name.value);
      let validity = engine
        ? document.getElementById("engineNameExists").textContent
        : "";
      this._name.setCustomValidity(validity);
    }
  },

  async onAliasInput() {
    let validity = "";
    if (this._alias.value) {
      let engine = await Services.search.getEngineByAlias(this._alias.value);
      if (engine) {
        validity = document.getElementById("engineAliasExists").textContent;
      }
    }
    this._alias.setCustomValidity(validity);
  },

  onFormInput() {
    this._dialog.setAttribute(
      "buttondisabledaccept",
      !this._form.checkValidity()
    );
  },
};

document.mozSubdialogReady = gAddEngineDialog.loadedResolvers.promise;

window.addEventListener("load", () => gAddEngineDialog.onLoad());
