/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.service.nimbus.messaging.Message
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MicrosurveyAction.Completed
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MicrosurveyAction.Dismissed
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MicrosurveyAction.OnPrivacyNoticeTapped
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MicrosurveyAction.SentConfirmationShown
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MicrosurveyAction.Shown
import org.mozilla.fenix.helpers.FenixGleanTestRule
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.SupportUtils

private val PRIVACY_POLICY_URL =
    SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE) +
        "?utm_medium=firefox-mobile&utm_source=modal&utm_campaign=microsurvey"

@RunWith(FenixRobolectricTestRunner::class)
class MicrosurveyMessageControllerTest {

    @get:Rule
    val gleanTestRule = FenixGleanTestRule(testContext)

    private val homeActivity: HomeActivity = mockk(relaxed = true)
    private val message: Message = mockk(relaxed = true)
    private lateinit var microsurveyMessageController: MicrosurveyMessageController
    private val appStore: AppStore = mockk(relaxed = true)

    @Before
    fun setup() {
        microsurveyMessageController = MicrosurveyMessageController(
            appStore = appStore,
            homeActivity = homeActivity,
        )
    }

    @Test
    fun `WHEN calling onMessagePressed THEN update the app store with the MessageClicked action`() {
        microsurveyMessageController.onMessagePressed(message)
        verify { appStore.dispatch(MessageClicked(message)) }
    }

    @Test
    fun `WHEN calling onMessageDismissed THEN update the app store with the MessageDismissed action`() {
        microsurveyMessageController.onMessageDismissed(message)

        verify { appStore.dispatch(AppAction.MessagingAction.MessageDismissed(message)) }
    }

    @Test
    fun `GIVEN has utmContent WHEN calling onPrivacyPolicyLinkClicked THEN open the privacy URL appended with the utmContent in a new tab`() {
        microsurveyMessageController.onPrivacyPolicyLinkClicked(message.id, "homepage")

        verify {
            homeActivity.openToBrowserAndLoad(
                searchTermOrURL = "$PRIVACY_POLICY_URL&utm_content=homepage",
                newTab = true,
                from = BrowserDirection.FromGlobal,
            )
        }
    }

    @Test
    fun `GIVEN no utmContent WHEN calling onPrivacyPolicyLinkClicked THEN open the privacy URL in a new tab`() {
        microsurveyMessageController.onPrivacyPolicyLinkClicked(message.id)

        verify { appStore.dispatch(OnPrivacyNoticeTapped(message.id)) }
        verify {
            homeActivity.openToBrowserAndLoad(
                searchTermOrURL = PRIVACY_POLICY_URL,
                newTab = true,
                from = BrowserDirection.FromGlobal,
            )
        }
    }

    @Test
    fun `WHEN calling onSurveyCompleted THEN update the app store with the SurveyCompleted action`() {
        val answer = "satisfied"
        microsurveyMessageController.onSurveyCompleted(message.id, answer)

        verify { appStore.dispatch(SentConfirmationShown(message.id)) }
        verify { appStore.dispatch(Completed(message.id, answer)) }
    }

    @Test
    fun `WHEN calling onMicrosurveyShown THEN update the app store with the Survey Shown action`() {
        microsurveyMessageController.onMicrosurveyShown(message.id)

        verify { appStore.dispatch(Shown(message.id)) }
    }

    @Test
    fun `WHEN calling onMicrosurveyDismissed THEN update the app store with the Survey Dismissed action`() {
        microsurveyMessageController.onMicrosurveyDismissed(message.id)

        verify { appStore.dispatch(Dismissed(message.id)) }
    }
}
