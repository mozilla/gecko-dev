/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.flow.emptyFlow
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations
import org.mozilla.fenix.onboarding.view.ThemeOptionType
import org.mozilla.fenix.onboarding.view.ToolbarOptionType

@RunWith(AndroidJUnit4::class)
class OnboardingPreferencesMiddlewareTest {

    @get:Rule
    val mainCoroutineTestRule = MainCoroutineRule()

    @Mock
    private lateinit var repository: OnboardingPreferencesRepository

    @Mock
    private lateinit var context: MiddlewareContext<OnboardingState, OnboardingAction>

    private lateinit var middleware: OnboardingPreferencesMiddleware

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        repository = mock()
        middleware = OnboardingPreferencesMiddleware(repository)
    }

    @Test
    fun `GIVEN init action WHEN middleware is invoked THEN the repo is initialized`() =
        runTestOnMain {
            `when`(repository.onboardingPreferenceUpdates).thenReturn(emptyFlow())
            middleware.invoke(context = context, next = {}, action = OnboardingAction.Init())

            verify(repository).init()
            verify(repository).onboardingPreferenceUpdates
            verifyNoMoreInteractions(repository)
        }

    @Test
    fun `GIVEN update selected theme action with WHEN middleware is invoked THEN the repo update function is called with the selected theme`() =
        runTestOnMain {
            middleware.invoke(
                context = context,
                next = {},
                action = OnboardingAction.OnboardingThemeAction.UpdateSelected(ThemeOptionType.THEME_DARK),
            )

            verify(repository).updateOnboardingPreference(
                OnboardingPreferencesRepository.OnboardingPreferenceUpdate(
                    OnboardingPreferencesRepository.OnboardingPreference.DarkTheme,
                ),
            )
            verifyNoMoreInteractions(repository)
        }

    @Test
    fun `GIVEN no op actions with WHEN middleware is invoked THEN nothing happens`() =
        runTestOnMain {
            middleware.invoke(
                context = context,
                next = {},
                action = OnboardingAction.OnboardingAddOnsAction.UpdateStatus(
                    addOnId = "test",
                    status = OnboardingAddonStatus.INSTALLED,
                ),
            )

            middleware.invoke(
                context = context,
                next = {},
                action = OnboardingAction.OnboardingToolbarAction.UpdateSelected(ToolbarOptionType.TOOLBAR_TOP),
            )

            verifyNoInteractions(repository)
        }
}
