/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * Class for displaying the adjusted ratings for a given product.
 */
class AdjustedRating extends MozLitElement {
  static properties = {
    rating: { type: Number, reflect: true },
  };

  render() {
    if (!this.rating && this.rating !== 0) {
      this.hidden = true;
      return null;
    }

    this.hidden = false;

    return html`
      <shopping-card
        data-l10n-id="shopping-adjusted-rating-label"
        data-l10n-attrs="label"
        rating=${this.rating}
      >
        <div slot="content">
          <span
            data-l10n-id="shopping-adjusted-rating-based-reliable-reviews"
          ></span>
        </div>
      </shopping-card>
    `;
  }
}

customElements.define("adjusted-rating", AdjustedRating);
