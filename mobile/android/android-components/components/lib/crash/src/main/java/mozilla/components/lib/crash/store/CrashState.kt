/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.store

typealias TimeInMillis = Long

/**
 * The state of the crash reporter.
 */
sealed class CrashState {
    /**
     * Crash reporter is currently idle.
     */
    data object Idle : CrashState()

    /**
     * Defer showing the crash reporter
     *
     * @param until [TimeInMillis] when to next show the crash reporter.
     */
    data class Deferred(val until: TimeInMillis) : CrashState()

    /**
     * Crash Reporter is ready to send any unsent crashes
     */
    data object Ready : CrashState()

    /**
     * Crash reporter is presenting UI to the user to send unsent crash reports.
     */
    data object Reporting : CrashState()

    /**
     * Crash reporter is done.
     */
    data object Done : CrashState()
}
