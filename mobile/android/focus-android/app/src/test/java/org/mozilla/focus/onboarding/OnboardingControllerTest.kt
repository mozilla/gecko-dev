/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.onboarding

import android.content.Context
import android.os.Build
import mozilla.components.support.test.whenever
import org.junit.Before
import org.junit.Test
import org.mockito.Mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations
import org.mozilla.focus.Components
import org.mozilla.focus.FocusApplication
import org.mozilla.focus.fragment.onboarding.DefaultOnboardingController
import org.mozilla.focus.fragment.onboarding.OnboardingController
import org.mozilla.focus.fragment.onboarding.OnboardingStorage
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.utils.Settings
import org.robolectric.annotation.Config

class OnboardingControllerTest {

    @Mock
    private lateinit var appStore: AppStore

    @Mock
    private lateinit var context: Context

    @Mock
    private lateinit var appContext: FocusApplication

    @Mock
    private lateinit var components: Components

    @Mock
    private lateinit var settings: Settings

    @Mock
    private lateinit var onboardingStorage: OnboardingStorage
    private lateinit var onboardingController: OnboardingController

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)

        whenever(context.applicationContext).thenReturn(appContext)
        whenever(appContext.components).thenReturn(components)
        whenever(components.settings).thenReturn(settings)

        onboardingController = spy(
            DefaultOnboardingController(
                onboardingStorage,
                appStore,
                context,
                "1",
            ),
        )
    }

    @Test
    fun `GIVEN onBoarding, WHEN start browsing is pressed, THEN onBoarding flag is true`() {
        onboardingController.handleFinishOnBoarding()

        verify(settings).isFirstRun = false
        verify(appStore).dispatch(AppAction.FinishFirstRun("1"))
    }

    @Config(sdk = [Build.VERSION_CODES.M])
    @Test
    fun `GIVEN onBoarding and build version is M, WHEN get started button is pressed, THEN onBoarding flow must end`() {
        onboardingController.handleGetStartedButtonClicked()

        verify(onboardingController, times(1)).handleFinishOnBoarding()
    }
}
