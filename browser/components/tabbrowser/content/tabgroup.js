/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  class MozTabbrowserTabGroup extends MozXULElement {
    static markup = `
      <label class="tab-group-label" crop="end"/>
      <html:slot/>
      `;

    constructor() {
      super();
    }

    static get inheritedAttributes() {
      return {
        ".tab-group-label": "value=label,tooltiptext=label",
      };
    }

    connectedCallback() {
      if (this._initialized) {
        return;
      }

      this.textContent = "";
      this.appendChild(this.constructor.fragment);
      this.initializeAttributeInheritance();
      this._initialized = true;
    }

    get color() {
      return this.style.getProperty("--tab-group-color");
    }

    set color(val) {
      this.style.setProperty("--tab-group-color", val);
    }

    get id() {
      return this.getAttribute("id");
    }

    set id(val) {
      this.setAttribute("id", val);
    }

    get label() {
      return this.getAttribute("label");
    }

    set label(val) {
      this.setAttribute("label", val);
    }
  }

  customElements.define("tab-group", MozTabbrowserTabGroup);
}
