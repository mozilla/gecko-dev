package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object CustomTabsSelectors {

    val MAIN_MENU_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "mozac_browser_toolbar_menu",
        description = "Custom tabs main menu button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        MAIN_MENU_BUTTON,
    )
}
