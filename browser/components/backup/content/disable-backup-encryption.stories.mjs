/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
import "chrome://global/content/elements/moz-card.mjs";
import { ERRORS } from "chrome://browser/content/backup/backup-constants.mjs";
import "./disable-backup-encryption.mjs";

window.MozXULElement.insertFTLIfNeeded("locales-preview/backupSettings.ftl");
window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");

const SELECTABLE_ERRORS = {
  "(none)": 0,
  ...ERRORS,
};

export default {
  title: "Domain-specific UI Widgets/Backup/Disable Encryption",
  component: "disable-backup-encryption",
  argTypes: {
    disableEncryptionErrorCode: {
      options: Object.keys(SELECTABLE_ERRORS),
      mapping: SELECTABLE_ERRORS,
      control: { type: "select" },
    },
  },
};

const Template = ({ disableEncryptionErrorCode }) => html`
  <moz-card style="width: 23.94rem;">
    <disable-backup-encryption
      .disableEncryptionErrorCode=${disableEncryptionErrorCode}
    ></disable-backup-encryption>
  </moz-card>
`;

export const Default = Template.bind({});

export const DisableError = Template.bind({});
DisableError.args = {
  disableEncryptionErrorCode: ERRORS.UNKNOWN,
};
