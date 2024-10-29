/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonObject
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.internal.validateEventQueries
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.GleanMetrics.NimbusSystem as GleanNimbus

@RunWith(FenixRobolectricTestRunner::class)
class RecordedNimbusContextTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `GIVEN a nimbusApi object WHEN recorded context with eventQueries is supplied THEN the event queries must be valid`() {
        val context = RecordedNimbusContext.createForTest()
        validateEventQueries(context)
    }

    @Test
    fun `GIVEN an instance of RecordedNimbusContext WHEN serialized to JSON THEN its JSON structure matches the expected value`() {
        val recordedContext = RecordedNimbusContext.createForTest(
            eventQueryValues = mapOf(
                "TEST" to 1.0,
            ),
        )

        // RecordedNimbusContext.toJson() returns
        // org.mozilla.experiments.nimbus.internal.JsonObject, which is a
        // different type.
        val contextAsJson = Json.decodeFromString<JsonObject>(recordedContext.toJson().toString())

        assertEquals(
            buildJsonObject {
                put("is_first_run", false)
                putJsonObject("events") {
                    put("TEST", 1)
                }
                put("install_referrer_response_utm_source", "")
                put("install_referrer_response_utm_medium", "")
                put("install_referrer_response_utm_campaign", "")
                put("install_referrer_response_utm_term", "")
                put("install_referrer_response_utm_content", "")
            },
            contextAsJson,
        )
    }

    @Test
    fun `GIVEN an instance of RecordedNimbusContext WHEN record called THEN the value recorded to Glean should match the expected value`() {
        val recordedContext = RecordedNimbusContext.createForTest()
        recordedContext.setEventQueryValues(
            mapOf(
                DAYS_OPENED_IN_LAST_28 to 1.5,
            ),
        )
        recordedContext.record()

        val recordedValue = GleanNimbus.recordedNimbusContext.testGetValue()

        assertNotNull(recordedValue)
        assertEquals(
            buildJsonObject {
                putJsonObject("eventQueryValues") {
                    put("daysOpenedInLast28", 1)
                }
                put("installReferrerResponseUtmSource", "")
                put("installReferrerResponseUtmMedium", "")
                put("installReferrerResponseUtmCampaign", "")
                put("installReferrerResponseUtmTerm", "")
                put("installReferrerResponseUtmContent", "")
                put("isFirstRun", false)
            },
            recordedValue?.jsonObject,
        )
    }

    @Test
    fun `GIVEN an instance of RecordedNimbusContext WHEN eventQueries have been supplied THEN getEventQueries should return a JSON object with the eventQueries`() {
        val query = "'event'|eventSum('Years', 1, 0)"
        val context = RecordedNimbusContext.createForTest(
            eventQueries = mutableMapOf(
                "TEST" to query,
            ),
        )

        assertEquals(query, context.getEventQueries()["TEST"])
    }

    @Test
    fun `GIVEN an instance of RecordedNimbusContext WHEN eventQueries have been supplied THEN setEventQueryValues should set the values for the eventQueries`() {
        val context = RecordedNimbusContext.createForTest(
            eventQueries = mapOf(
                "TEST" to "'event'|eventSum('Years', 1, 0)",
            ),
        )

        context.setEventQueryValues(mapOf("TEST" to 1.0))

        assertEquals(1.0, context.toJson().getJSONObject("events").get("TEST"))
    }
}
