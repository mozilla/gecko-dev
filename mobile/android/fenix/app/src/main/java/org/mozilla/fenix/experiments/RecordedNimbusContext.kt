/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.experiments

import org.json.JSONObject
import org.mozilla.experiments.nimbus.internal.JsonObject
import org.mozilla.experiments.nimbus.internal.RecordedContext
import org.mozilla.fenix.GleanMetrics.NimbusSystem

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
    val utmSource: String,
    val utmMedium: String,
    val utmCampaign: String,
    val utmTerm: String,
    val utmContent: String,
) : RecordedContext {
    override fun record() {
        NimbusSystem.recordedNimbusContext.set(
            NimbusSystem.RecordedNimbusContextObject(
                isFirstRun = isFirstRun,
                installReferrerResponseUtmSource = utmSource,
                installReferrerResponseUtmMedium = utmMedium,
                installReferrerResponseUtmCampaign = utmCampaign,
                installReferrerResponseUtmTerm = utmTerm,
                installReferrerResponseUtmContent = utmContent,
            ),
        )
    }

    override fun toJson(): JsonObject {
        val obj = JSONObject()
        obj.put("is_first_run", isFirstRun)
        obj.put("install_referrer_response_utm_source", utmSource)
        obj.put("install_referrer_response_utm_medium", utmMedium)
        obj.put("install_referrer_response_utm_campaign", utmCampaign)
        obj.put("install_referrer_response_utm_term", utmTerm)
        obj.put("install_referrer_response_utm_content", utmContent)
        return obj
    }
}
