/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import MozInputText from "chrome://global/content/elements/moz-input-text.mjs";

/**
 * A password input custom element.
 *
 * @tagname moz-input-password
 * @property {string} label - The text of the label element
 * @property {string} name - The name of the input control
 * @property {string} value - The value of the input control
 * @property {boolean} disabled - The disabled state of the input control
 * @property {boolean} readonly - The readonly state of the input control
 * @property {string} iconSrc - The src for an optional icon
 * @property {string} description - The text for the description element that helps describe the input control
 * @property {string} supportPage - Name of the SUMO support page to link to.
 * @property {string} placeholder - Text to display when the input has no value.
 */
export default class MozInputPassword extends MozInputText {
  inputTemplate() {
    return super.inputTemplate({ type: "password" });
  }
}
customElements.define("moz-input-password", MozInputPassword);
