package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object TranslationsSelectors {

    val TRANSLATIONS_OPTIONS_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_CONTENT_DESCRIPTION,
        value = getStringResource(R.string.translation_option_bottom_sheet_title_heading),
        description = "Translations sheet options button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        TRANSLATIONS_OPTIONS_BUTTON,
    )
}
