/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable jsdoc/check-tag-names */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";
import { MozRadio } from "chrome://global/content/elements/moz-radio-group.mjs";
import { SelectControlBaseElement } from "chrome://global/content/lit-select-control.mjs";

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
export class ProfilesGroup extends SelectControlBaseElement {
  static childElementName = "profiles-group-item";
  static orientation = "horizontal";

  get currentFocus() {
    let focusedIndex = this.childElements.findIndex(
      button => button.itemTabIndex === 0
    );

    if (focusedIndex !== -1) {
      return focusedIndex;
    }

    return this.focusableIndex;
  }

  updateFocusIndex(focusIndex) {
    this.childElements.forEach((button, index) => {
      button.itemTabIndex = focusIndex === index ? 0 : -1;
    });
  }

  navigate(direction) {
    let currentIndex = this.currentFocus;
    let indexStep = this.childElements.length + NAVIGATION_VALUE[direction];

    for (let i = 1; i < this.childElements.length; i++) {
      let nextIndex =
        (currentIndex + indexStep * i) % this.childElements.length;
      let nextButton = this.childElements[nextIndex];
      if (!nextButton.disabled) {
        this.updateFocusIndex(nextIndex);
        this.childElements[nextIndex].focus();
        return;
      }
    }
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
    name: { type: String },
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
          data-l10n-id=${ifDefined(this.l10nId)}
          >${this.name}</label
        >${super.inputTemplate()}
        <slot></slot>
      </div>
    `;
  }
}
customElements.define("profiles-group-item", ProfilesGroupItem);
