/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1878024 - g1.globo.com squashed account icons
 *
 * Account icons appear to be squashed due to bug 1700474 with flex-sizing.
 * This works around the issue by adding CSS to the relevant web component.
 */

/* globals exportFunction */

const ENABLED_MESSAGE =
  "Extra CSS to fix account icons is being applied to the nova-barra-globocom web component. See https://bugzilla.mozilla.org/show_bug.cgi?id=1878024 for details.";

const { appendChild } = DocumentFragment.prototype.wrappedJSObject;

ShadowRoot.wrappedJSObject.prototype.appendChild = exportFunction(function (
  child
) {
  if (this.host?.localName === "nova-barra-globocom") {
    const style = child.querySelector("style");
    if (style) {
      console.info(ENABLED_MESSAGE);
      style.textContent = `${style.textContent} .base-container .button-login-icon img { flex-shrink: 0; }`;
    }
    delete ShadowRoot.wrappedJSObject.prototype.appendChild;
  }
  return appendChild.call(this, child);
}, window);
