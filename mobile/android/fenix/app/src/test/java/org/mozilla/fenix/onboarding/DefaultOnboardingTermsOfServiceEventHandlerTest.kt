/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class DefaultOnboardingTermsOfServiceEventHandlerTest {

    private lateinit var eventHandler: DefaultOnboardingTermsOfServiceEventHandler
    private lateinit var telemetryRecorder: OnboardingTelemetryRecorder
    private lateinit var openLink: (String) -> Unit
    private lateinit var showManagePrivacyPreferencesDialog: () -> Unit
    private lateinit var settings: Settings

    @Before
    fun setup() {
        telemetryRecorder = mockk(relaxed = true)
        openLink = mockk(relaxed = true)
        showManagePrivacyPreferencesDialog = mockk(relaxed = true)
        settings = Settings(testContext)

        eventHandler = DefaultOnboardingTermsOfServiceEventHandler(
            telemetryRecorder = telemetryRecorder,
            openLink = openLink,
            showManagePrivacyPreferencesDialog = showManagePrivacyPreferencesDialog,
            settings = settings,
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

        assert(settings.hasAcceptedTermsOfService)
    }
}
