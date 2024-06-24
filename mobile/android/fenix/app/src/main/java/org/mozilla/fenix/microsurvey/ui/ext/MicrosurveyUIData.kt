/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui.ext

import androidx.annotation.DrawableRes
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.service.nimbus.messaging.MicrosurveyAnswer
import mozilla.components.service.nimbus.messaging.MicrosurveyConfig
import org.mozilla.fenix.R

/**
 * UI model for [MicrosurveyConfig].
 *
 * @property id Unique identifier of the microsurvey.
 * @property promptTitle The title to display on the 'prompt'.
 * @property icon The survey icon.
 * @property question The survey question.
 * @property answers The list of survey answers in Asc order based on [MicrosurveyAnswer.ordering].
 * @property utmContent Optional utm content parameter to specify the surveyed feature in a URL.
 */
data class MicrosurveyUIData(
    val id: String,
    val promptTitle: String,
    @DrawableRes val icon: Int,
    val question: String,
    val answers: List<String>,
    val utmContent: String? = null,
)

/**
 * @returns a [MicrosurveyUIData] derived from the given [Message].
 * [MicrosurveyUIData.answers] are sorted in Asc order based on [MicrosurveyAnswer.ordering].
 */
fun Message.toMicrosurveyUIData() = if (hasValidMicrosurveyConfig()) {
    MicrosurveyUIData(
        id = id,
        // title null checked in hasValidMicrosurveyConfig
        promptTitle = title!!,
        // microsurvey null checked in hasValidMicrosurveyConfig
        icon = microsurvey!!.icon?.resourceId ?: R.drawable.mozac_ic_lightbulb_24,
        question = text,
        // microsurvey null checked in hasValidMicrosurveyConfig
        answers = microsurvey!!.toSortedAnswers(),
        utmContent = microsurvey?.utmContent,
    )
} else {
    null
}

private fun Message.hasValidMicrosurveyConfig() =
    title != null && microsurvey != null && microsurvey!!.answers.isNotEmpty()

/**
 * @return a list of text answers derived from the given [MicrosurveyConfig.answers] sorted in
 * Asc order based on [MicrosurveyAnswer.ordering].
 */
private fun MicrosurveyConfig.toSortedAnswers() = answers.sortedBy { it.ordering }.map { it.text }
