package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object ReaderViewSelectors {

    val APPEARANCE_FONT_SANS_SERIF = Selector(
        strategy = SelectorStrategy.ESPRESSO_BY_ID,
        value = "mozac_feature_readerview_font_sans_serif",
        description = "Sans serif font button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        APPEARANCE_FONT_SANS_SERIF,
    )
}
