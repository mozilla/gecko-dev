# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

### These strings appear in the Unified Extensions panel.

## Panel

unified-extensions-header-title = Extensions
unified-extensions-manage-extensions =
    .label = Manage extensions

## An extension in the main list

# Each extension in the unified extensions panel (list) has a secondary button
# to open a context menu. This string is used for each of these buttons.
# Variables:
#   $extensionName (String) - Name of the extension
unified-extensions-item-open-menu =
    .aria-label = Open menu for { $extensionName }

unified-extensions-item-message-manage = Manage extension

## Extension's context menu

unified-extensions-context-menu-pin-to-toolbar =
    .label = Pin to Toolbar

unified-extensions-context-menu-manage-extension =
    .label = Manage Extension

unified-extensions-context-menu-remove-extension =
    .label = Remove Extension

unified-extensions-context-menu-report-extension =
    .label = Report Extension

unified-extensions-context-menu-move-widget-up =
    .label = Move Up

unified-extensions-context-menu-move-widget-down =
    .label = Move Down

## Notifications

# .heading is processed by moz-message-bar to be used as a heading attribute
unified-extensions-mb-quarantined-domain-message-3 =
    .heading = Some extensions are not allowed
    .message = To protect your data, some extensions can’t read or change data on this site. Use the extension’s settings to allow on sites restricted by { -vendor-short-name }.

unified-extensions-mb-quarantined-domain-learn-more = Learn more
    .aria-label = Learn more: Some extensions are not allowed

unified-extensions-mb-about-addons-link = Go to extension settings

# Variables:
#   $extensionName (String) - Name of the extension disabled through a soft-block.
unified-extensions-mb-blocklist-warning-single =
    .heading = { $extensionName } disabled
    .message =
        This extension violates Mozilla’s policies and has been disabled.
        You can enable it in settings, but this may be risky.

# Variables:
#   $extensionName (String) - Name of the extension disabled through a hard-block.
unified-extensions-mb-blocklist-error-single =
    .heading = { $extensionName } disabled
    .message =
        This extension violates Mozilla’s policies and has been disabled.

# Variables:
#   $extensionsCount (Number) - Number of extensions disabled through both soft and hard-blocks (always going to be greater than 1)
unified-extensions-mb-blocklist-warning-multiple =
    .heading =
        { $extensionsCount ->
            *[other] { $extensionsCount } extensions disabled
        }
    .message =
        Some of your extensions have been disabled for violating Mozilla’s policies.
        You can enable them in settings, but this may be risky.

# Variables:
#   $extensionsCount (Number) - Number of extensions disabled through hard-blocks.
unified-extensions-mb-blocklist-error-multiple =
    .heading =
        { $extensionsCount ->
            *[other] { $extensionsCount } extensions disabled
        }
    .message =
        Some of your extensions have been disabled for violating Mozilla’s policies.
