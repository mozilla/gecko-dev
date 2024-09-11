/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.store

/**
 * Actions for running the crash reporter.
 */
sealed class CrashAction {
    /**
     * [CrashAction] to initialize the crash reporter.
     */
    data object Initialize : CrashAction()

    /**
     * [CrashAction] to check if we have a stored deferred value.
     */
    data object CheckDeferred : CrashAction()

    /**
     * [CrashAction] to restore a stored deferred value.
     */
    data class RestoreDeferred(val now: TimeInMillis, val until: TimeInMillis) : CrashAction()

    /**
     * [CrashAction] to check if we have any unsent crashes.
     */
    data object CheckForCrashes : CrashAction()

    /**
     * [CrashAction] to return the result of [CheckForCrashes].
     */
    data class FinishCheckingForCrashes(val hasUnsentCrashes: Boolean) : CrashAction()

    /**
     * [CrashAction] to defer sending crashes until some point in the future.
     */
    data class Defer(val now: TimeInMillis) : CrashAction()

    /**
     * [CrashAction] to send when a user taps the cancel button.
     */
    data object CancelTapped : CrashAction()

    /**
     * [CrashAction] to send when a user taps the send report button.
     */
    data object ReportTapped : CrashAction()
}
