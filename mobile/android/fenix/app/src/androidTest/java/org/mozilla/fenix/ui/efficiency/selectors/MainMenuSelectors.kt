package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object MainMenuSelectors {
    val SETTINGS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.browser_menu_settings),
        description = "Main menu Settings Button",
        groups = listOf("requiredForPage"),
    )

    val HISTORY_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.library_history),
        description = "Main menu History button",
        groups = listOf("requiredForPage"),
    )

    val BOOKMARKS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.library_bookmarks),
        description = "Main menu Bookmarks button",
        groups = listOf("requiredForPage"),
    )

    val DOWNLOADS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.library_downloads),
        description = "Main menu Downloads button",
        groups = listOf("requiredForPage"),
    )

    val PASSWORDS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = getStringResource(R.string.browser_menu_passwords),
        description = "Main menu Passwords button",
        groups = listOf("requiredForPage"),
    )

    val EXTENSIONS_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT,
        value = getStringResource(R.string.browser_menu_extensions),
        description = "Main menu Extensions button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        HISTORY_BUTTON,
        BOOKMARKS_BUTTON,
        DOWNLOADS_BUTTON,
        PASSWORDS_BUTTON,
        SETTINGS_BUTTON,
        EXTENSIONS_BUTTON,
    )
}
