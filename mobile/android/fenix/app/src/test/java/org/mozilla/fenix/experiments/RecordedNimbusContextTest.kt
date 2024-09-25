/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
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
    fun `GIVEN an instance of RecordedNimbusContext WHEN serialized to JSON THEN its JSON structure matches the expected value`() {
        val context = RecordedNimbusContext(
            isFirstRun = true,
        )

        // RecordedNimbusContext.toJson() returns
        // org.mozilla.experiments.nimbus.internal.JsonObject, which is a
        // different type.
        val contextAsJson = Json.decodeFromString<JsonObject>(context.toJson().toString())

        assertEquals(
            contextAsJson,
            buildJsonObject {
                put("is_first_run", true)
            },
        )
    }

    @Test
    fun `GIVEN an instance of RecordedNimbusContext WHEN record called THEN the value recorded to Glean should match the expected value`() {
        RecordedNimbusContext(
            isFirstRun = true,
        ).record()

        val recordedValue = GleanNimbus.recordedNimbusContext.testGetValue()

        assertNotNull(recordedValue)
        assertEquals(
            recordedValue.jsonObject,
            buildJsonObject {
                put("isFirstRun", true)
            },
        )
    }
}
