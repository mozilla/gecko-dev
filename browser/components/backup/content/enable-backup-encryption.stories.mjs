/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
import "chrome://global/content/elements/moz-card.mjs";
import "./enable-backup-encryption.mjs";

window.MozXULElement.insertFTLIfNeeded("locales-preview/backupSettings.ftl");
window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");

export default {
  title: "Domain-specific UI Widgets/Backup/Enable Encryption",
  component: "enable-backup-encryption",
  argTypes: {
    type: {
      control: { type: "select" },
      options: ["set-password", "change-password"],
    },
  },
};

const Template = ({ type }) => html`
  <moz-card style="width: 23.94rem; position: relative;">
    <enable-backup-encryption type=${type}></enable-backup-encryption>
  </moz-card>
`;

export const SetPassword = Template.bind({});
SetPassword.args = {
  type: "set-password",
};

export const ChangePassword = Template.bind({});
ChangePassword.args = {
  type: "change-password",
};
