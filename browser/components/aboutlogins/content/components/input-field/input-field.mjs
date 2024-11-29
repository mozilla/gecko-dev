/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";

export const stylesTemplate = () => html`
  <link rel="stylesheet" href="chrome://global/skin/in-content/common.css" />
  <link
    rel="stylesheet"
    href="chrome://browser/content/aboutlogins/components/input-field/input-field.css"
  />
`;

export const editableFieldTemplate = ({
  type,
  value,
  disabled,
  required,
  onFocus,
  onBlur,
  labelL10nId,
  noteL10nId,
}) => html`
  <label
    for="input"
    class="field-label"
    data-l10n-id=${labelL10nId}
    tabindex="-1"
  >
  </label>
  <input
    id="input"
    class="input-field"
    type=${type}
    value=${value}
    aria-describedby="explainer"
    ?disabled=${disabled}
    ?required=${required}
    @focus=${onFocus}
    @blur=${onBlur}
  />
  <span
    id="explainer"
    role="note"
    class="explainer text-deemphasized"
    data-l10n-id=${ifDefined(noteL10nId)}
  ></span>
`;
