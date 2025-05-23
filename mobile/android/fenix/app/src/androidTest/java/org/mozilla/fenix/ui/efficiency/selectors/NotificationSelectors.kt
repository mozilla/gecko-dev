package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.helpers.TestHelper.appName
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object NotificationSelectors {

    // We should add UISelector with res id and text
    val NOTIFICATION_HEADER = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR_WITH_TEXT,
        value = appName,
        description = "System notification header",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        NOTIFICATION_HEADER,
    )
}
