package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object MainMenuComposeSelectors {

    val NEW_PRIVATE_TAB_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = getStringResource(R.string.browser_menu_new_private_tab),
        description = "Main menu New private tab button",
        // Removed in https://bugzilla.mozilla.org/show_bug.cgi?id=1966222 as part of the menu redesign effort
        groups = listOf("removedIn=141"),
    )

    val EXTENSIONS_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.browser_menu_extensions) + getStringResource(R.string.browser_menu_try_a_recommended_extension_description),
        description = "Main menu Extensions button",
        groups = listOf("requiredForPage"),
    )

    val BOOKMARKS_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.library_bookmarks),
        description = "Main menu Bookmarks button",
        groups = listOf("requiredForPage"),
    )

    val HISTORY_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.library_history),
        description = "Main menu History button",
        groups = listOf("requiredForPage"),
    )

    val DOWLOADS_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.library_downloads),
        description = "Main menu Downloads button",
        groups = listOf("requiredForPage"),
    )

    val PASSWORDS_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.browser_menu_passwords),
        description = "Main menu Passwords button",
        groups = listOf("requiredForPage"),
    )

    val SIGN_IN_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.browser_menu_sign_in) + getStringResource(R.string.browser_menu_sign_in_caption_2),
        description = "Main menu Sign in button",
        groups = listOf("requiredForPage"),
    )

    val SETTINGS_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.browser_menu_settings),
        description = "Main menu Settings button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        NEW_PRIVATE_TAB_BUTTON,
        EXTENSIONS_BUTTON,
        BOOKMARKS_BUTTON,
        HISTORY_BUTTON,
        DOWLOADS_BUTTON,
        PASSWORDS_BUTTON,
        SIGN_IN_BUTTON,
        SETTINGS_BUTTON,
    )
}
