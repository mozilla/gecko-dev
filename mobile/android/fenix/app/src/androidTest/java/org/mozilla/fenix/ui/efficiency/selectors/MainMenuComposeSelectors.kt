package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object MainMenuComposeSelectors {

    val NEW_PRIVATE_TAB_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = getStringResource(R.string.browser_menu_new_private_tab),
        description = "Find in page close button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        NEW_PRIVATE_TAB_BUTTON,
    )
}
