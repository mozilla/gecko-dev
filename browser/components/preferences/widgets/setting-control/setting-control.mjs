/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  createRef,
  html,
  ifDefined,
  ref,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class SettingControl extends MozLitElement {
  #lastSetting;

  static properties = {
    setting: { type: Object },
    config: { type: Object },
    value: {},
    parentDisabled: { type: Boolean },
  };

  constructor() {
    super();
    this.inputRef = createRef();
  }

  createRenderRoot() {
    return this;
  }

  get inputEl() {
    return this.inputRef.value;
  }

  async getUpdateComplete() {
    let result = await super.getUpdateComplete();
    await this.inputEl.updateComplete;
    return result;
  }

  onSettingChange = () => {
    this.value = this.setting.value;
  };

  willUpdate(changedProperties) {
    if (changedProperties.has("setting")) {
      if (this.#lastSetting) {
        this.#lastSetting.off("change", this.onSettingChange);
      }
      this.#lastSetting = this.setting;
      this.value = this.getValue();
      this.setting.on("change", this.onSettingChange);
    }
  }

  getValue() {
    return this.setting.value;
  }

  // Called by our parent when our input changed.
  onChange(el) {
    this.setting.userChange(el.checked);
    this.value = this.getValue();
  }

  render() {
    let { config } = this;
    let itemArgs =
      config.items
        ?.map(i => ({
          config: i,
          setting: this.getSetting(i.id),
        }))
        .filter(i => i.setting.visible) || [];
    switch (config.control) {
      case "checkbox":
      default:
        return html`<moz-checkbox
          id=${config.id}
          data-l10n-id=${config.l10nId}
          .iconSrc=${config.iconSrc}
          .checked=${this.value}
          .supportPage=${this.config.supportPage}
          .parentDisabled=${this.parentDisabled}
          .control=${this}
          data-subcategory=${ifDefined(this.config.subcategory)}
          ?disabled=${this.setting.locked}
          ${ref(this.inputRef)}
          >${itemArgs.map(
            opts =>
              html`<setting-control
                .config=${opts.config}
                .setting=${opts.setting}
                .getSetting=${this.getSetting}
                slot="nested"
              ></setting-control>`
          )}</moz-checkbox
        >`;
    }
  }
}
customElements.define("setting-control", SettingControl);
