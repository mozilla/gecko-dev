# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

fxa-toolbar-sync-syncing2 = Syncing…

sync-disconnect-dialog-title2 = Disconnect?
sync-disconnect-dialog-body = { -brand-product-name } will stop syncing your account but won’t delete any of your browsing data on this device.
sync-disconnect-dialog-button = Disconnect

fxa-signout-dialog-title2 = Sign out of your account?
fxa-signout-dialog-body = Synced data will remain in your account.
fxa-signout-dialog2-button = Sign out
fxa-signout-dialog2-checkbox = Delete data from this device (passwords, history, bookmarks, etc.)

fxa-menu-sync-settings =
    .label = Sync settings
fxa-menu-turn-on-sync =
    .value = Turn on sync
fxa-menu-turn-on-sync-default = Turn on sync

fxa-menu-connect-another-device =
    .label = Connect another device…
# Variables:
#   $tabCount (Number): The number of tabs sent to the device.
fxa-menu-send-tab-to-device =
    .label =
        { $tabCount ->
            [1] Send tab to device
           *[other] Send { $tabCount } tabs to device
        }

# This is shown dynamically within "Send tab to device" in fxa menu.
fxa-menu-send-tab-to-device-syncnotready =
    .label = Syncing Devices…

# This is shown within "Send tab to device" in fxa menu if account is not configured.
fxa-menu-send-tab-to-device-description = Send a tab instantly to any device you’re signed in on.

fxa-menu-sign-out =
    .label = Sign out…


fxa-menu-sync-title = Sync
fxa-menu-sync-description = Access your web anywhere


# Dialog strings that we show the user when signing into Mozilla account/setting up sync

sync-setup-verify-continue = Continue
sync-setup-verify-title = Merge Warning
sync-setup-verify-heading = Are you sure you want to sign in to sync?

# The user was previously signed into sync. This dialog confirms to the user
# that they will be merging the data from the previously signed in into the newly signed in one
# Variables:
#   $email - Email address of a user previously signed into sync.
sync-setup-verify-description = A different user was previously signed in to sync on this computer. Signing in will merge this browser’s bookmarks, passwords and other settings with { $email }
