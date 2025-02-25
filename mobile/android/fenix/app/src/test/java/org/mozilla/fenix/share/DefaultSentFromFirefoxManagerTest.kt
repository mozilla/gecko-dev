/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import org.junit.Test

private const val TEMPLATE_MESSAGE = "%1\$s\n\nSent from %2\$s: %3\$s"
private const val SHARE_MESSAGE = "https://www.mozilla.org"
private const val APP_NAME = "Firefox"
private const val DOWNLOAD_LINK = "https://www.mozilla.org/en-US/firefox/download/thanks/"
private const val EXPECTED_MESSAGE = "$SHARE_MESSAGE\n\nSent from $APP_NAME: $DOWNLOAD_LINK"
private const val WHATSAPP_PACKAGE_NAME = "com.whatsapp"
private const val SLACK_PACKAGE_NAME = "com.Slack"

class DefaultSentFromFirefoxManagerTest {

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

    @Test
    fun `WHEN feature is disabled THEN we should not show snackbar`() {
        val testedFeature = buildDefaultFeature(isFeatureEnabled = false)

        testedFeature.maybeAppendShareText(WHATSAPP_PACKAGE_NAME, SHARE_MESSAGE)

        assertFalse(testedFeature.shouldShowSnackbar)
    }

    @Test
    fun `WHEN share link snackbar is disabled THEN we should not show snackbar`() {
        val testedFeature = buildDefaultFeature(isSnackbarEnabled = false)

        testedFeature.maybeAppendShareText(WHATSAPP_PACKAGE_NAME, SHARE_MESSAGE)

        assertFalse(testedFeature.shouldShowSnackbar)
    }

    @Test
    fun `GIVEN feature and snackbar are enabled, snackbar has not been shown WHEN shared message is appended THEN we should show snackbar`() {
        val testedFeature = buildDefaultFeature(
            isFeatureEnabled = true,
            isSnackbarEnabled = true,
            isLinkSharingSettingsSnackbarShown = false,
        )

        testedFeature.maybeAppendShareText(WHATSAPP_PACKAGE_NAME, SHARE_MESSAGE)

        assertTrue(testedFeature.shouldShowSnackbar)
    }

    private fun buildDefaultFeature(
        isFeatureEnabled: Boolean = true,
        isSnackbarEnabled: Boolean = true,
        isLinkSharingSettingsSnackbarShown: Boolean = false,
        templateMessage: String = TEMPLATE_MESSAGE,
        appName: String = APP_NAME,
        downloadLink: String = DOWNLOAD_LINK,
    ) = DefaultSentFromFirefoxManager(
        snackbarEnabled = isSnackbarEnabled,
        templateMessage = templateMessage,
        appName = appName,
        downloadLink = downloadLink,
        storage = object : SentFromStorage {
            override var isLinkSharingSettingsSnackbarShown = isLinkSharingSettingsSnackbarShown
            override val featureEnabled: Boolean = isFeatureEnabled
        },
    )
}
