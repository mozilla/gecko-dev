/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html, ifDefined } from "lit.all.mjs";
import "chrome://global/content/elements/moz-card.mjs";
import { ERRORS } from "chrome://browser/content/backup/backup-constants.mjs";
import "./turn-on-scheduled-backups.mjs";

window.MozXULElement.insertFTLIfNeeded("locales-preview/backupSettings.ftl");
window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");

const SELECTABLE_ERRORS = {
  "(none)": 0,
  ...ERRORS,
};

export default {
  title: "Domain-specific UI Widgets/Backup/Turn On Scheduled Backups",
  component: "turn-on-scheduled-backups",
  argTypes: {
    enableBackupErrorCode: {
      options: Object.keys(SELECTABLE_ERRORS),
      mapping: SELECTABLE_ERRORS,
      control: { type: "select" },
    },
  },
};

const Template = ({
  defaultPath,
  _newPath,
  defaultLabel,
  _newLabel,
  enableBackupErrorCode,
}) => html`
  <moz-card style="width: 27.8rem; position: relative;">
    <turn-on-scheduled-backups
      defaultPath=${defaultPath}
      _newPath=${ifDefined(_newPath)}
      defaultLabel=${defaultLabel}
      _newLabel=${ifDefined(_newLabel)}
      .enableBackupErrorCode=${enableBackupErrorCode}
    ></turn-on-scheduled-backups>
  </moz-card>
`;

export const Default = Template.bind({});
Default.args = {
  defaultPath: "/Some/User/Documents",
  defaultLabel: "Documents",
};

export const CustomLocation = Template.bind({});
CustomLocation.args = {
  ...Default.args,
  _newPath: "/Some/Test/Custom/Dir",
  _newLabel: "Dir",
};

export const EnableError = Template.bind({});
EnableError.args = {
  ...CustomLocation.args,
  enableBackupErrorCode: ERRORS.FILE_SYSTEM_ERROR,
};
