/*
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.focus.telemetry.fake

import org.mozilla.focus.telemetry.GleanUsageReportingMetricsService

/**
 * Allows tests to insert their own version of a GleanProfileIdStore
 * and make assertions against it
 */
class FakeGleanProfileIdStore : GleanUsageReportingMetricsService.GleanProfileIdStore {
    var appStoredProfileId: String? = null
    override var profileId: String?
        get() = appStoredProfileId
        set(value) { appStoredProfileId = value }

    override fun clear() {
        appStoredProfileId = null
    }
}
