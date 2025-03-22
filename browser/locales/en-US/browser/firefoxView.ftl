# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

toolbar-button-firefox-view-2 =
  .label = { -firefoxview-brand-name }
  .tooltiptext = View recent browsing across windows and devices

menu-tools-firefox-view =
  .label = { -firefoxview-brand-name }
  .accesskey = F

firefoxview-page-title = { -firefoxview-brand-name }

firefoxview-page-heading =
  .heading = { -firefoxview-brand-name }

firefoxview-page-label =
  .label = { -firefoxview-brand-name }

# Used instead of the localized relative time when a timestamp is within a minute or so of now
firefoxview-just-now-timestamp = Just now

firefoxview-syncedtabs-signin-header-2 = Your { -brand-product-name } on all your devices
firefoxview-syncedtabs-signin-description-2 = To see tabs you have open on your phone and other devices, sign in or sign up for an account. With an account, you can also sync your passwords, history, and more.
firefoxview-syncedtabs-signin-primarybutton-2 = Sign in

firefoxview-syncedtabs-adddevice-header-2 = Grab tabs from anywhere
firefoxview-syncedtabs-adddevice-description-2 = Sign in to { -brand-product-name } on your phone or another computer to see tabs here. Learn how to <a data-l10n-name="url">connect additional devices</a>.
firefoxview-syncedtabs-adddevice-primarybutton = Try { -brand-product-name } for mobile

firefoxview-tabpickup-synctabs-primarybutton = Sync open tabs

firefoxview-syncedtabs-synctabs-header = Update your sync settings
firefoxview-syncedtabs-synctabs-description = To see tabs from other devices, you need to sync your open tabs.

firefoxview-syncedtabs-loading-header = Sync in progress
firefoxview-syncedtabs-loading-description = When it’s done, you’ll see any tabs you have open on other devices. Check back soon.

firefoxview-tabpickup-fxa-admin-disabled-header = Your organization has disabled sync
firefoxview-tabpickup-fxa-disabled-by-policy-description = { -brand-short-name } is not able to sync tabs between devices because your organization has disabled syncing.

firefoxview-tabpickup-network-offline-header = Check your internet connection
firefoxview-tabpickup-network-offline-description = If you’re using a firewall or proxy, check that { -brand-short-name } has permission to access the web.
firefoxview-tabpickup-network-offline-primarybutton = Try again

firefoxview-tabpickup-sync-error-header = We’re having trouble syncing
firefoxview-tabpickup-generic-sync-error-description = { -brand-short-name } can’t reach the syncing service right now. Try again in a few moments.
firefoxview-tabpickup-sync-error-primarybutton = Try again

firefoxview-tabpickup-sync-disconnected-header = Turn on syncing to continue
firefoxview-tabpickup-sync-disconnected-description = To grab your tabs, you’ll need to allow syncing in { -brand-short-name }.
firefoxview-tabpickup-sync-disconnected-primarybutton = Turn on sync in settings

firefoxview-tabpickup-password-locked-header = Enter your Primary Password to view tabs
firefoxview-tabpickup-password-locked-description = To grab your tabs, you’ll need to enter the Primary Password for { -brand-short-name }.
firefoxview-tabpickup-password-locked-link = Learn more
firefoxview-tabpickup-password-locked-primarybutton = Enter Primary Password
firefoxview-syncedtab-password-locked-link = <a data-l10n-name="syncedtab-password-locked-link">Learn more</a>

firefoxview-tabpickup-signed-out-header = Sign in to reconnect
firefoxview-tabpickup-signed-out-description2 = To reconnect and grab your tabs, sign in to your account.
firefoxview-tabpickup-signed-out-primarybutton = Sign in

# Variables:
#   $tabTitle (string) - Title of tab being dismissed
firefoxview-closed-tabs-dismiss-tab =
  .title = Dismiss { $tabTitle }

# Variables:
#   $targetURI (string) - URL that will be opened in the new tab
firefoxview-tabs-list-tab-button =
  .title = Open { $targetURI } in a new tab

firefoxview-collapse-button-show =
  .title = Show list

firefoxview-collapse-button-hide =
  .title = Hide list

firefoxview-overview-nav = Recent browsing
  .title = Recent browsing
firefoxview-overview-header = Recent browsing
  .title = Recent browsing

## History in this context refers to browser history

firefoxview-history-nav = History
  .title = History
firefoxview-history-header = History
firefoxview-history-context-delete = Delete from History
    .accesskey = D

## Open Tabs in this context refers to all open tabs in the browser

firefoxview-opentabs-nav = Open tabs
  .title = Open tabs
firefoxview-opentabs-header = Open tabs

## Recently closed tabs in this context refers to recently closed tabs from all windows

firefoxview-recently-closed-nav = Recently closed tabs
  .title = Recently closed tabs
firefoxview-recently-closed-header = Recently closed tabs

## Tabs from other devices refers in this context refers to synced tabs from other devices

firefoxview-synced-tabs-nav = Tabs from other devices
  .title = Tabs from other devices
firefoxview-synced-tabs-header = Tabs from other devices

##

# Used for a link in collapsible cards, in the ’Recent browsing’ page of Firefox View
firefoxview-view-all-link = View all

# Variables:
#   $winID (Number) - The index of the owner window for this set of tabs
firefoxview-opentabs-window-header =
  .title = Window { $winID }

