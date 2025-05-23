package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SiteSecuritySelectors {

    val CLEAR_COOKIES_AND_SITE_DATA_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_RES_ID,
        value = "clearSiteData",
        description = "Clear cookies and site date quick settings sheet button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        CLEAR_COOKIES_AND_SITE_DATA_BUTTON,
    )
}
