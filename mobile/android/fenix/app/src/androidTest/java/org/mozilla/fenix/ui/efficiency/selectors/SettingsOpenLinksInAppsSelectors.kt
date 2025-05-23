package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsOpenLinksInAppsSelectors {

    val TOOLBAR_TITLE = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = getStringResource(R.string.preferences_open_links_in_apps),
        description = "Open link in apps toolbar title",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        TOOLBAR_TITLE,
    )
}
