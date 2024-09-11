/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.store

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.lib.crash.CrashReporter

/**
 * An interface to store and retrieve a timestamp to defer submitting unsent crashes until.
 */
interface CrashReportCache {

    /**
     * Gets the stored deferred timestamp.
     */
    suspend fun getDeferredUntil(): TimeInMillis?

    /**
     * Stores a deferred timestamp.
     */
    suspend fun setDeferredUntil(timeInMillis: TimeInMillis?)
}

/**
 * Middleware for the crash reporter.
 *
 * @param cache stored values for getting/setting deferredUntil.
 * @param crashReporter instance of [CrashReporter] for checking for and sending unsent crashes.
 * @param currentTimeInMillis get the current time in milliseconds.
 * @param scope [CoroutineScope] to run suspended functions on.
 */
class CrashMiddleware(
    private val cache: CrashReportCache,
    private val crashReporter: CrashReporter,
    private val currentTimeInMillis: () -> TimeInMillis = { System.currentTimeMillis() },
    private val scope: CoroutineScope,
) {

    /**
     * Handle any middleware logic before an action reaches the [crashReducer].
     *
     * @param middlewareContext accessors for the current [CrashState] and dispatcher from the store.
     * @param next The next middleware in the chain.
     * @param action The current [CrashAction] to process in the middleware.
     */
    fun invoke(
        middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit>,
        next: (CrashAction) -> Unit,
        action: CrashAction,
    ) {
        val (getState, dispatch) = middlewareContext

        next(action)

        when (action) {
            is CrashAction.Initialize -> {
                dispatch(CrashAction.CheckDeferred)
            }
            is CrashAction.CheckDeferred -> scope.launch {
                val nextAction = cache.getDeferredUntil()?.let {
                    CrashAction.RestoreDeferred(now = currentTimeInMillis(), until = it)
                } ?: CrashAction.CheckForCrashes

                dispatch(nextAction)
            }
            is CrashAction.RestoreDeferred -> {
                if (getState() is CrashState.Ready) {
                    scope.launch {
                        cache.setDeferredUntil(null)
                        dispatch(CrashAction.CheckForCrashes)
                    }
                }
            }
            is CrashAction.CheckForCrashes -> scope.launch {
                dispatch(CrashAction.FinishCheckingForCrashes(crashReporter.hasUnsentCrashReports()))
            }
            CrashAction.CancelTapped -> dispatch(CrashAction.Defer(now = currentTimeInMillis()))
            is CrashAction.Defer -> scope.launch {
                val state = getState()
                if (state is CrashState.Deferred) {
                    cache.setDeferredUntil(state.until)
                }
            }
            CrashAction.ReportTapped -> scope.launch {
                crashReporter.unsentCrashReports().forEach {
                    crashReporter.submitReport(it)
                }
            }

            is CrashAction.FinishCheckingForCrashes -> {} // noop
        }
    }
}
