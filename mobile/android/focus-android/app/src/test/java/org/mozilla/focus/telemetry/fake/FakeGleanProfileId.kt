/*
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.focus.telemetry.fake

import org.mozilla.focus.telemetry.GleanUsageReportingMetricsService
import java.util.UUID

/**
 * Allows tests to insert their own version of a GleanProfileId
 * and make assertions against it
 */
class FakeGleanProfileId : GleanUsageReportingMetricsService.GleanProfileId {
    var gleanStoredProfileId: UUID? = null
    var generatedProfileId: UUID = UUID.randomUUID()

    override fun generateAndSet(): UUID {
        gleanStoredProfileId = generatedProfileId
        return generatedProfileId
    }
    override fun set(profileId: UUID) { gleanStoredProfileId = profileId }
    override fun unset() { gleanStoredProfileId = null }
}
