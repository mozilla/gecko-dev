package org.mozilla.fenix.ui.efficiency.selectors

import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.DataGenerationHelper.getStringResource
import org.mozilla.fenix.ui.efficiency.helpers.Selector
import org.mozilla.fenix.ui.efficiency.helpers.SelectorStrategy

object MicrosurveysSelectors {

    val CONTINUE_SURVEY_BUTTON = Selector(
        strategy = SelectorStrategy.COMPOSE_BY_TEXT,
        value = getStringResource(R.string.micro_survey_continue_button_label),
        description = "Survey Continue button",
        // Will see what groups we'll have once e start converting UI tests
        groups = listOf("browserSurvey"),
    )

    val all = listOf(
        CONTINUE_SURVEY_BUTTON,
    )
}
