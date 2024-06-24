/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui.ext

import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.service.nimbus.messaging.MessageData
import mozilla.components.service.nimbus.messaging.MicrosurveyAnswer
import mozilla.components.service.nimbus.messaging.MicrosurveyConfig
import mozilla.components.service.nimbus.messaging.StyleData
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.NullVariables
import org.mozilla.experiments.nimbus.StringHolder
import org.mozilla.fenix.R
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class MicrosurveyUIDataTest {
    private val answer1 = MicrosurveyAnswer(text = StringHolder(null, "a"), ordering = 0)
    private val answer2 = MicrosurveyAnswer(text = StringHolder(null, "b"), ordering = 1)
    private val answer3 = MicrosurveyAnswer(text = StringHolder(null, "c"), ordering = 2)
    private val answer4 = MicrosurveyAnswer(text = StringHolder(null, "d"), ordering = 3)
    private val unorderedAnswers = listOf(answer3, answer1, answer4, answer2)

    private val orderedAnswersText = listOf("a", "b", "c", "d")

    @Before
    fun setup() {
        NullVariables.instance.setContext(testContext)
    }

    @Test
    fun `WHEN message has a valid microsurvey configuration THEN toMicrosurveyUIData returns the UI data from the raw data`() {
        val microsurveyConfig = MicrosurveyConfig(
            utmContent = "test utm content",
            icon = R.drawable.ic_print,
            answers = unorderedAnswers,
        )
        val messageData = MessageData(
            title = StringHolder(null, "test title"),
            text = StringHolder(null, "test question"),
            microsurveyConfig = microsurveyConfig,
        )
        val message = createTestMessage(messageData)

        val expected = MicrosurveyUIData(
            id = "test ID",
            promptTitle = "test title",
            icon = R.drawable.ic_print,
            question = "test question",
            answers = orderedAnswersText,
            utmContent = "test utm content",
        )
        val actual = message.toMicrosurveyUIData()
        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN message has no title THEN toMicrosurveyUIData returns null`() {
        val microsurveyConfig = MicrosurveyConfig(
            utmContent = "test utm content",
            icon = R.drawable.ic_print,
            answers = unorderedAnswers,
        )
        val messageData = MessageData(
            text = StringHolder(null, "test question"),
            microsurveyConfig = microsurveyConfig,
        )
        val message = createTestMessage(messageData)

        val actual = message.toMicrosurveyUIData()
        assertEquals(null, actual)
    }

    @Test
    fun `WHEN message has no microsurvey THEN toMicrosurveyUIData returns null`() {
        val messageData = MessageData(
            title = StringHolder(null, "test title"),
            text = StringHolder(null, "test question"),
        )
        val message = createTestMessage(messageData)

        val actual = message.toMicrosurveyUIData()
        assertEquals(null, actual)
    }

    @Test
    fun `WHEN microsurvey answers is empty THEN toMicrosurveyUIData returns null`() {
        val microsurveyConfig = MicrosurveyConfig(
            utmContent = "test utm content",
            icon = R.drawable.ic_print,
            answers = emptyList(),
        )
        val messageData = MessageData(
            title = StringHolder(null, "test title"),
            text = StringHolder(null, "test question"),
            microsurveyConfig = microsurveyConfig,
        )
        val message = createTestMessage(messageData)

        val actual = message.toMicrosurveyUIData()
        assertEquals(null, actual)
    }

    @Test
    fun `WHEN microsurvey has no icon THEN toMicrosurveyUIData returns the UI data from the raw data with the default icon`() {
        val microsurveyConfig = MicrosurveyConfig(
            utmContent = "test utm content",
            answers = unorderedAnswers,
        )
        val messageData = MessageData(
            title = StringHolder(null, "test title"),
            text = StringHolder(null, "test question"),
            microsurveyConfig = microsurveyConfig,
        )
        val message = createTestMessage(messageData)

        val expected = MicrosurveyUIData(
            id = "test ID",
            promptTitle = "test title",
            icon = R.drawable.mozac_ic_lightbulb_24,
            question = "test question",
            answers = orderedAnswersText,
            utmContent = "test utm content",
        )
        val actual = message.toMicrosurveyUIData()
        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN microsurvey has no utm content THEN toMicrosurveyUIData returns the UI data from the raw data`() {
        val microsurveyConfig = MicrosurveyConfig(
            icon = R.drawable.ic_print,
            answers = unorderedAnswers,
        )
        val messageData = MessageData(
            title = StringHolder(null, "test title"),
            text = StringHolder(null, "test question"),
            microsurveyConfig = microsurveyConfig,
        )
        val message = createTestMessage(messageData)

        val expected = MicrosurveyUIData(
            id = "test ID",
            promptTitle = "test title",
            icon = R.drawable.ic_print,
            question = "test question",
            answers = orderedAnswersText,
            utmContent = null,
        )
        val actual = message.toMicrosurveyUIData()
        assertEquals(expected, actual)
    }

    private fun createTestMessage(messageData: MessageData) = Message(
        id = "test ID",
        data = messageData,
        action = "action",
        style = StyleData(),
        triggerIfAll = emptyList(),
        excludeIfAny = emptyList(),
        metadata = Message.Metadata(id = "test ID"),
    )
}
