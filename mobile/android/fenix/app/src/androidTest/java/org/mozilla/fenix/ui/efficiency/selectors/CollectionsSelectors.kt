package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object CollectionsSelectors {

    val ADD_NEW_COLLECTION_BUTTON = Selector(
        strategy = SelectorStrategy.UIAUTOMATOR2_BY_TEXT,
        value = "Add new collection",
        description = "Add new collection from tabs tray collections section",
        groups = listOf("tabsTrayCollectionsSection"),
    )

    val all = listOf(
        ADD_NEW_COLLECTION_BUTTON,
    )
}
