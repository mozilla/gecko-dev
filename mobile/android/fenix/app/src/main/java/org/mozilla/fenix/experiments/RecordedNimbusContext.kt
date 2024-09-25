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
) : RecordedContext {
    override fun record() {
        NimbusSystem.recordedNimbusContext.set(
            NimbusSystem.RecordedNimbusContextObject(
                isFirstRun = isFirstRun,
            ),
        )
    }

    override fun toJson(): JsonObject {
        val obj = JSONObject()
        obj.put("isFirstRun", isFirstRun)
        return obj
    }
}
