/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.onboarding

import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.onboarding.view.OnboardingPageUiData
import org.mozilla.fenix.onboarding.view.notificationPageUiData
import org.mozilla.fenix.onboarding.view.syncPageUiData

@RunWith(FenixRobolectricTestRunner::class)
class DefaultBrowserPromptManagerTest {

    @Test
    fun `WHEN browser is already default THEN can not show the prompt`() {
        val promptManager = DefaultBrowserPromptManager(
            storage = buildStorage(isDefaultBrowser = true),
            promptToSetAsDefaultBrowser = {},
        )

        assertFalse(promptManager.canShowPrompt())
    }

    @Test
    fun `WHEN prompt is already displayed THEN can not show it`() {
        val promptManager = DefaultBrowserPromptManager(
            storage = buildStorage(promptToSetAsDefaultBrowserDisplayedInOnboarding = true),
            promptToSetAsDefaultBrowser = {},
        )

        assertFalse(promptManager.canShowPrompt())
    }

    @Test
    fun `WHEN prompt is not supported THEN we can not show it`() {
        val promptManager = DefaultBrowserPromptManager(
            storage = buildStorage(isDefaultBrowserPromptSupported = false),
            promptToSetAsDefaultBrowser = {},
        )

        assertFalse(promptManager.canShowPrompt())
    }

    @Test
    fun `GIVEN we can show prompt and there is no ToS card WHEN a card is shown THEN prompt the user`() {
        var promptToSetAsDefaultBrowserCalled = false
        val promptManager = DefaultBrowserPromptManager(
            storage = buildStorage(),
            promptToSetAsDefaultBrowser = { promptToSetAsDefaultBrowserCalled = true },
        )

        assertTrue(promptManager.canShowPrompt())

        promptManager.maybePromptToSetAsDefaultBrowser(
            pagesToDisplay = listOf(
                syncPageUiData,
                notificationPageUiData,
            ),
            currentCard = syncPageUiData,
        )

        assertTrue(promptToSetAsDefaultBrowserCalled)
    }

    @Test
    fun `GIVEN we can show prompt WHEN there is a ToS card THEN wait for it to be shown before prompting the user`() {
        var promptToSetAsDefaultBrowserCalled = false
        val promptManager = DefaultBrowserPromptManager(
            storage = buildStorage(),
            promptToSetAsDefaultBrowser = { promptToSetAsDefaultBrowserCalled = true },
        )
        val pagesToDisplay = listOf(syncPageUiData, tosPageUiData, notificationPageUiData)

        assertTrue(promptManager.canShowPrompt())

        // yet to show ToS
        promptManager.maybePromptToSetAsDefaultBrowser(
            pagesToDisplay = pagesToDisplay,
            currentCard = syncPageUiData,
        )
        assertFalse(promptToSetAsDefaultBrowserCalled)

        // showing ToS
        promptManager.maybePromptToSetAsDefaultBrowser(
            pagesToDisplay = pagesToDisplay,
            currentCard = tosPageUiData,
        )
        assertFalse(promptToSetAsDefaultBrowserCalled)

        // already showed ToS, can prompt the user
        promptManager.maybePromptToSetAsDefaultBrowser(
            pagesToDisplay = pagesToDisplay,
            currentCard = notificationPageUiData,
        )

        assertTrue(promptToSetAsDefaultBrowserCalled)
    }

    private fun buildStorage(
        isDefaultBrowser: Boolean = false,
        isDefaultBrowserPromptSupported: Boolean = true,
        promptToSetAsDefaultBrowserDisplayedInOnboarding: Boolean = false,
    ) = object : DefaultBrowserPromptStorage {
        override val isDefaultBrowser: Boolean = isDefaultBrowser
        override val isDefaultBrowserPromptSupported: Boolean = isDefaultBrowserPromptSupported
        override var promptToSetAsDefaultBrowserDisplayedInOnboarding = promptToSetAsDefaultBrowserDisplayedInOnboarding
    }
}

val tosPageUiData = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.TERMS_OF_SERVICE,
    imageRes = R.drawable.ic_firefox,
    title = "tos title",
    description = "tos body",
    primaryButtonLabel = "tos primary button text",
    secondaryButtonLabel = "tos secondary button text",
    privacyCaption = null,
)
