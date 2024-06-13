/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.GleanMetrics.NimbusSystem as GleanNimbus

@RunWith(FenixRobolectricTestRunner::class)
class RecordedNimbusContextTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `GIVEN an instance of RecordedNimbusContext WHEN record called THEN its JSON structure and the value recorded to Glean should be the same`() {
        val context = RecordedNimbusContext(
            isFirstRun = true,
        )

        context.record()

        val value = GleanNimbus.recordedNimbusContext.testGetValue()

        assertEquals(Json.decodeFromString<JsonObject>(context.toJson().toString()), value)
    }
}
