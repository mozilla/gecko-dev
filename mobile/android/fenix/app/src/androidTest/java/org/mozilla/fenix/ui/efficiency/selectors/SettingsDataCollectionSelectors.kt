package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsDataCollectionSelectors {

    val NAVIGATE_BACK_TOOLBAR_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_DESCRIPTION_CONTAINS,
        value = "Navigate up",
        description = "Navigate back toolbar button",
        groups = listOf("requiredForPage"),
    )

    val SEND_TECHNICAL_AND_INTERACTION_DATA_OPTION = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT_CONTAINS,
        value = getStringResource(R.string.preference_usage_data_2),
        description = "Navigate back toolbar button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        NAVIGATE_BACK_TOOLBAR_BUTTON,
    )
}
