package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object DownloadsSelectors {

    val NAVIGATE_BACK_TOOLBAR_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.download_navigate_back_description),
        description = "Navigate back toolbar button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        NAVIGATE_BACK_TOOLBAR_BUTTON,
    )
}
