/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "./vendor/lit.all.mjs";
import { MozLitElement } from "./lit-utils.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-fieldset.mjs";

const NAVIGATION_FORWARD = "forward";
const NAVIGATION_BACKWARD = "backward";

const NAVIGATION_VALUE = {
  [NAVIGATION_FORWARD]: 1,
  [NAVIGATION_BACKWARD]: -1,
};

const DIRECTION_RIGHT = "Right";
const DIRECTION_LEFT = "Left";

const NAVIGATION_DIRECTIONS = {
  LTR: {
    FORWARD: DIRECTION_RIGHT,
    BACKWARD: DIRECTION_LEFT,
  },
  RTL: {
    FORWARD: DIRECTION_LEFT,
    BACKWARD: DIRECTION_RIGHT,
  },
};

/**
 * Class that can be extended to handle managing the selected and focus states
 * of child elements using a roving tabindex. For more information on this focus
 * management pattern, see:
 * https://www.w3.org/WAI/ARIA/apg/practices/keyboard-interface/#kbd_roving_tabindex
 *
 * Child elements must use SelectControlItemMixin for behavior to work as
 * expected.
 */
export class SelectControlBaseElement extends MozLitElement {
  #childElements;
  #value;

  static properties = {
    disabled: { type: Boolean, reflect: true },
    description: { type: String, fluent: true },
    supportPage: { type: String, attribute: "support-page" },
    label: { type: String, fluent: true },
    name: { type: String },
    value: { type: String },
  };

  static queries = {
    fieldset: "moz-fieldset",
  };

  set value(newValue) {
    this.#value = newValue;
    this.childElements.forEach(item => {
      item.checked = this.value === item.value;
    });
    this.syncFocusState();
  }

  get value() {
    return this.#value;
  }

  get focusableIndex() {
    if (this.#value) {
      let selectedIndex = this.childElements.findIndex(
        item => item.value === this.#value && !item.disabled
      );
      if (selectedIndex !== -1) {
        return selectedIndex;
      }
    }
    return this.childElements.findIndex(item => !item.disabled);
  }

  // Query for child elements the first time they are needed + ensure they
  // have been upgraded so we can access properties.
  get childElements() {
    if (!this.#childElements) {
      this.#childElements = (
        this.shadowRoot
          ?.querySelector("slot:not([name])")
          ?.assignedElements() || [...this.children]
      )?.filter(
        el => el.localName === this.constructor.childElementName && !el.slot
      );
      this.#childElements.forEach(item => customElements.upgrade(item));
    }
    return this.#childElements;
  }

  constructor() {
    super();
    this.disabled = false;
    this.addEventListener("keydown", e => this.handleKeydown(e));
  }

  firstUpdated() {
    this.syncStateToChildElements();
  }

  async getUpdateComplete() {
    await super.getUpdateComplete();
    await Promise.all(this.childElements.map(item => item.updateComplete));
  }

  syncStateToChildElements() {
    this.childElements.forEach(item => {
      if (item.checked && this.value == undefined) {
        this.value = item.value;
      }
      item.name = this.name;
    });
    this.syncFocusState();
  }

  syncFocusState() {
    let focusableIndex = this.focusableIndex;
    this.childElements.forEach((item, index) => {
      item.itemTabIndex = focusableIndex === index ? 0 : -1;
    });
  }

  // NB: We may need to revise this to avoid bugs when we add more focusable
  // elements to select control base/items.
  handleKeydown(event) {
    let directions = this.getNavigationDirections();
    switch (event.key) {
      case "Down":
      case "ArrowDown":
      case directions.FORWARD:
      case `Arrow${directions.FORWARD}`: {
        event.preventDefault();
        this.navigate(NAVIGATION_FORWARD);
        break;
      }
      case "Up":
      case "ArrowUp":
      case directions.BACKWARD:
      case `Arrow${directions.BACKWARD}`: {
        event.preventDefault();
        this.navigate(NAVIGATION_BACKWARD);
        break;
      }
    }
  }

  getNavigationDirections() {
    if (this.isDocumentRTL) {
      return NAVIGATION_DIRECTIONS.RTL;
    }
    return NAVIGATION_DIRECTIONS.LTR;
  }

  get isDocumentRTL() {
    if (typeof Services !== "undefined") {
      return Services.locale.isAppLocaleRTL;
    }
    return document.dir === "rtl";
  }

  navigate(direction) {
    let currentIndex = this.focusableIndex;
    let children = this.childElements;
    let indexStep = children.length + NAVIGATION_VALUE[direction];

    for (let i = 1; i < children.length; i++) {
      let nextIndex = (currentIndex + indexStep * i) % children.length;
      let nextItem = children[nextIndex];
      if (!nextItem.disabled) {
        this.value = nextItem.value;
        nextItem.focus();
        this.dispatchEvent(new Event("input"), {
          bubbles: true,
          composed: true,
        });
        this.dispatchEvent(new Event("change"), { bubbles: true });
        return;
      }
    }
  }

  willUpdate(changedProperties) {
    if (changedProperties.has("name")) {
      this.handleSetName();
    }
    if (changedProperties.has("disabled")) {
      this.childElements.forEach(item => {
        item.requestUpdate();
      });
    }
  }

  handleSetName() {
    this.childElements.forEach(item => {
      item.name = this.name;
    });
  }

  // Re-dispatch change event so it's re-targeted to the custom element.
  handleChange(event) {
    event.stopPropagation();
    this.dispatchEvent(new Event(event.type));
  }

  handleSlotChange() {
    this.#childElements = null;
    this.syncStateToChildElements();
  }

  render() {
    return html`
      <moz-fieldset
        part="fieldset"
        description=${ifDefined(this.description)}
        support-page=${ifDefined(this.supportPage)}
        role="radiogroup"
        ?disabled=${this.disabled}
        label=${this.label}
        exportparts="inputs, support-link"
        aria-orientation=${ifDefined(this.constructor.orientation)}
      >
        ${!this.supportPage
          ? html`<slot slot="support-link" name="support-link"></slot>`
          : ""}
        <slot
          @slotchange=${this.handleSlotChange}
          @change=${this.handleChange}
        ></slot>
      </moz-fieldset>
    `;
  }
}

