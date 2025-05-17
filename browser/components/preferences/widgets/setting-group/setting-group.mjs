/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class SettingGroup extends MozLitElement {
  static properties = {
    config: { type: Object },
    groupId: { type: String },
    // getSetting should be Preferences.getSetting from preferencesBindings.js
    getSetting: { type: Function },
  };

  createRenderRoot() {
    return this;
  }

  itemTemplate(item) {
    let setting = this.getSetting(item.id);
    if (!setting.visible) {
      return "";
    }
    return html`<setting-control
      .setting=${setting}
      .config=${item}
    ></setting-control>`;
  }

  render() {
    if (!this.config) {
      return "";
    }
    return html`<moz-fieldset data-l10n-id=${ifDefined(this.config.l10nId)}
      >${this.config.items.map(item => this.itemTemplate(item))}</moz-fieldset
    >`;
  }
}
customElements.define("setting-group", SettingGroup);
