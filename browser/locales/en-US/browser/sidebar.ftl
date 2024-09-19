# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

menu-view-genai-chat =
  .label = AI Chatbot

menu-view-review-checker =
  .label = Review Checker

## Labels for sidebar history panel

# Variables:
#   $date (string) - Date to be formatted based on locale
sidebar-history-date-today =
  .heading = Today - { DATETIME($date, dateStyle: "full") }
sidebar-history-date-yesterday =
  .heading = Yesterday - { DATETIME($date, dateStyle: "full") }
sidebar-history-date-this-month =
  .heading = { DATETIME($date, dateStyle: "full") }
sidebar-history-date-prev-month =
  .heading = { DATETIME($date, month: "long", year: "numeric") }

sidebar-history-delete =
  .title = Delete from History

sidebar-history-sort-by-date =
  .label = Sort by date
sidebar-history-sort-by-site =
  .label = Sort by site
sidebar-history-clear =
  .label = Clear history

## Labels for sidebar search

# "Search" is a noun (as in "Results of the search for")
# Variables:
#   $query (String) - The search query used for searching through browser history.
sidebar-search-results-header =
  .heading = Search results for “{ $query }”

## Labels for sidebar customize panel

sidebar-customize-extensions-header = Sidebar extensions
sidebar-customize-firefox-tools-header =
  .label = { -brand-product-name } tools
sidebar-customize-firefox-settings = Manage { -brand-short-name } settings
sidebar-position-left =
  .label = Show on the left
sidebar-position-right =
  .label = Show on the right
sidebar-vertical-tabs =
  .label = Vertical tabs
sidebar-horizontal-tabs =
  .label = Horizontal tabs
sidebar-customize-tabs-header =
  .label = Tab settings
sidebar-customize-settings-header =
  .label = Sidebar settings
sidebar-visibility-always-show =
  .label = Always show
sidebar-visibility-hide-sidebar =
  .label = Hide sidebar

## Labels for sidebar context menu items

sidebar-context-menu-manage-extension =
  .label = Manage extension
sidebar-context-menu-remove-extension =
  .label = Remove extension
sidebar-context-menu-report-extension =
  .label = Report extension
sidebar-context-menu-open-in-window =
  .label = Open in New Window
sidebar-context-menu-open-in-private-window =
  .label = Open in New Private Window
sidebar-context-menu-bookmark-tab =
  .label = Bookmark Tab…
sidebar-context-menu-copy-link =
  .label = Copy Link
# Variables:
#   $deviceName (String) - The name of the device the user is closing a tab for
sidebar-context-menu-close-remote-tab =
  .label = Close tab on { $deviceName }

## Labels for sidebar history context menu items

sidebar-history-context-menu-delete-page =
  .label = Delete from History

## Labels for sidebar menu items.

sidebar-menu-genai-chat-label =
  .label = AI chatbot
sidebar-menu-history-label =
  .label = History
sidebar-menu-synced-tabs-label =
  .label = Tabs from other devices
sidebar-menu-bookmarks-label =
  .label = Bookmarks
sidebar-menu-customize-label =
  .label = Customize sidebar
sidebar-menu-review-checker-label =
  .label = Review Checker

## Headings for sidebar menu panels.

sidebar-menu-customize-header =
  .heading = Customize sidebar
sidebar-menu-history-header =
  .heading = History
sidebar-menu-syncedtabs-header =
  .heading = Tabs from other devices

## Context for closing synced tabs when hovering over the items

# Context for hovering over the close tab button that will
# send a push to the device to close said tab
# Variables:
#   $deviceName (String) - the name of the device the user is closing a tab for
synced-tabs-context-close-tab-title =
    .title = Close tab on { $deviceName }
