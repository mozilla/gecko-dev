/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-card.mjs";

const MIN_SHOW_MORE_HEIGHT = 200;
/**
 * A card container to be used in the shopping sidebar. There are three card types.
 * The default type where no type attribute is required and the card will have no extra functionality.
 * The "accordion" type will initially not show any content. The card will contain a arrow to expand the
 * card so all of the content is visible.
 *
 * @property {string} label - The label text that will be used for the card header
 * @property {string} type - (optional) The type of card. No type specified
 *   will be the default card. The other available types are "accordion" and "show-more".
 */
class ShoppingCard extends MozLitElement {
  static properties = {
    label: { type: String },
    type: { type: String },
    rating: { type: String },
    _isExpanded: { type: Boolean },
  };

  static get queries() {
    return {
      contentEl: "#content",
    };
  }

  cardTemplate() {
    if (this.type === "show-more") {
      return html`
        <article
          id="content"
          class="show-more"
          aria-describedby="content"
          expanded="false"
        >
          <slot name="content"></slot>

          <footer>
            <moz-button
              size="small"
              aria-controls="content"
              data-l10n-id="shopping-show-more-button"
              @click=${this.handleShowMoreButtonClick}
            ></moz-button>
          </footer>
        </article>
      `;
    }
    return html`
      <div id="content" aria-describedby="content">
        ${this.headingTemplate()}
        <slot name="content"></slot>
      </div>
    `;
  }

  headingTemplate() {
    if (this.rating) {
      return html`<div id="label-wrapper">
        <span id="heading">${this.label}</span>
        <moz-five-star
          rating="${this.rating === 0 ? 0.5 : this.rating}"
        </moz-five-star>
      </div>`;
    }
    return "";
  }

  onCardToggle(e) {
    const action = e.newState == "open" ? "expanded" : "collapsed";
    let cardId = this.getAttribute("id");
    switch (cardId) {
      case "shopping-settings-label":
        Glean.shopping.surfaceSettingsExpandClicked.record({ action });
        break;
      case "shopping-analysis-explainer-label":
        Glean.shopping.surfaceShowQualityExplainerClicked.record({
          action,
        });
        break;
    }
  }

  handleShowMoreButtonClick(e) {
    this._isExpanded = !this._isExpanded;
    // toggle show more/show less text
    e.target.setAttribute(
      "data-l10n-id",
      this._isExpanded
        ? "shopping-show-less-button"
        : "shopping-show-more-button"
    );
    // toggle content expanded attribute
    this.contentEl.attributes.expanded.value = this._isExpanded;

    let action = this._isExpanded ? "expanded" : "collapsed";
    Glean.shopping.surfaceShowMoreReviewsButtonClicked.record({
      action,
    });
  }

  enableShowMoreButton() {
    this._isExpanded = false;
    this.toggleAttribute("showMoreButtonDisabled", false);
    this.contentEl.attributes.expanded.value = false;
  }

  disableShowMoreButton() {
    this._isExpanded = true;
    this.toggleAttribute("showMoreButtonDisabled", true);
    this.contentEl.attributes.expanded.value = true;
  }

  firstUpdated() {
    if (this.type !== "show-more") {
      return;
    }

    let contentSlot = this.shadowRoot.querySelector("slot[name='content']");
    let contentSlotEls = contentSlot.assignedElements();
    if (!contentSlotEls.length) {
      return;
    }

    let slottedDiv = contentSlotEls[0];

    this.handleContentSlotResize = this.handleContentSlotResize.bind(this);
    this.contentResizeObserver = new ResizeObserver(
      this.handleContentSlotResize
    );
    this.contentResizeObserver.observe(slottedDiv);
  }

  disconnectedCallback() {
    this.contentResizeObserver?.disconnect();
  }

  handleContentSlotResize(entries) {
    for (let entry of entries) {
      if (entry.contentRect.height === 0) {
        return;
      }

      if (entry.contentRect.height < MIN_SHOW_MORE_HEIGHT) {
        this.disableShowMoreButton();
      } else if (this.hasAttribute("showMoreButtonDisabled")) {
        this.enableShowMoreButton();
      }
    }
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/shopping/shopping-card.css"
      />
      <moz-card
        class="shopping-card"
        type=${this.type}
        heading=${ifDefined(
          this.label && !this.rating ? this.label : undefined
        )}
        @toggle=${this.onCardToggle}
      >
        ${this.cardTemplate()}
      </moz-card>
    `;
  }
}
customElements.define("shopping-card", ShoppingCard);
