/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import mozilla.components.lib.crash.store.CrashReportCache
import mozilla.components.lib.crash.store.TimeInMillis
import org.mozilla.fenix.utils.Settings

/**
 * A disk cache for handling data related to Crash Reports.
 *
 * @param settings Convenience delegate for Shared Preferences.
 */
class SettingsCrashReportCache(private val settings: Settings) : CrashReportCache {
    override suspend fun getCutoffDate(): TimeInMillis? =
        settings.crashReportCutoffDate.takeIf { it != 0L }

    override suspend fun setCutoffDate(timeInMillis: TimeInMillis?) {
        settings.crashReportCutoffDate = timeInMillis ?: 0
    }

    override suspend fun getDeferredUntil(): TimeInMillis? =
        settings.crashReportDeferredUntil.takeIf { it != 0L }

    override suspend fun setDeferredUntil(timeInMillis: TimeInMillis?) {
        settings.crashReportDeferredUntil = timeInMillis ?: 0
    }

    override suspend fun getAlwaysSend(): Boolean = settings.crashReportAlwaysSend

    override suspend fun setAlwaysSend(alwaysSend: Boolean) {
        settings.crashReportAlwaysSend = alwaysSend
    }
}
