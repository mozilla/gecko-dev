/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class CrashMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `WHEN given initialize action THEN dispatch CheckDeferred`() {
        val middleware = CrashMiddleware(mock(), mock(), mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        middleware.invoke(middlewareContext, mock(), CrashAction.Initialize)

        val (_, dispatcher) = middlewareContext
        verify(dispatcher).invoke(CrashAction.CheckDeferred)
    }

    @Test
    fun `WHEN given CheckDeferred action THEN dispatch the cached deferred value`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), { 999_999 }, scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        `when`(cache.getDeferredUntil()).thenReturn(1_000_000)
        middleware.invoke(middlewareContext, mock(), CrashAction.CheckDeferred)

        val (_, dispatcher) = middlewareContext
        verify(dispatcher).invoke(CrashAction.RestoreDeferred(now = 999_999, until = 1_000_000))
    }

    @Test
    fun `GIVEN a Ready state WHEN we dispatch RestoreDeferred THEN clear cache and dispatch CheckForCrashes`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (getState, dispatcher) = middlewareContext
        `when`(getState.invoke()).thenReturn(CrashState.Ready)
        middleware.invoke(middlewareContext, mock(), CrashAction.RestoreDeferred(now = 1, until = 1))

        verify(cache).setDeferredUntil(null)
        verify(dispatcher).invoke(CrashAction.CheckForCrashes)
    }

    @Test
    fun `WHEN we dispatch CheckForCrashes THEN dispatch FinishCheckingForCrashes`() = runTestOnMain {
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(mock(), crashReporter, mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (_, dispatcher) = middlewareContext
        `when`(crashReporter.hasUnsentCrashReports()).thenReturn(true)
        middleware.invoke(middlewareContext, mock(), CrashAction.CheckForCrashes)

        verify(dispatcher).invoke(CrashAction.FinishCheckingForCrashes(true))
    }

    @Test
    fun `WHEN we dispatch CancelTapped THEN dispatch Defer`() {
        val middleware = CrashMiddleware(mock(), mock(), { 1_000_000 }, scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (_, dispatcher) = middlewareContext
        middleware.invoke(middlewareContext, mock(), CrashAction.CancelTapped)

        verify(dispatcher).invoke(CrashAction.Defer(1_000_000))
    }

    @Test
    fun `WHEN we dispatch Defer THEN cache the deferred value`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (getState, _) = middlewareContext
        `when`(getState.invoke()).thenReturn(CrashState.Deferred(until = 1_000_000))
        middleware.invoke(middlewareContext, mock(), CrashAction.Defer(0))

        verify(cache).setDeferredUntil(1_000_000)
    }

    @Test
    fun `WHEN we dispatch ReportTapped THEN submit unsent crashes`() = runTestOnMain {
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(mock(), crashReporter, mock(), scope)

        val crashes = listOf(
            Crash.UncaughtExceptionCrash(0L, mock(), mock(), uuid = "1"),
            Crash.UncaughtExceptionCrash(0L, mock(), mock(), uuid = "2"),
            Crash.UncaughtExceptionCrash(0L, mock(), mock(), uuid = "3"),
        )

        `when`(crashReporter.unsentCrashReports()).thenReturn(crashes)
        middleware.invoke(mock(), mock(), CrashAction.ReportTapped)

        crashes.forEach {
            verify(crashReporter).submitReport(it)
        }
    }
}
