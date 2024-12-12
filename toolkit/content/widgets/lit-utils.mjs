/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  LitElement,
  html,
  ifDefined,
} from "chrome://global/content/vendor/lit.all.mjs";

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
 * Mapped properties support (moving a standard attribute to rendered content)
 *
 * When you want to accept a standard attribute such as accesskey, title or
 * aria-label at the component level but it should really be set on a child
 * element then you can set the `mapped: true` option in your property
 * definition and the attribute will be removed from the host when it is set.
 * Note that the attribute can not be unset once it is set.
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
  #l10nObj;
  #l10nRootConnected = false;

  static createProperty(attrName, options) {
    if (options.mapped) {
      let domAttrPropertyName = `${attrName}Attribute`;
      let domAttrName = options.attribute ?? attrName.toLowerCase();
      if (attrName.startsWith("aria")) {
        domAttrName = domAttrName.replace("aria", "aria-");
      }
      this.mappedAttributes ??= [];
      this.mappedAttributes.push([attrName, domAttrPropertyName]);
      options.state = true;
      super.createProperty(domAttrPropertyName, {
        type: String,
        attribute: domAttrName,
        reflect: true,
      });
    }
    if (options.fluent) {
      this.fluentProperties ??= [];
      this.fluentProperties.push(options.attribute || attrName.toLowerCase());
    }
    return super.createProperty(attrName, options);
  }

  constructor() {
    super();
    let { queries } = this.constructor;
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
  }

  connectedCallback() {
    super.connectedCallback();
    if (
      this.renderRoot == this.shadowRoot &&
      !this.#l10nRootConnected &&
      this.#l10n
    ) {
      this.#l10n.connectRoot(this.renderRoot);
      this.#l10nRootConnected = true;

      if (this.constructor.fluentProperties?.length) {
        this.dataset.l10nAttrs = this.constructor.fluentProperties.join(",");
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
      this.#l10nRootConnected &&
      this.#l10n
    ) {
      this.#l10n.disconnectRoot(this.renderRoot);
      this.#l10nRootConnected = false;
    }
  }

  willUpdate(changes) {
    this.#handleMappedAttributeChange(changes);
  }

  #handleMappedAttributeChange(changes) {
    if (!this.constructor.mappedAttributes) {
      return;
    }
    for (let [attrName, domAttrName] of this.constructor.mappedAttributes) {
      if (changes.has(domAttrName)) {
        this[attrName] = this[domAttrName];
        this[domAttrName] = null;
      }
    }
  }

  get #l10n() {
    if (!this.#l10nObj) {
      this.#l10nObj =
        (window.Cu?.isInAutomation && window.mockL10n) || document.l10n;
    }
    return this.#l10nObj;
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

/**
 * A base input element. Provides common layout and properties for our design
 * system input elements.
 *
 * Subclasses must implement the inputTemplate() method which returns the input
 * template for this specific input element with its id set to "input".
 *
 * @property {string} label - The text of the label element
 * @property {string} name - The name of the input control
 * @property {string} value - The value of the input control
 * @property {boolean} disabled - The disabled state of the input control
 * @property {string} iconSrc - The src for an optional icon
 * @property {string} description - The text for the description element that helps describe the input control
 * @property {string} supportPage - Name of the SUMO support page to link to.
 */
export class MozBaseInputElement extends MozLitElement {
  #internals;
  #hasSlottedContent = new Map();

  static properties = {
    label: { type: String, fluent: true },
    name: { type: String },
    value: { type: String },
    iconSrc: { type: String },
    disabled: { type: Boolean, reflect: true },
    description: { type: String, fluent: true },
    supportPage: { type: String, attribute: "support-page" },
    accessKey: { type: String, mapped: true, fluent: true },
  };
  static inputLayout = "inline";

  constructor() {
    super();
    this.disabled = false;
    this.#internals = this.attachInternals();
  }

  connectedCallback() {
    super.connectedCallback();
    this.setAttribute("inputlayout", this.constructor.inputLayout);
  }

  willUpdate(changedProperties) {
    super.willUpdate(changedProperties);
    this.#updateInternalState(this.description, "description");
    this.#updateInternalState(this.supportPage, "support-link");
  }

  #updateInternalState(propVal, slotName) {
    let stateKey = `has-${slotName}`;
    if (propVal || this.#hasSlottedContent.get(slotName)) {
      this.#internals.states.add(stateKey);
    } else {
      this.#internals.states.delete(stateKey);
    }
  }

  get inputEl() {
    return this.renderRoot.getElementById("input");
  }

  get labelEl() {
    return this.renderRoot.querySelector("label");
  }

  get icon() {
    return this.renderRoot.querySelector(".icon");
  }

  get descriptionEl() {
    return this.renderRoot.getElementById("description");
  }

  get hasDescription() {
    return this.#internals.states.has("has-description");
  }

  get hasSupportLink() {
    return this.#internals.states.has("has-support-link");
  }

  click() {
    this.inputEl.click();
  }

  focus() {
    this.inputEl.focus();
  }

  inputTemplate() {
    throw new Error(
      "inputTemplate() must be implemented and provide the input element"
    );
  }

  render() {
    let isInlineLayout = this.constructor.inputLayout == "inline";
    return html`
      <link
        rel="stylesheet"
        href="chrome://global/content/elements/moz-input-common.css"
      />
      <span class="label-wrapper">
        <label
          is="moz-label"
          part="label"
          for="input"
          shownaccesskey=${ifDefined(this.accessKey)}
          >${isInlineLayout
            ? this.inputTemplate()
            : ""}${this.labelTemplate()}</label
        >${this.hasDescription ? "" : this.supportLinkTemplate()}
      </span>
      ${this.descriptionTemplate()}
      ${!isInlineLayout ? this.inputTemplate() : ""}
    `;
  }

  labelTemplate() {
    return html`<span class="label-content"
      >${this.iconTemplate()}<span class="text">${this.label}</span></span
    >`;
  }

  descriptionTemplate() {
    return html`
      <div id="description" class="description text-deemphasized">
        ${this.description ??
        html`<slot
          name="description"
          @slotchange=${this.onSlotchange}
        ></slot>`}${this.hasDescription ? this.supportLinkTemplate() : ""}
      </div>
    `;
  }

  iconTemplate() {
    if (this.iconSrc) {
      return html`<img src=${this.iconSrc} role="presentation" class="icon" />`;
    }
    return "";
  }

  supportLinkTemplate() {
    if (this.supportPage) {
      return html`<a
        is="moz-support-link"
        support-page=${this.supportPage}
        part="support-link"
      ></a>`;
    }
    return html`<slot
      name="support-link"
      @slotchange=${this.onSlotchange}
    ></slot>`;
  }

  onSlotchange(e) {
    let propName = e.target.name;
    let hasSlottedContent = e.target
      .assignedNodes()
      .some(node => node.textContent.trim());

    if (hasSlottedContent == this.#hasSlottedContent.get(propName)) {
      return;
    }

    this.#hasSlottedContent.set(propName, hasSlottedContent);
    this.requestUpdate();
  }
}
