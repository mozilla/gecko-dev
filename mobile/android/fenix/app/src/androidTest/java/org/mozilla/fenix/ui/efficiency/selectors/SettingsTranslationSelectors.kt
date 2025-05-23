package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object SettingsTranslationSelectors {

    val DOWNLOAD_LANGUAGES_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = getStringResource(R.string.translation_settings_download_language),
        description = "Download languages button",
        groups = listOf("requiredForPage"),
    )

    val all = listOf(
        DOWNLOAD_LANGUAGES_BUTTON,
    )
}
