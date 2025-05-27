package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SitePermissionsSelectors {

    val PAGE_PERMISSION_DIALOG_ALLOW_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "allow_button",
        description = "Permission dialog allow button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        PAGE_PERMISSION_DIALOG_ALLOW_BUTTON,
    )
}
