# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

menu-view-genai-chat =
  .label = AI Chatbot

## Variables:
##   $date (string) - Date to be formatted based on locale

sidebar-history-date-today =
  .heading = Today — { DATETIME($date, dateStyle: "full") }
sidebar-history-date-yesterday =
  .heading = Yesterday — { DATETIME($date, dateStyle: "full") }
sidebar-history-date-this-month =
  .heading = { DATETIME($date, dateStyle: "full") }
sidebar-history-date-prev-month =
  .heading = { DATETIME($date, month: "long", year: "numeric") }

##

# "Search" is a noun (as in "Results of the search for")
# Variables:
#   $query (String) - The search query used for searching through browser history.
sidebar-search-results-header =
  .heading = Search results for “{ $query }”

## Labels for sidebar customize panel

sidebar-customize-firefox-tools =
  .label = { -brand-product-name } tools
sidebar-customize-firefox-settings = Manage { -brand-short-name } settings
sidebar-customize-settings = Sidebar settings
sidebar-position-left =
  .label = Show on the left
sidebar-position-right =
  .label = Show on the right

## Labels for sidebar context menu items

sidebar-context-menu-manage-extension =
  .label = Manage extension
sidebar-context-menu-remove-extension =
  .label = Remove extension
sidebar-context-menu-report-extension =
  .label = Report extension

# A header for a list of sidebar-specific extensions in the sidebar
sidebar-customize-extensions = Sidebar extensions

## Labels for sidebar menu items.

sidebar-menu-genai-chat-label =
  .label = AI chatbot
sidebar-menu-history-label =
  .label = History
sidebar-menu-synced-tabs-label =
  .label = Tabs from other devices
sidebar-menu-bookmarks-label =
  .label = Bookmarks
sidebar-menu-customize-label = Customize sidebar

## Tooltips for sidebar menu items.

sidebar-menu-genai-chat-item = {""}
  .title = AI chatbot
sidebar-menu-history-item = {""}
  .title = History
sidebar-menu-synced-tabs-item = {""}
  .title = Tabs from other devices
sidebar-menu-bookmarks-item = {""}
  .title = Bookmarks
sidebar-menu-customize-item = {""}
  .title = { sidebar-menu-customize-label }

## Headings for sidebar menu panels.

sidebar-menu-customize-header =
  .heading = Customize sidebar
sidebar-menu-history-header =
  .heading = History
sidebar-menu-syncedtabs-header =
  .heading = Tabs from other devices