# Variables:
#   $winID (Number) - The index of the owner window (which is currently focused) for this set of tabs
firefoxview-opentabs-current-window-header =
  .title = Window { $winID } (Current)

firefoxview-show-more = Show more
firefoxview-show-less = Show less
firefoxview-show-all = Show all

firefoxview-search-text-box-clear-button =
  .title = Clear

# Placeholder for the input field to search in recent browsing ("search" is a verb).
firefoxview-search-text-box-recentbrowsing =
  .placeholder = Search

# Placeholder for the input field to search in history ("search" is a verb).
firefoxview-search-text-box-history =
  .placeholder = Search history

# Placeholder for the input field to search in recently closed tabs ("search" is a verb).
firefoxview-search-text-box-recentlyclosed =
  .placeholder = Search recently closed tabs

# Placeholder for the input field to search in tabs from other devices ("search" is a verb).
firefoxview-search-text-box-tabs =
  .placeholder = Search tabs

# Placeholder for the input field to search in open tabs ("search" is a verb).
firefoxview-search-text-box-opentabs =
  .placeholder = Search open tabs

# "Search" is a noun (as in "Results of the search for")
# Variables:
#   $query (String) - The search query used for searching through browser history.
firefoxview-search-results-header = Search results for “{ $query }”

# Variables:
#   $count (Number) - The number of visits matching the search query.
firefoxview-search-results-count = { $count ->
  [one] { $count } site
 *[other] { $count } sites
}

# Message displayed when a search is performed and no matching results were found.
# Variables:
#   $query (String) - The search query.
firefoxview-search-results-empty = No results for “{ $query }”

firefoxview-sort-history-by-date-label = Sort by date
firefoxview-sort-history-by-site-label = Sort by site
firefoxview-sort-open-tabs-by-recency-label = Sort by recent activity
firefoxview-sort-open-tabs-by-order-label = Sort by tab order

## Variables:
##   $date (string) - Date to be formatted based on locale

firefoxview-history-date-today = Today - { DATETIME($date, dateStyle: "full") }
firefoxview-history-date-yesterday = Yesterday - { DATETIME($date, dateStyle: "full") }
firefoxview-history-date-this-month = { DATETIME($date, dateStyle: "full") }
firefoxview-history-date-prev-month = { DATETIME($date, month: "long", year: "numeric") }

# When history is sorted by site, this heading is used in place of a domain, in
# order to group sites that do not come from an outside host.
# For example, this would be the heading for all file:/// URLs in history.
firefoxview-history-site-localhost = (local files)

##

firefoxview-show-all-history = Show all history

## Message displayed in Firefox View when the user has no history data

firefoxview-history-empty-header = Get back to where you’ve been
firefoxview-history-empty-description = As you browse, the pages you visit will be listed here.
firefoxview-history-empty-description-two = Protecting your privacy is at the heart of what we do. It’s why you can control the activity { -brand-short-name } remembers, in your <a data-l10n-name="history-settings-url">history settings</a>.

##

# Button text for choosing a browser within the ’Import history from another browser’ banner
firefoxview-choose-browser-button = Choose browser
  .title = Choose browser

## Message displayed in Firefox View when the user has chosen to never remember History

firefoxview-dont-remember-history-empty-header-2 = You’re in control of what { -brand-short-name } remembers
firefoxview-dont-remember-history-empty-description-one = Right now, { -brand-short-name } does not remember your browsing activity. To change that, <a data-l10n-name="history-settings-url-two">update your history settings</a>.

##

# This label is read by screen readers when focusing the close button for the "Import history from another browser" banner in Firefox View
firefoxview-import-history-close-button =
  .aria-label = Close
  .title = Close

## Text displayed in a dismissable banner to import bookmarks/history from another browser

firefoxview-import-history-header = Import history from another browser
firefoxview-import-history-description = Make { -brand-short-name } your go-to browser. Import browsing history, bookmarks, and more.

## Message displayed in Firefox View when the user has no recently closed tabs data

firefoxview-recentlyclosed-empty-header = Closed a tab too soon?
firefoxview-recentlyclosed-empty-description = Here you’ll find the tabs you recently closed, so you can reopen any of them quickly.
firefoxview-recentlyclosed-empty-description-two = To find tabs from longer ago, view your <a data-l10n-name="history-url">browsing history</a>.

## This message is displayed below the name of another connected device when it doesn't have any open tabs.

firefoxview-syncedtabs-device-notabs = No tabs open on this device

firefoxview-syncedtabs-connect-another-device = Connect another device

firefoxview-pinned-tabs =
  .title = Pinned Tabs

firefoxview-tabs =
  .title = Tabs

## These tooltips will be displayed when hovering over a pinned tab on the Open Tabs page
## Variables:
##  $tabTitle (string) - Title of pinned tab that will be opened when selected

firefoxview-opentabs-pinned-tab =
  .title = Switch to { $tabTitle }

# This tooltip will be shown for a pinned tab whose URL is currently bookmarked.
firefoxview-opentabs-bookmarked-pinned-tab =
  .title = Switch to (Bookmarked) { $tabTitle }

## These tooltips will be displayed when hovering over an unpinned Open Tab
## Variables:
##   $url (string) - URL of tab that will be opened when selected

# This tooltip will be shown for an unpinned tab whose URL is currently bookmarked.
firefoxview-opentabs-bookmarked-tab =
  .title = (Bookmarked) { $url }
