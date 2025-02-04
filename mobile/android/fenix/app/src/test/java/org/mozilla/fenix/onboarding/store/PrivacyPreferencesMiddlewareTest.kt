/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase.assertEquals
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.verifyNoInteractions
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
    fun `WHEN data usage update action is invoked THEN new value is set in repository`() {
        var dataUsageEnabled = true
        val middleware = PrivacyPreferencesMiddleware(
            repository = object : PrivacyPreferencesRepository {
                override fun getPreference(type: PreferenceType) = false

                override fun setPreference(type: PreferenceType, enabled: Boolean) {
                    dataUsageEnabled = enabled
                }
            },
        )

        val updatedDataUsageEnabled = !dataUsageEnabled
        middleware.invoke(context, {}, PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo(enabled = updatedDataUsageEnabled))

        assertEquals(updatedDataUsageEnabled, dataUsageEnabled)
    }

    @Test
    fun `WHEN crash reporting update action is invoked THEN new value is set in repository`() {
        var crashReportEnabled = true
        val middleware = PrivacyPreferencesMiddleware(
            repository = object : PrivacyPreferencesRepository {
                override fun getPreference(type: PreferenceType) = false

                override fun setPreference(type: PreferenceType, enabled: Boolean) {
                    crashReportEnabled = enabled
                }
            },
        )

        val updatedCrashReportEnabled = !crashReportEnabled
        middleware.invoke(context, {}, PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo(enabled = updatedCrashReportEnabled))

        assertEquals(updatedCrashReportEnabled, crashReportEnabled)
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
