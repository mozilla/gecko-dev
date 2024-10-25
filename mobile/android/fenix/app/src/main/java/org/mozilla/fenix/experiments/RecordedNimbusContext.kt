/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

import android.content.Context
import androidx.annotation.VisibleForTesting
import org.json.JSONObject
import org.mozilla.experiments.nimbus.internal.JsonObject
import org.mozilla.experiments.nimbus.internal.RecordedContext
import org.mozilla.fenix.GleanMetrics.NimbusSystem
import org.mozilla.fenix.ext.settings

/**
 * The following constants are string constants of the keys that appear in the [EVENT_QUERIES] map.
 */
const val DAYS_OPENED_IN_LAST_28 = "days_opened_in_last_28"

/**
 * [EVENT_QUERIES] is a map of keys to Nimbus SDK EventStore queries.
 */
private val EVENT_QUERIES = mapOf(
    DAYS_OPENED_IN_LAST_28 to "'events.app_opened'|eventCountNonZero('Days', 28, 0)",
)

/**
 * The RecordedNimbusContext class inherits from an internal Nimbus interface that provides methods
 * for obtaining a JSON value for the object and recording the object's value to Glean. Its JSON
 * value is loaded into the Nimbus targeting context.
 *
 * The value recorded to Glean is used to automate population sizing. Any additions to this object
 * require a new data review for the `nimbus_system.recorded_nimbus_context` metric.
 */
class RecordedNimbusContext(
    val isFirstRun: Boolean,
    private val eventQueries: Map<String, String> = mapOf(),
    private var eventQueryValues: Map<String, Double> = mapOf(),
    val utmSource: String,
    val utmMedium: String,
    val utmCampaign: String,
    val utmTerm: String,
    val utmContent: String,
) : RecordedContext {
    /**
     * [getEventQueries] is called by the Nimbus SDK Rust code to retrieve the map of event
     * queries. The are then executed against the Nimbus SDK's EventStore to retrieve their values.
     *
     * @return Map<String, String>
     */
    override fun getEventQueries(): Map<String, String> {
        return eventQueries
    }

    /**
     * [record] is called when experiment enrollments are evolved. It should apply the
     * [RecordedNimbusContext]'s values to a [NimbusSystem.RecordedNimbusContextObject] instance,
     * and use that instance to record the values to Glean.
     */
    override fun record() {
        val eventQueryValuesObject = NimbusSystem.RecordedNimbusContextObjectItemEventQueryValuesObject(
            daysOpenedInLast28 = eventQueryValues[DAYS_OPENED_IN_LAST_28]?.toInt(),
        )
        NimbusSystem.recordedNimbusContext.set(
            NimbusSystem.RecordedNimbusContextObject(
                isFirstRun = isFirstRun,
                eventQueryValues = eventQueryValuesObject,
                installReferrerResponseUtmSource = utmSource,
                installReferrerResponseUtmMedium = utmMedium,
                installReferrerResponseUtmCampaign = utmCampaign,
                installReferrerResponseUtmTerm = utmTerm,
                installReferrerResponseUtmContent = utmContent,
            ),
        )
    }

    /**
     * [setEventQueryValues] is called by the Nimbus SDK Rust code after the event queries have been
     * executed. The [eventQueryValues] should be written back to the Kotlin object.
     *
     * @param [eventQueryValues] The values for each query after they have been executed in the
     * Nimbus SDK Rust environment.
     */
    override fun setEventQueryValues(eventQueryValues: Map<String, Double>) {
        this.eventQueryValues = eventQueryValues
    }

    /**
     * [toJson] is called by the Nimbus SDK Rust code after the event queries have been executed,
     * and before experiment enrollments have been evolved. The value returned from this method
     * will be applied directly to the Nimbus targeting context, and its keys/values take
     * precedence over those in the main Nimbus targeting context.
     *
     * @return JSONObject
     */
    override fun toJson(): JsonObject {
        val obj = JSONObject(
            mapOf(
                "is_first_run" to isFirstRun,
                "events" to JSONObject(eventQueryValues),
                "install_referrer_response_utm_source" to utmSource,
                "install_referrer_response_utm_medium" to utmMedium,
                "install_referrer_response_utm_campaign" to utmCampaign,
                "install_referrer_response_utm_term" to utmTerm,
                "install_referrer_response_utm_content" to utmContent,
            ),
        )
        return obj
    }

    /**
     * Companion object for RecordedNimbusContext
     */
    companion object {

        /**
         * Creates a RecordedNimbusContext instance, populated with the application-defined
         * eventQueries
         *
         * @return RecordedNimbusContext
         */
        fun create(
            context: Context,
            isFirstRun: Boolean,
        ): RecordedNimbusContext {
            return RecordedNimbusContext(
                isFirstRun = isFirstRun,
                eventQueries = EVENT_QUERIES,
                utmSource = context.settings().utmSource,
                utmMedium = context.settings().utmMedium,
                utmCampaign = context.settings().utmCampaign,
                utmTerm = context.settings().utmTerm,
                utmContent = context.settings().utmContent,
            )
        }

        /**
         * Creates a RecordedNimbusContext instance for test purposes
         *
         * @return RecordedNimbusContext
         */
        @VisibleForTesting
        internal fun createForTest(
            isFirstRun: Boolean = false,
            eventQueries: Map<String, String> = EVENT_QUERIES,
            eventQueryValues: Map<String, Double> = mapOf(),
        ): RecordedNimbusContext {
            return RecordedNimbusContext(
                isFirstRun = isFirstRun,
                eventQueries = eventQueries,
                eventQueryValues = eventQueryValues,
                utmSource = "",
                utmMedium = "",
                utmCampaign = "",
                utmTerm = "",
                utmContent = "",
            )
        }
    }
}
