/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

export class MLModelListIntro extends MozLitElement {
  render() {
    return this.template;
  }

  get template() {
    return html`
      <link
        href="chrome://mozapps/content/extensions/components/mlmodel-list-intro.css"
        rel="stylesheet"
      />
      <header>
        <p data-l10n-id="mlmodel-description">
          <a
            data-l10n-name="learn-more"
            is="moz-support-link"
            support-page="local-models"
          ></a>
        </p>
      </header>
    `;
  }
}
customElements.define("mlmodel-list-intro", MLModelListIntro);
