/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import junit.framework.TestCase.assertEquals
import org.junit.Test
import org.mozilla.fenix.share.DefaultSentFromFirefoxFeature.Companion.WHATSAPP_PACKAGE_NAME

class DefaultSentFromFirefoxFeatureTest {

    companion object {
        private const val TEMPLATE_MESSAGE = "%1\$s\n\nSent from %2\$s: %3\$s"
        private const val SHARE_MESSAGE = "https://www.mozilla.org"
        private const val APP_NAME = "Firefox"
        private const val DOWNLOAD_LINK = "https://www.mozilla.org/en-US/firefox/download/thanks/"
        private const val EXPECTED_MESSAGE = "$SHARE_MESSAGE\n\nSent from $APP_NAME: $DOWNLOAD_LINK"
        private const val SLACK_PACKAGE_NAME = "com.Slack"
    }

    @Test
    fun `GIVEN feature is enabled WHEN sharing with WhatsApp THEN message is appended`() {
        val feature = buildDefaultFeature(isFeatureEnabled = true)

        val result = feature.maybeAppendShareText(WHATSAPP_PACKAGE_NAME, SHARE_MESSAGE)

        assertEquals(EXPECTED_MESSAGE, result)
    }

    @Test
    fun `GIVEN feature is disabled WHEN sharing with WhatsApp THEN message is not changed`() {
        val feature = buildDefaultFeature(isFeatureEnabled = false)

        val result = feature.maybeAppendShareText(WHATSAPP_PACKAGE_NAME, SHARE_MESSAGE)

        assertEquals(SHARE_MESSAGE, result)
    }

    @Test
    fun `GIVEN feature is enabled WHEN sharing with a different app THEN message is not changed`() {
        val feature = buildDefaultFeature(isFeatureEnabled = true)

        val result = feature.maybeAppendShareText(SLACK_PACKAGE_NAME, SHARE_MESSAGE)

        assertEquals(SHARE_MESSAGE, result)
    }

    @Test
    fun `WHEN template message does not contain a placeholder THEN text is formatted accordingly`() {
        val unexpectedTemplate = "%1\$s\n\n %3\$s"
        val expectedResult = "$SHARE_MESSAGE\n\n $DOWNLOAD_LINK"
        val feature = buildDefaultFeature(isFeatureEnabled = true, templateMessage = unexpectedTemplate)

        val result = feature.getSentFromFirefoxMessage(SHARE_MESSAGE)

        assertEquals(expectedResult, result)
    }

    @Test
    fun `WHEN template does not have placeholders THEN only template is shown`() {
        val unexpectedTemplate = "Hello world"
        val feature = buildDefaultFeature(isFeatureEnabled = true, templateMessage = unexpectedTemplate)

        val result = feature.getSentFromFirefoxMessage(SHARE_MESSAGE)

        assertEquals(unexpectedTemplate, result)
    }

    private fun buildDefaultFeature(
        isFeatureEnabled: Boolean = true,
        templateMessage: String = TEMPLATE_MESSAGE,
        appName: String = APP_NAME,
        downloadLink: String = DOWNLOAD_LINK,
    ) = DefaultSentFromFirefoxFeature(
        isFeatureEnabled = isFeatureEnabled,
        templateMessage = templateMessage,
        appName = appName,
        downloadLink = downloadLink,
    )
}
