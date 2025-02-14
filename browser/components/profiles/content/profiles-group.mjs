/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable jsdoc/check-tag-names */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import {
  MozRadioGroup,
  MozRadio,
} from "chrome://global/content/elements/moz-radio-group.mjs";

const NAVIGATION_FORWARD = "forward";
const NAVIGATION_BACKWARD = "backward";

const NAVIGATION_VALUE = {
  [NAVIGATION_FORWARD]: 1,
  [NAVIGATION_BACKWARD]: -1,
};

/**
 * Element used to group and associate profiles-group-item buttons so that they function
 * as a single form-control element.
 *
 * @tagname profiles-group
 * @property {string} label - Label for the group of profiles-group-item elements.
 * @property {string} description - Description for the group of profiles-group-item elements.
 * @property {string} name
 *  Input name of the radio group. Propagates to profiles-group-item children.
 * @property {string} value
 *  Selected value for the group. Changing the value updates the checked
 *  state of profiles-group-item children and vice versa.
 * @slot default - The radio group's content, intended for profiles-group-item elements.
 */
export class ProfilesGroup extends MozRadioGroup {
  #radioButtons;

  get currentFocus() {
    let focusedIndex = this.radioButtons.findIndex(
      button => button.inputTabIndex === 0
    );

    if (focusedIndex !== -1) {
      return focusedIndex;
    }

    return this.focusableIndex;
  }

  // Query for child elements the first time they are needed + ensure they
  // have been upgraded so we can access properties.
  get radioButtons() {
    if (!this.#radioButtons) {
      this.#radioButtons = (
        this.shadowRoot
          ?.querySelector("slot:not([name])")
          ?.assignedElements() || [...this.children]
      )?.filter(el => el.localName === "profiles-group-item" && !el.slot);
      this.#radioButtons.forEach(button => customElements.upgrade(button));
    }
    return this.#radioButtons;
  }

  updateFocusIndex(focusIndex) {
    this.radioButtons.forEach((button, index) => {
      button.inputTabIndex = focusIndex === index ? 0 : -1;
    });
  }

  navigate(direction) {
    let currentIndex = this.currentFocus;
    let indexStep = this.radioButtons.length + NAVIGATION_VALUE[direction];

    for (let i = 1; i < this.radioButtons.length; i++) {
      let nextIndex = (currentIndex + indexStep * i) % this.radioButtons.length;
      let nextButton = this.radioButtons[nextIndex];
      if (!nextButton.disabled) {
        this.updateFocusIndex(nextIndex);
        this.radioButtons[nextIndex].focus();
        return;
      }
    }
  }

  handleSlotChange() {
    this.#radioButtons = null;
    this.syncStateToRadioButtons();
  }
}
customElements.define("profiles-group", ProfilesGroup);

/**
 * This element wraps any element to function as a radio element.
 */
export class ProfilesGroupItem extends MozRadio {
  #controller;

  static properties = {
    l10nId: { type: String },
  };

  connectedCallback() {
    super.connectedCallback();
    this.#controller = super.controller;
  }

  handleClick() {
    this.#controller.value = this.value;
    this.focus();
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        href="chrome://browser/content/profiles/profiles-group-item.css"
      />
      <div class="wrapper" @click=${this.handleClick} ?checked=${this.checked}>
        <label
          is="moz-label"
          part="label"
          for="input"
          data-l10n-id=${this.l10nId}
        ></label
        >${super.inputTemplate()}
        <slot></slot>
      </div>
    `;
  }
}
customElements.define("profiles-group-item", ProfilesGroupItem);