/**
 * Class that can be extended by items nested in a subclass of
 * SelectControlBaseElement to handle selection, focus management, and keyboard
 * navigation. Implemented as a mixin to enable use with elements that inherit
 * from something other than MozLitElement.
 *
 * @param {LitElement} superClass
 * @returns LitElement
 */
export const SelectControlItemMixin = superClass =>
  class extends superClass {
    #controller;

    static properties = {
      name: { type: String },
      value: { type: String },
      disabled: { type: Boolean, reflect: true },
      checked: { type: Boolean, reflect: true },
      itemTabIndex: { type: Number, state: true },
    };

    get controller() {
      return this.#controller;
    }

    get isDisabled() {
      return this.disabled || this.#controller.disabled;
    }

    constructor() {
      super();
      this.checked = false;
    }

    connectedCallback() {
      super.connectedCallback();

      let hostElement = this.parentElement || this.getRootNode().host;
      if (!(hostElement instanceof SelectControlBaseElement)) {
        console.error(
          `${this.localName} should only be used in an element that extends SelectControlBaseElement.`
        );
      }

      this.#controller = hostElement;
      if (this.#controller.value) {
        this.checked = this.value === this.#controller.value;
      }
    }

    willUpdate(changedProperties) {
      super.willUpdate(changedProperties);
      // Handle setting checked directly via JS.
      if (
        changedProperties.has("checked") &&
        this.checked &&
        this.#controller.value &&
        this.value !== this.#controller.value
      ) {
        this.#controller.value = this.value;
      }
      // Handle un-checking directly via JS. If the checked item is un-checked,
      // the value of the associated focus manager parent needs to be un-set.
      if (
        changedProperties.has("checked") &&
        !this.checked &&
        this.#controller.value &&
        this.value === this.#controller.value
      ) {
        this.#controller.value = "";
      }

      if (changedProperties.has("disabled")) {
        // Prevent enabling a items if containing focus manager is disabled.
        if (this.disabled === false && this.#controller.disabled) {
          this.disabled = true;
        } else if (this.checked || !this.#controller.value) {
          // Update items via focus manager parent for proper keyboard nav behavior.
          this.#controller.syncFocusState();
        }
      }
    }

    handleClick() {
      if (this.isDisabled || this.checked) {
        return;
      }

      this.#controller.value = this.value;
      if (this.getRootNode().activeElement?.localName == this.localName) {
        this.focus();
      }
    }

    // Re-dispatch change event so it propagates out of the element.
    handleChange(e) {
      this.dispatchEvent(new Event(e.type, e));
    }
  };
