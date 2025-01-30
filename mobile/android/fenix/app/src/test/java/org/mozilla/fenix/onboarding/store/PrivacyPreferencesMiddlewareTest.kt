/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.flow.emptyFlow
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoInteractions
import org.mockito.Mockito.verifyNoMoreInteractions
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations

@RunWith(AndroidJUnit4::class)
class PrivacyPreferencesMiddlewareTest {

    @Mock
    private lateinit var repository: PrivacyPreferencesRepository

    @Mock
    private lateinit var context: MiddlewareContext<PrivacyPreferencesState, PrivacyPreferencesAction>

    private lateinit var middleware: PrivacyPreferencesMiddleware

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        repository = mock()
        middleware = PrivacyPreferencesMiddleware(repository)
    }

    @Test
    fun `GIVEN init called with no preferences WHEN middleware is invoked THEN only the repo is initialized`() {
        `when`(repository.privacyPreferenceUpdates).thenReturn(emptyFlow())

        middleware.invoke(context, {}, PrivacyPreferencesAction.Init)

        verifyNoInteractions(context)
        verify(repository).init()
    }

    @Test
    fun `GIVEN init called with preferences WHEN middleware is invoked THEN the repo is initialized`() {
        middleware.invoke(context, {}, PrivacyPreferencesAction.Init)

        verify(repository).init()
        verifyNoMoreInteractions(repository)
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
    fun `GIVEN usage data learn more called WHEN middleware is invoked THEN the repo is unchanged`() {
        val action = PrivacyPreferencesAction.UsageDataUserLearnMore
        middleware.invoke(context, {}, action)

        verifyNoInteractions(repository)
    }

    @Test
    fun `GIVEN crash reporting learn more called WHEN middleware is invoked THEN the repo is unchanged`() {
        val action = PrivacyPreferencesAction.CrashReportingLearnMore
        middleware.invoke(context, {}, action)

        verifyNoInteractions(repository)
    }
}
