/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import mozilla.components.service.nimbus.messaging.Message
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageClicked
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MessageDismissed
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction.MicrosurveyAction.Completed
import org.mozilla.fenix.settings.SupportUtils

private val PRIVACY_POLICY_URL =
    SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE) +
        "?utm_medium=firefox-mobile&utm_source=modal&utm_campaign=microsurvey"

/**
 * Handles interactions with a microsurvey.
 */
class MicrosurveyMessageController(
    private val appStore: AppStore,
    private val homeActivity: HomeActivity,
) : MessageController {

    override fun onMessagePressed(message: Message) {
        appStore.dispatch(MessageClicked(message))
    }

    override fun onMessageDismissed(message: Message) {
        appStore.dispatch(MessageDismissed(message))
    }

    /**
     * Handles the click event on the privacy link within a message.
     * @param utmContent Optional utm parameter to add to the privacy policy URL.
     */
    fun onPrivacyPolicyLinkClicked(utmContent: String? = null) {
        val url = getPrivacyPolicyUrlFor(utmContent)

        homeActivity.openToBrowserAndLoad(
            searchTermOrURL = url,
            newTab = true,
            from = BrowserDirection.FromGlobal,
        )
    }

    private fun getPrivacyPolicyUrlFor(utmContent: String?) = if (utmContent == null) {
        PRIVACY_POLICY_URL
    } else {
        "$PRIVACY_POLICY_URL&utm_content=$utmContent"
    }

    /**
     * Dispatches an action when a survey is completed.
     * @param message The message containing the completed survey.
     * @param answer The answer provided in the survey.
     */
    fun onSurveyCompleted(message: Message, answer: String) {
        appStore.dispatch(Completed(message, answer))
    }
}
