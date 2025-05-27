package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object PWASelectors {

    val PWA_SCREEN = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "engineView",
        description = "PWA screen",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        PWA_SCREEN,
    )
}
