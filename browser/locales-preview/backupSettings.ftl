# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This string is used to name the folder that users will save backups to.
# "Restore" is an action and intended for prompting users to select this folder
# when following backup restoration steps. Please only include characters that
# can be used for folders. Invalid characters will be automatically stripped out
# or replaced with underscores.
backup-folder-name = Restore { -brand-product-name }

# This string is used for the generated file that will be stored within the
# backup-folder-name folder. It will have the profile name and an encoding of
# the backup date appended to it, followed by `.html`. Please only include
# characters that can be used for filenames. Invalid characters will be
# automatically stripped out or replaced with underscores.
#
# This is an example of what the final filename might look like after the
# profile name and backup date are appended to it:
#
# FirefoxBackup_default_20240606-1830.html
backup-file-name = { -brand-product-name }Backup

settings-data-backup-header = Backup
settings-data-backup-toggle = Manage backup
settings-data-backup-scheduled-backups-on = Backup: ON
settings-data-backup-scheduled-backups-off = Backup: OFF
settings-data-backup-scheduled-backups-description = Automatically protect your bookmarks, history, and other data. <a data-l10n-name="support-link">Learn more</a>
settings-data-backup-last-backup-date = Last backup: { DATETIME($date, timeStyle: "short") }, { DATETIME($date, dateStyle: "short") }
# "Location" refers to the folder where backups are being written to.
settings-data-backup-last-backup-location = Location
settings-data-backup-last-backup-location-show-in-folder = Show in folder
settings-data-backup-last-backup-location-edit = Edit…

# Variables:
#   $fileName (String) - The file name of the last backup that was created.
settings-data-backup-last-backup-filename = Filename: { $fileName }

settings-data-backup-restore-header = Restore your data

## These strings are shown under the header if scheduled backups are disabled.

settings-data-backup-scheduled-backups-off-restore-description = Use a { -brand-product-name } backup from another device to restore your data.
settings-data-backup-scheduled-backups-off-restore-choose = Choose backup file…

## These strings are shown under the header if scheduled backups are enabled.

settings-data-backup-scheduled-backups-on-restore-description = Recover your { -brand-product-name } data back from the last time it was backed up.
settings-data-backup-scheduled-backups-on-restore-choose = Restore…

settings-data-toggle-encryption-label = Back up your sensitive data
settings-data-toggle-encryption-description = Back up your passwords, payment methods, and cookies with encryption.
settings-data-toggle-encryption-support-link = Learn more

settings-data-change-password = Change password…

## These strings are displayed in a modal when users want to turn on scheduled backups.

turn-on-scheduled-backups-header = Turn on backup
turn-on-scheduled-backups-description = { -brand-short-name } will create a snapshot of your data every 60 minutes. You can restore it if there’s a problem or you get a new device.
turn-on-scheduled-backups-support-link = What will be backed up?

# "Location" refers to the save location or a folder where users want backups stored.
turn-on-scheduled-backups-location-label = Location
# Variables:
#   $recommendedFolder (String) - Name of the recommended folder for saving backups
turn-on-scheduled-backups-location-default-folder =
    .value = { $recommendedFolder } (recommended)
turn-on-scheduled-backups-location-choose-button =
    { PLATFORM() ->
        [macos] Choose…
        *[other] Browse…
    }

turn-on-scheduled-backups-encryption-label = Back up your sensitive data
turn-on-scheduled-backups-encryption-description = Back up your passwords, payment methods, and cookies with encryption.
turn-on-scheduled-backups-encryption-create-password-label = Password
# Users will be prompted to re-type a password, to ensure that the password is entered correctly.
turn-on-scheduled-backups-encryption-repeat-password-label = Repeat password

turn-on-scheduled-backups-cancel-button = Cancel
turn-on-scheduled-backups-confirm-button = Turn on backup

## These strings are displayed in a modal when users want to turn off scheduled backups.

turn-off-scheduled-backups-header = Turn off backup?
turn-off-scheduled-backups-description = This also deletes all of your backup data. It can’t be undone.
turn-off-scheduled-backups-support-link = Learn more

