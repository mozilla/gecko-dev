/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "../vendor/lit.all.mjs";
import { MozLitElement, MozBaseInputElement } from "../lit-utils.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-label.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-fieldset.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "chrome://global/content/elements/moz-support-link.mjs";

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
 * Element used to group and associate moz-radio buttons so that they function
 * as a single form-control element.
 *
 * @tagname moz-radio-group
 * @property {boolean} disabled - Whether or not the fieldset is disabled.
 * @property {string} label - Label for the group of moz-radio elements.
 * @property {string} description - Description for the group of moz-radio elements.
 * @property {string} supportPage - Support page for the group of moz-radio elements.
 * @property {string} name
 *  Input name of the radio group. Propagates to moz-radio children.
 * @property {string} value
 *  Selected value for the group. Changing the value updates the checked
 *  state of moz-radio children and vice versa.
 * @slot default - The radio group's content, intended for moz-radio elements.
 * @slot support-link - The radio group's support link intended for moz-radio elements.
 */
export class MozRadioGroup extends MozLitElement {
  #radioButtons = [];
  #value;

  static properties = {
    disabled: { type: Boolean, reflect: true },
    description: { type: String, fluent: true },
    supportPage: { type: String, attribute: "support-page" },
    label: { type: String, fluent: true },
    name: { type: String },
  };

  static queries = {
    defaultSlot: "slot:not([name])",
    fieldset: "moz-fieldset",
  };

  set value(newValue) {
    this.#value = newValue;
    this.#radioButtons.forEach(button => {
      button.checked = this.value === button.value;
    });
    this.syncFocusState();
  }

  get value() {
    return this.#value;
  }

  get focusableIndex() {
    if (this.#value) {
      let selectedIndex = this.#radioButtons.findIndex(
        button => button.value === this.#value && !button.disabled
      );
      if (selectedIndex !== -1) {
        return selectedIndex;
      }
    }
    return this.#radioButtons.findIndex(button => !button.disabled);
  }

  constructor() {
    super();
    this.disabled = false;
    this.addEventListener("keydown", e => this.handleKeydown(e));
  }

  firstUpdated() {
    this.syncStateToRadioButtons();
  }

  async getUpdateComplete() {
    await super.getUpdateComplete();
    await Promise.all(this.#radioButtons.map(button => button.updateComplete));
  }

  syncStateToRadioButtons() {
    this.#radioButtons = this.defaultSlot
      ?.assignedElements()
      .filter(el => el.localName === "moz-radio");

    this.#radioButtons.forEach(button => {
      if (button.checked && this.value == undefined) {
        this.value = button.value;
      }
      button.name = this.name;
    });
    this.syncFocusState();
  }

  syncFocusState() {
    let focusableIndex = this.focusableIndex;
    this.#radioButtons.forEach((button, index) => {
      button.inputTabIndex = focusableIndex === index ? 0 : -1;
    });
  }

  // NB: We may need to revise this to avoid bugs when we add more focusable
  // elements to moz-radio-group / moz-radio.
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
    let indexStep = this.#radioButtons.length + NAVIGATION_VALUE[direction];

    for (let i = 1; i < this.#radioButtons.length; i++) {
      let nextIndex =
        (currentIndex + indexStep * i) % this.#radioButtons.length;
      if (!this.#radioButtons[nextIndex].disabled) {
        this.#radioButtons[nextIndex].click();
        return;
      }
    }
  }

  willUpdate(changedProperties) {
    if (changedProperties.has("name")) {
      this.handleSetName();
    }
    if (changedProperties.has("disabled")) {
      this.#radioButtons.forEach(button => {
        button.requestUpdate();
      });
    }
  }

  handleSetName() {
    this.#radioButtons.forEach(button => {
      button.name = this.name;
    });
  }

  // Re-dispatch change event so it's re-targeted to moz-radio-group.
  handleChange(event) {
    event.stopPropagation();
    this.dispatchEvent(new Event(event.type));
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
      >
        ${!this.supportPage
          ? html`<slot slot="support-link" name="support-link"></slot>`
          : ""}
        <slot
          @slotchange=${this.syncStateToRadioButtons}
          @change=${this.handleChange}
        ></slot>
      </moz-fieldset>
    `;
  }
}
customElements.define("moz-radio-group", MozRadioGroup);

/**
 * Input element that allows a user to select one option from a group of options.
 *
 * @tagname moz-radio
 * @property {boolean} checked - Whether or not the input is selected.
 * @property {string} description - Description for the input.
 * @property {boolean} disabled - Whether or not the input is disabled.
 * @property {string} iconSrc - Path to an icon displayed next to the input.
 * @property {number} inputTabIndex - Tabindex of the input element.
 * @property {string} label - Label for the radio input.
 * @property {string} name
 *  Name of the input control, set by the associated moz-radio-group element.
 * @property {string} supportPage - Name of the SUMO support page to link to.
 * @property {number} value - Value of the radio input.
 */
export class MozRadio extends MozBaseInputElement {
  #controller;

  static properties = {
    checked: { type: Boolean, reflect: true },
    inputTabIndex: { type: Number, state: true },
  };

  constructor() {
    super();
    this.checked = false;
  }

  connectedCallback() {
    super.connectedCallback();

    let hostRadioGroup = this.parentElement || this.getRootNode().host;
    if (!(hostRadioGroup instanceof MozRadioGroup)) {
      console.error("moz-radio can only be used in moz-radio-group element.");
    }

    this.#controller = hostRadioGroup;
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
    // Handle un-checking directly via JS. If the checked input is un-checked,
    // the value of the associated moz-radio-group needs to be un-set.
    if (
      changedProperties.has("checked") &&
      !this.checked &&
      this.#controller.value &&
      this.value === this.#controller.value
    ) {
      this.#controller.value = "";
    }

    if (changedProperties.has("disabled")) {
      // Prevent enabling a radio button if containing radio-group is disabled.
      if (this.disabled === false && this.#controller.disabled) {
        this.disabled = true;
      } else if (this.checked || !this.#controller.value) {
        // Update buttons via moz-radio-group for proper keyboard nav behavior.
        this.#controller.syncFocusState();
      }
    }
  }

  handleClick() {
    this.#controller.value = this.value;
    this.focus();
  }

  // Re-dispatch change event so it propagates out of moz-radio.
  handleChange(e) {
    this.dispatchEvent(new Event(e.type, e));
  }

  inputTemplate() {
    return html`<input
      type="radio"
      id="input"
      value=${this.value}
      name=${this.name}
      .checked=${this.checked}
      aria-checked=${this.checked}
      aria-describedby="description"
      tabindex=${this.inputTabIndex}
      ?disabled=${this.disabled || this.#controller.disabled}
      accesskey=${ifDefined(this.accessKey)}
      @click=${this.handleClick}
      @change=${this.handleChange}
    />`;
  }
}
customElements.define("moz-radio", MozRadio);
