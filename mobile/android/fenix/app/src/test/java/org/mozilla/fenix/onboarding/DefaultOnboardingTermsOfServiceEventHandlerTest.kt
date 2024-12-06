/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import io.mockk.mockk
import io.mockk.verify
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DefaultOnboardingTermsOfServiceEventHandlerTest {

    private lateinit var eventHandler: DefaultOnboardingTermsOfServiceEventHandler
    private lateinit var telemetryRecorder: OnboardingTelemetryRecorder
    private lateinit var openLink: (String) -> Unit
    private lateinit var showManagePrivacyPreferencesDialog: () -> Unit

    @Before
    fun setup() {
        telemetryRecorder = mockk(relaxed = true)
        openLink = mockk(relaxed = true)
        showManagePrivacyPreferencesDialog = mockk(relaxed = true)

        eventHandler = DefaultOnboardingTermsOfServiceEventHandler(
            telemetryRecorder = telemetryRecorder,
            openLink = openLink,
            showManagePrivacyPreferencesDialog = showManagePrivacyPreferencesDialog,
        )
    }

    @Test
    fun onTermsOfServiceLinkClicked() {
        val url = "terms-of-services"

        eventHandler.onTermsOfServiceLinkClicked(url)

        verify {
            telemetryRecorder.onTermsOfServiceLinkClick()
        }
        verify {
            openLink(url)
        }
    }

    @Test
    fun onPrivacyNoticeLinkClicked() {
        val url = "privacy-notice"

        eventHandler.onPrivacyNoticeLinkClicked(url)

        verify {
            telemetryRecorder.onTermsOfServicePrivacyNoticeLinkClick()
        }
        verify {
            openLink(url)
        }
    }

    @Test
    fun onManagePrivacyPreferencesLinkClicked() {
        eventHandler.onManagePrivacyPreferencesLinkClicked()

        verify {
            telemetryRecorder.onTermsOfServiceManagePrivacyPreferencesLinkClick()
        }
        verify {
            showManagePrivacyPreferencesDialog()
        }
    }

    @Test
    fun onAcceptTermsButtonClicked() {
        eventHandler.onAcceptTermsButtonClicked()

        verify {
            telemetryRecorder.onTermsOfServiceManagerAcceptTermsButtonClick()
        }
    }
}
