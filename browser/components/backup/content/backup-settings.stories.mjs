/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// eslint-disable-next-line import/no-unresolved
import { html } from "lit.all.mjs";
import "./backup-settings.mjs";

window.MozXULElement.insertFTLIfNeeded("locales-preview/backupSettings.ftl");
window.MozXULElement.insertFTLIfNeeded("branding/brand.ftl");

export default {
  title: "Domain-specific UI Widgets/Backup/Backup Settings",
  component: "backup-settings",
  argTypes: {},
};

const Template = ({ backupServiceState }) => html`
  <backup-settings .backupServiceState=${backupServiceState}></backup-settings>
`;

export const ScheduledBackupsDisabled = Template.bind({});
ScheduledBackupsDisabled.args = {
  backupServiceState: {
    backupDirPath: "/Some/User/Documents",
    defaultParent: {
      path: "/Some/User/Documents",
      fileName: "Documents",
    },
    scheduledBackupsEnabled: false,
  },
};

export const ScheduledBackupsEnabled = Template.bind({});
ScheduledBackupsEnabled.args = {
  backupServiceState: {
    backupDirPath: "/Some/User/Documents",
    defaultParent: {
      path: "/Some/User/Documents",
      fileName: "Documents",
    },
    scheduledBackupsEnabled: true,
  },
};

export const ExistingBackup = Template.bind({});
ExistingBackup.args = {
  backupServiceState: {
    backupDirPath: "/Some/User/Documents",
    defaultParent: {
      path: "/Some/User/Documents",
      fileName: "Documents",
    },
    scheduledBackupsEnabled: true,
    lastBackupDate: 1719625747,
    lastBackupFileName: "FirefoxBackup_default_123123123.html",
  },
};

export const EncryptionEnabled = Template.bind({});
EncryptionEnabled.args = {
  backupServiceState: {
    backupDirPath: "/Some/User/Documents",
    defaultParent: {
      path: "/Some/User/Documents",
      fileName: "Documents",
    },
    scheduledBackupsEnabled: true,
    encryptionEnabled: true,
    lastBackupDate: 1719625747,
    lastBackupFileName: "FirefoxBackup_default_123123123.html",
  },
};
