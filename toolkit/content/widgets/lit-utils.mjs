/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { LitElement } from "chrome://global/content/vendor/lit.all.mjs";

/**
 * Helper for our replacement of @query. Used with `static queries` property.
 *
 * https://github.com/lit/lit/blob/main/packages/reactive-element/src/decorators/query.ts
 */
function query(el, selector) {
  return () => el.renderRoot.querySelector(selector);
}

/**
 * Helper for our replacement of @queryAll. Used with `static queries` property.
 *
 * https://github.com/lit/lit/blob/main/packages/reactive-element/src/decorators/query-all.ts
 */
function queryAll(el, selector) {
  return () => el.renderRoot.querySelectorAll(selector);
}

/**
 * MozLitElement provides extensions to the lit-provided LitElement class.
 *
 *******
 *
 * `@query` support (define a getter for a querySelector):
 *
 * static get queries() {
 *   return {
 *     propertyName: ".aNormal .cssSelector",
 *     anotherName: { all: ".selectorFor .querySelectorAll" },
 *   };
 * }
 *
 * This example would add properties that would be written like this without
 * using `queries`:
 *
 * get propertyName() {
 *   return this.renderRoot?.querySelector(".aNormal .cssSelector");
 * }
 *
 * get anotherName() {
 *   return this.renderRoot?.querySelectorAll(".selectorFor .querySelectorAll");
 * }
 *******
 *
 * Automatic Fluent support for shadow DOM.
 *
 * Fluent requires that a shadowRoot be connected before it can use Fluent.
 * Shadow roots will get connected automatically.
 *
 *******
 *
 * Automatic Fluent support for localized Reactive Properties
 *
 * When a Reactive Property can be set by fluent, set `fluent: true` in its
 * property definition and it will automatically be added to the data-l10n-attrs
 * attribute so that fluent will allow setting the attribute.
 *
 *******
 *
 * Test helper for sending events after a change: `dispatchOnUpdateComplete`
 *
 * When some async stuff is going on and you want to wait for it in a test, you
 * can use `this.dispatchOnUpdateComplete(myEvent)` and have the test wait on
 * your event.
 *
 * The component will then wait for your reactive property change to take effect
 * and dispatch the desired event.
 *
 * Example:
 *
 * async onClick() {
 *   let response = await this.getServerResponse(this.data);
 *   // Show the response status to the user.
 *   this.responseStatus = respose.status;
 *   this.dispatchOnUpdateComplete(
 *     new CustomEvent("status-shown")
 *   );
 * }
 *
 * add_task(async testButton() {
 *   let button = this.setupAndGetButton();
 *   button.click();
 *   await BrowserTestUtils.waitForEvent(button, "status-shown");
 * });
 */
export class MozLitElement extends LitElement {
  #l10n;
  #l10nAttrs = [];

  constructor() {
    super();
    this.#l10n =
      (window.Cu?.isInAutomation && window.mockL10n) || document.l10n;
    let { properties, queries } = this.constructor;
    if (queries) {
      for (let [selectorName, selector] of Object.entries(queries)) {
        if (selector.all) {
          Object.defineProperty(this, selectorName, {
            get: queryAll(this, selector.all),
          });
        } else {
          Object.defineProperty(this, selectorName, {
            get: query(this, selector),
          });
        }
      }
    }
    if (properties) {
      for (let [propName, attributes] of Object.entries(properties)) {
        if (attributes.fluent) {
          this.#l10nAttrs.push(attributes.attribute || propName.toLowerCase());
        }
      }
    }
  }

  connectedCallback() {
    super.connectedCallback();
    if (
      this.renderRoot == this.shadowRoot &&
      !this._l10nRootConnected &&
      this.#l10n
    ) {
      this.#l10n.connectRoot(this.renderRoot);
      this._l10nRootConnected = true;

      if (this.#l10nAttrs.length) {
        this.dataset.l10nAttrs = this.#l10nAttrs.join(",");
        if (this.dataset.l10nId) {
          this.#l10n.translateElements([this]);
        }
      }
    }
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    if (
      this.renderRoot == this.shadowRoot &&
      this._l10nRootConnected &&
      this.#l10n
    ) {
      this.#l10n.disconnectRoot(this.renderRoot);
      this._l10nRootConnected = false;
    }
  }

  async dispatchOnUpdateComplete(event) {
    await this.updateComplete;
    this.dispatchEvent(event);
  }

  update() {
    super.update();
    if (this.#l10n) {
      this.#l10n.translateFragment(this.renderRoot);
    }
  }
}
