package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object HomeSelectors {
    val TOP_SITES_LIST = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_RES_ID,
        value = "id/top_sites_list",
        description = "Top Sites List",
        groups = listOf("topSites"),
    )

    val TOP_SITES_LIST_COMPOSE = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TAG,
        value = "top_sites_list",
        description = "Top Sites List",
        groups = listOf("topSitesCompose"),
    )

    val THREE_DOT_MENU = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "menuButton",
        description = "Three Dot Menu",
        groups = listOf("requiredForPage"),
    )

    // TODO (Jackie J. 4/18/25): move THREE_DOT_MENU to component object file in next phase

    // TODO (Jackie J. 4/18/25): add support for resId as text
    val TDM_SETTINGS_BUTTON_COMPOSE = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = "Settings",
        description = "Menu item with text 'Settings'",
        groups = listOf("threeDotMenu", "compose"),
    )

    val TDM_SETTINGS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.browser_menu_settings),
        description = "the Settings Button",
        groups = listOf("threeDotMenu"),
    )

    val TDM_HISTORY_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.library_history),
        description = "the History button",
        groups = listOf("threeDotMenu", "Bug-1234"),
    )

    val TDM_BOOKMARKS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.library_bookmarks),
        description = "the Bookmarks button",
        groups = listOf("threeDotMenu"),
    )

    val TDM_DOWNLOADS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.library_downloads),
        description = "the Downloads button",
        groups = listOf("threeDotMenu"),
    )

    val TDM_PASSWORDS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.browser_menu_passwords),
        description = "the Passwords button",
        groups = listOf("threeDotMenu"),
    )

    val all = listOf(
        THREE_DOT_MENU,
        TDM_HISTORY_BUTTON,
        TDM_BOOKMARKS_BUTTON,
        TDM_DOWNLOADS_BUTTON,
        TDM_PASSWORDS_BUTTON,
        TDM_SETTINGS_BUTTON,
        TOP_SITES_LIST,
    )
}
