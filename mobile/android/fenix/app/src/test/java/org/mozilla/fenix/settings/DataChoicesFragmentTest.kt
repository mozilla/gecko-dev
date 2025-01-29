/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class DataChoicesFragmentTest {

    private val fragment = DataChoicesFragment()
    private lateinit var settings: Settings

    @Before
    fun setup() {
        settings = Settings(testContext)
    }

    @Test
    fun `GIVEN the user has not seen the marketing telemetry screen WHEN shouldShowMarketingTelemetryPreference THEN returns false`() {
        settings.hasMadeMarketingTelemetrySelection = false

        val result = fragment.shouldShowMarketingTelemetryPreference(settings)

        assertFalse(result)
    }

    @Test
    fun `GIVEN the user has seen the marketing telemetry screen WHEN shouldShowMarketingTelemetryPreference THEN returns true`() {
        settings.hasMadeMarketingTelemetrySelection = true

        val result = fragment.shouldShowMarketingTelemetryPreference(settings)

        assertTrue(result)
    }
}
