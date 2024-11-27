/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.state.MiddlewareContext
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.MockitoAnnotations
import org.mozilla.fenix.GleanMetrics.Onboarding

@RunWith(AndroidJUnit4::class)
class PrivacyPreferencesTelemetryMiddlewareTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Mock
    private lateinit var context: MiddlewareContext<PrivacyPreferencesState, PrivacyPreferencesAction>

    private lateinit var middleware: PrivacyPreferencesTelemetryMiddleware

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        middleware = PrivacyPreferencesTelemetryMiddleware()
    }

    @Test
    fun `GIVEN crash reporting enabled action WHEN middleware is invoked THEN the corresponding telemetry is sent`() {
        assertNull(Onboarding.privacyPreferencesModalCrashReportingEnabled.testGetValue())

        middleware.invoke(
            context,
            {},
            PrivacyPreferencesAction.CrashReportingPreferenceUpdatedTo(true),
        )

        val event = Onboarding.privacyPreferencesModalCrashReportingEnabled.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("value").toBoolean()
        assertTrue(result)
    }

    @Test
    fun `GIVEN usage data enabled action WHEN middleware is invoked THEN the corresponding telemetry is sent`() {
        assertNull(Onboarding.privacyPreferencesModalUsageDataEnabled.testGetValue())

        middleware.invoke(
            context,
            {},
            PrivacyPreferencesAction.UsageDataPreferenceUpdatedTo(true),
        )

        val event = Onboarding.privacyPreferencesModalUsageDataEnabled.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("value").toBoolean()
        assertTrue(result)
    }

    @Test
    fun `GIVEN crash reporting checked action WHEN middleware is invoked THEN the corresponding telemetry is sent`() {
        assertNull(Onboarding.privacyPreferencesModalCrashReportingChecked.testGetValue())

        middleware.invoke(
            context,
            {},
            PrivacyPreferencesAction.CrashReportingChecked(true),
        )

        val event = Onboarding.privacyPreferencesModalCrashReportingChecked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("checked").toBoolean()
        assertTrue(result)
    }

    @Test
    fun `GIVEN usage data checked action WHEN middleware is invoked THEN the corresponding telemetry is sent`() {
        assertNull(Onboarding.privacyPreferencesModalUsageDataChecked.testGetValue())

        middleware.invoke(
            context,
            {},
            PrivacyPreferencesAction.UsageDataUserChecked(true),
        )

        val event = Onboarding.privacyPreferencesModalUsageDataChecked.testGetValue()!!
        assertNotNull(event)
        assertEquals(1, event.size)
        val result = event.single().extra?.getValue("checked").toBoolean()
        assertTrue(result)
    }
}
