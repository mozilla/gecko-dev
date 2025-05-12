/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

internal object TabsTrayTestTag {
    const val TABS_TRAY = "tabstray"

    // Tabs Tray Banner
    const val BANNER_ROOT = "$TABS_TRAY.banner"
    const val BANNER_HANDLE = "$BANNER_ROOT.handle"
    const val NORMAL_TABS_PAGE_BUTTON = "$BANNER_ROOT.normalTabsPageButton"
    const val PRIVATE_TABS_PAGE_BUTTON = "$BANNER_ROOT.privateTabsPageButton"
    const val SYNCED_TABS_PAGE_BUTTON = "$BANNER_ROOT.syncedTabsPageButton"

    const val SELECTION_COUNTER = "$BANNER_ROOT.selectionCounter"
    const val COLLECTIONS_BUTTON = "$BANNER_ROOT.collections"

    // Tabs Tray Banner three dot menu
    const val THREE_DOT_BUTTON = "$BANNER_ROOT.threeDotButton"

    const val ACCOUNT_SETTINGS = "$THREE_DOT_BUTTON.accountSettings"
    const val CLOSE_ALL_TABS = "$THREE_DOT_BUTTON.closeAllTabs"
    const val RECENTLY_CLOSED_TABS = "$THREE_DOT_BUTTON.recentlyClosedTabs"
    const val SELECT_TABS = "$THREE_DOT_BUTTON.selectTabs"
    const val SHARE_ALL_TABS = "$THREE_DOT_BUTTON.shareAllTabs"
    const val TAB_SETTINGS = "$THREE_DOT_BUTTON.tabSettings"

    // FAB
    const val FAB = "$TABS_TRAY.fab"

    // Tab lists
    private const val TAB_LIST_ROOT = "$TABS_TRAY.tabList"
    const val NORMAL_TABS_LIST = "$TAB_LIST_ROOT.normal"
    const val PRIVATE_TABS_LIST = "$TAB_LIST_ROOT.private"
    const val SYNCED_TABS_LIST = "$TAB_LIST_ROOT.synced"

    const val EMPTY_NORMAL_TABS_LIST = "$NORMAL_TABS_LIST.empty"
    const val EMPTY_PRIVATE_TABS_LIST = "$PRIVATE_TABS_LIST.empty"

    // Tab items
    const val TAB_ITEM_ROOT = "$TABS_TRAY.tabItem"
    const val TAB_ITEM_CLOSE = "$TAB_ITEM_ROOT.close"
    const val TAB_ITEM_THUMBNAIL = "$TAB_ITEM_ROOT.thumbnail"
}
