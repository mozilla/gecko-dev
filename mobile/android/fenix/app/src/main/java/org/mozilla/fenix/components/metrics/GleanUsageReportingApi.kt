/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

/**
 * Defines the possible interactions with the Glean usage-reporting ping.
 */
interface GleanUsageReportingApi {

    /**
     * This marks whether the ping is being activated or inactivated.
     */
    enum class UsageReason { ACTIVE, INACTIVE }

    /**
     * The usage reason should be set here before the ping is sent.
     * @param usageReason the new reason state of the usage ping.
     */
    fun setUsageReason(usageReason: UsageReason)

    /**
     * The duration should be set here before the ping is sent.
     * @param durationMillis the duration in milliseconds of the last foreground session
     */
    fun setDuration(durationMillis: Long)

    /**
     * Send the ping to Glean.
     */
    fun submitPing()
}