turn-off-scheduled-backups-cancel-button = Cancel
turn-off-scheduled-backups-confirm-button = Turn off and delete backup

## These strings are displayed in a modal when users want restore from a backup.

restore-from-backup-header = Restore your data
# Variables:
#   $date (string) - Date to be formatted based on locale
restore-from-backup-description-with-metadata = { -brand-short-name } will replace all your current data with your backup from { DATETIME($date, timeStyle: "short") }, { DATETIME($date, dateStyle: "short") }.
restore-from-backup-support-link = What will be restored?

restore-from-backup-filepicker-label = Backup file
restore-from-backup-filepicker-title = Choose Backup File:
restore-from-backup-file-choose-button =
    { PLATFORM() ->
        [macos] Choose…
        *[other] Browse…
    }
restore-from-backup-password-label = Password
restore-from-backup-password-description = This unlocks your encrypted backup.

restore-from-backup-cancel-button = Cancel
restore-from-backup-confirm-button = Restore and restart
restore-from-backup-restoring-button = Restoring…

## These strings are displayed in a modal when users want to enable encryption or change the password for an existing backup.

enable-backup-encryption-header = Back up your sensitive data
enable-backup-encryption-description = Back up your passwords, payment methods, and cookies, plus keep all your data safe with encryption.
enable-backup-encryption-support-link = Learn more

enable-backup-encryption-create-password-label = Password
# Users will be prompted to re-type a password, to ensure that the password is entered correctly.
enable-backup-encryption-repeat-password-label = Repeat password

enable-backup-encryption-cancel-button = Cancel
enable-backup-encryption-confirm-button = Save

change-backup-encryption-header = Change backup password

## These strings are displayed in a tooltip showing what requirements are met while creating a password.

password-rules-header = Password requirements
password-rules-length-description = At least 8 characters
password-rules-email-description = Not your email address
password-rules-common-description = Not a commonly used password
password-rules-disclaimer = Stay safe — don’t reuse passwords. See more tips to <a data-l10n-name="password-support-link">create strong passwords</a>.

## These strings are only used for assistive technologies, like screen readers, in the password requirements tooltip.

password-rules-a11y-success =
    .alt = Success
password-rules-a11y-warning =
    .alt = Warning

## These strings are displayed in a modal when users want to disable encryption for an existing backup.

disable-backup-encryption-header = Remove password protection
disable-backup-encryption-description = Your saved passwords, payment methods, and cookies will no longer be backed up.
disable-backup-encryption-support-link = What will be backed up?

disable-backup-encryption-cancel-button = Cancel
disable-backup-encryption-confirm-button = Remove password

## These strings are inserted into the generated single-file backup archive.
## The single-file backup archive is a specially-crafted, static HTML file
## that is placed within a user specified directory (the Documents folder by
## default) within a folder labelled with the "backup-folder-name" string.

backup-file-header = { -brand-short-name } is ready to be restored
backup-file-title = Restore { -brand-short-name }
backup-file-intro = Get back to browsing and recover all your bookmarks, history, and other data. <a data-l10n-name="backup-file-support-link">Learn more</a>

# Variables:
#   $date (string) - Date to be formatted based on locale
backup-file-last-backed-up = <strong>Last backed up:</strong> { DATETIME($date, timeStyle: "short") }, { DATETIME($date, dateStyle: "short") }

backup-file-encryption-state-encrypted = Encrypted
backup-file-encryption-state-not-encrypted = Not encrypted

# Variables:
#   $machineName (String) - Name of the machine that the backup was created on.
backup-file-creation-device = Created on { $machineName }

backup-file-how-to-restore-header = How to restore your data:
backup-file-moz-browser-restore-step-1 = Go to Settings > Backup
backup-file-moz-browser-restore-step-2 = Under “Restore”, click “Choose backup file”
backup-file-moz-browser-restore-step-3 = Restart { -brand-short-name } when asked

backup-file-other-browser-restore-step-1 = Download and install { -brand-short-name }:
backup-file-download-moz-browser-button = Download { -brand-short-name }
backup-file-other-browser-restore-step-2 = Open { -brand-short-name } and restore your backup

##
