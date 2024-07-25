/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
import "chrome://global/content/elements/moz-card.mjs";
import "./password-validation-inputs.mjs";

window.MozXULElement.insertFTLIfNeeded("locales-preview/backupSettings.ftl");
window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");

export default {
  title: "Domain-specific UI Widgets/Backup/Password Inputs",
  component: "password-validation-inputs",
  argTypes: {},
};

const Template = () => html`
  <moz-card style="position: relative; width: 25rem;">
    <password-validation-inputs></password-validation-inputs>
  </moz-card>
`;

export const Default = Template.bind({});
