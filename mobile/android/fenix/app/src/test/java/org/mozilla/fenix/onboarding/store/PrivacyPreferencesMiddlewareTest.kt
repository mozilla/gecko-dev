/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.flow.asFlow
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
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations

@RunWith(AndroidJUnit4::class)
class PrivacyPreferencesMiddlewareTest {

    @get:Rule
    val mainCoroutineTestRule = MainCoroutineRule()

    @Mock
    private lateinit var repository: PrivacyPreferencesRepository

    @Mock
    private lateinit var context: MiddlewareContext<PrivacyPreferencesState, PrivacyPreferencesAction>

    @Mock
    private lateinit var store: PrivacyPreferencesStore

    private lateinit var middleware: PrivacyPreferencesMiddleware

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        repository = mock()
        middleware = PrivacyPreferencesMiddleware(repository)
    }

    @Test
    fun `GIVEN init called with no preferences WHEN middleware is invoked THEN only the repo is initialized`() =
        runTestOnMain {
            `when`(repository.privacyPreferenceUpdates).thenReturn(emptyFlow())

            middleware.invoke(context, {}, PrivacyPreferencesAction.Init)

            verifyNoInteractions(context)
            verify(repository).init()
        }

    @Test
    fun `GIVEN init called with preferences WHEN middleware is invoked THEN the store is updated and repo is initialized`() =
        runTestOnMain {
            val preferenceUpdates = PrivacyPreferencesRepository.PrivacyPreference.entries.map {
                PrivacyPreferencesRepository.PrivacyPreferenceUpdate(it, false)
            }

            `when`(repository.privacyPreferenceUpdates).thenReturn(preferenceUpdates.asFlow())
            `when`(context.store).thenReturn(store)

            middleware.invoke(context, {}, PrivacyPreferencesAction.Init)

            val expectedFirstAction =
                PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo(preferenceUpdates[0].value)
            verify(context.store).dispatch(expectedFirstAction)

            val expectedSecondAction =
                PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo(preferenceUpdates[1].value)
            verify(context.store).dispatch(expectedSecondAction)

            verify(repository).init()
        }

    @Test
    fun `GIVEN crash reporting called WHEN middleware is invoked THEN the repo is updated`() {
        val action = PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo(true)
        middleware.invoke(context, {}, action)

        verify(repository).updatePrivacyPreference(
            PrivacyPreferencesRepository.PrivacyPreferenceUpdate(
                preferenceType = PrivacyPreferencesRepository.PrivacyPreference.CrashReporting,
                value = action.enabled,
            ),
        )
    }

    @Test
    fun `GIVEN usage data called WHEN middleware is invoked THEN the repo is updated`() {
        val action = PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo(true)
        middleware.invoke(context, {}, action)

        verify(repository).updatePrivacyPreference(
            PrivacyPreferencesRepository.PrivacyPreferenceUpdate(
                preferenceType = PrivacyPreferencesRepository.PrivacyPreference.UsageData,
                value = action.enabled,
            ),
        )
    }

    @Test
    fun `GIVEN crash reporting checked called WHEN middleware is invoked THEN the repo is unchanged`() {
        val action = PrivacyPreferencesAction.CrashReportingChecked(true)
        middleware.invoke(context, {}, action)

        verifyNoInteractions(repository)
    }

    @Test
    fun `GIVEN usage data checked called WHEN middleware is invoked THEN the repo is unchanged`() {
        val action = PrivacyPreferencesAction.UsageDataUserChecked(true)
        middleware.invoke(context, {}, action)

        verifyNoInteractions(repository)
    }
}
