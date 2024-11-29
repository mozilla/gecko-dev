/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  classMap,
  repeat,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
import { navigateToLink } from "chrome://browser/content/firefoxview/helpers.mjs";

/**
 * An empty state card to be used throughout Firefox View
 *
 * @property {string} headerLabel - (Optional) The l10n id for the header text for the empty/error state
 * @property {object} headerArgs - (Optional) The l10n args for the header text for the empty/error state
 * @property {string} isInnerCard - (Optional) True if the card is displayed within another card and needs a border instead of box shadow
 * @property {boolean} isSelectedTab - (Optional) True if the component is the selected navigation tab - defaults to false
 * @property {Array} descriptionLabels - (Optional) An array of l10n ids for the secondary description text for the empty/error state
 * @property {object} descriptionLink - (Optional) An object describing the l10n name and url needed within a description label
 * @property {string} mainImageUrl - (Optional) The chrome:// url for the main image of the empty/error state
 * @property {string} errorGrayscale - (Optional) The image should be shown in gray scale
 * @property {boolean} openLinkInParentWindow - (Optional) The link, when clicked, should be opened programatically in the parent window.
 */
class FxviewEmptyState extends MozLitElement {
  constructor() {
    super();
    this.isSelectedTab = false;
    this.descriptionLabels = [];
    this.headerArgs = {};
  }

  static properties = {
    headerLabel: { type: String },
    headerArgs: { type: Object },
    isInnerCard: { type: Boolean },
    isSelectedTab: { type: Boolean },
    descriptionLabels: { type: Array },
    desciptionLink: { type: Object },
    mainImageUrl: { type: String },
    errorGrayscale: { type: Boolean },
    openLinkInParentWindow: { type: Boolean },
  };

  static queries = {
    headerEl: ".header",
    descriptionEls: { all: ".description" },
  };

  linkTemplate(descriptionLink) {
    if (!descriptionLink) {
      return html``;
    }
    return html` <a
      data-l10n-name=${descriptionLink.name}
      href=${descriptionLink.url}
      target=${descriptionLink?.sameTarget ? "_self" : "_blank"}
    />`;
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/firefoxview/fxview-empty-state.css"
      />
      <card-container
        hideHeader="true"
        exportparts="image"
        ?isInnerCard="${this.isInnerCard}"
        id="card-container"
        isEmptyState="true"
        role="group"
        aria-labelledby="header"
        aria-describedby="description"
      >
        <div
          slot="main"
          part="container"
          class=${classMap({
            selectedTab: this.isSelectedTab,
            imageHidden: !this.mainImageUrl,
          })}
        >
          <div class="image-container" part="image-container">
            <img
              class=${classMap({
                image: true,
                greyscale: this.errorGrayscale,
              })}
              part="image"
              role="presentation"
              alt=""
              ?hidden=${!this.mainImageUrl}
              src=${this.mainImageUrl}
            />
          </div>
          <div class="main" part="main">
            <h2
              part="header"
              id="header"
              class="header heading-large"
              ?hidden=${!this.headerLabel}
            >
              <span
                data-l10n-id="${this.headerLabel}"
                data-l10n-args="${JSON.stringify(this.headerArgs)}"
              >
              </span>
            </h2>
            <span id="description">
              ${repeat(
                this.descriptionLabels,
                descLabel => descLabel,
                (descLabel, index) =>
                  html`<p
                    class=${classMap({
                      description: true,
                      secondary: index !== 0,
                    })}
                    data-l10n-id="${descLabel}"
                    @click=${this.openLinkInParentWindow &&
                    this.linkActionHandler}
                    @keydown=${this.openLinkInParentWindow &&
                    this.linkActionHandler}
                  >
                    ${this.linkTemplate(this.descriptionLink)}
                  </p>`
              )}
            </span>
            <slot name="primary-action"></slot>
          </div>
        </div>
      </card-container>
    `;
  }

  linkActionHandler(e) {
    const shouldNavigate =
      (e.type == "click" && !e.altKey) ||
      (e.type == "keydown" && e.code == "Enter") ||
      (e.type == "keydown" && e.code == "Space");
    if (shouldNavigate && e.target.href) {
      navigateToLink(e, e.target.href);
      e.preventDefault();
    }
  }
}
customElements.define("fxview-empty-state", FxviewEmptyState);
