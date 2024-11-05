/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class CrashMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Test
    fun `GIVEN crash reporting is initializing WHEN user has chosen to always send crashes THEN skip checking whether we've recently deferred reporting`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        `when`(cache.getAlwaysSend()).thenReturn(true)
        middleware.invoke(middlewareContext, mock(), CrashAction.Initialize)

        val (_, dispatcher) = middlewareContext
        verify(dispatcher).invoke(CrashAction.CheckForCrashes)
    }

    @Test
    fun `GIVEN crash reporting is initialized WHEN user has not chosen to always send crashes THEN check if the user has deferred sending crash reports`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        `when`(cache.getAlwaysSend()).thenReturn(false)
        middleware.invoke(middlewareContext, mock(), CrashAction.Initialize)

        val (_, dispatcher) = middlewareContext
        verify(dispatcher).invoke(CrashAction.CheckDeferred)
    }

    @Test
    fun `GIVEN a crash reporter that is checking if it should defer WHEN we get a result from the cache THEN try restoring the result`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), { 999_999 }, scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        `when`(cache.getDeferredUntil()).thenReturn(1_000_000)
        middleware.invoke(middlewareContext, mock(), CrashAction.CheckDeferred)

        val (_, dispatcher) = middlewareContext
        verify(dispatcher).invoke(CrashAction.RestoreDeferred(now = 999_999, until = 1_000_000))
    }

    @Test
    fun `GIVEN a crash reporter that is checking if it should defer WHEN we get a null from the cache THEN check for crashes`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), { 999_999 }, scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        `when`(cache.getDeferredUntil()).thenReturn(null)
        middleware.invoke(middlewareContext, mock(), CrashAction.CheckDeferred)

        val (_, dispatcher) = middlewareContext
        verify(dispatcher).invoke(CrashAction.CheckForCrashes)
    }

    @Test
    fun `GIVEN a crash reporter in a deferred state WHEN handling RestoreDeferred THEN do nothing`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (getState, dispatcher) = middlewareContext
        `when`(getState.invoke()).thenReturn(CrashState.Deferred(until = 1))
        middleware.invoke(middlewareContext, mock(), CrashAction.RestoreDeferred(now = 1, until = 1))

        verify(cache, never()).setDeferredUntil(null)
        verify(dispatcher, never()).invoke(CrashAction.CheckForCrashes)
    }

    @Test
    fun `GIVEN a crash reporter is ready WHEN handling RestoreDefer THEN clear cache and check for crashes`() = runTestOnMain {
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
    fun `GIVEN a crash reporter that has unsent crashes WHEN handling the check for crashes THEN dispatch FinishCheckingForCrashes with hasUnsentCrashes = true`() = runTestOnMain {
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(mock(), crashReporter, mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (_, dispatcher) = middlewareContext
        `when`(crashReporter.hasUnsentCrashReportsSince()).thenReturn(true)
        middleware.invoke(middlewareContext, mock(), CrashAction.CheckForCrashes)

        verify(dispatcher).invoke(CrashAction.FinishCheckingForCrashes(true))
    }

    @Test
    fun `GIVEN a crash reporter that has no unsent crashes WHEN handling the check for crashes THEN dispatch FinishCheckingForCrashes with hasUnsentCrashes = false`() = runTestOnMain {
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(mock(), crashReporter, mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (_, dispatcher) = middlewareContext
        `when`(crashReporter.hasUnsentCrashReportsSince()).thenReturn(false)
        middleware.invoke(middlewareContext, mock(), CrashAction.CheckForCrashes)

        verify(dispatcher).invoke(CrashAction.FinishCheckingForCrashes(false))
    }

    @Test
    fun `GIVEN a crash reporter that has no unsent crashes WHEN handling FinishCheckingForCrashes THEN do nothing`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(cache, crashReporter, mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (_, dispatcher) = middlewareContext
        middleware.invoke(middlewareContext, mock(), CrashAction.FinishCheckingForCrashes(hasUnsentCrashes = false))

        verify(cache, never()).getAlwaysSend()
        verify(dispatcher, never()).invoke(CrashAction.ShowPrompt)
    }

    @Test
    fun `GIVEN FinishCheckingForCrashes action WHEN hasUnsentCrashes is true AND getAlwaysSend returns true THEN send unsent crash reports`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(cache, crashReporter, mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        `when`(cache.getAlwaysSend()).thenReturn(true)
        `when`(crashReporter.unsentCrashReportsSince()).thenReturn(listOf(mock<Crash.UncaughtExceptionCrash>(), mock<Crash.UncaughtExceptionCrash>()))
        val (_, dispatcher) = middlewareContext
        middleware.invoke(middlewareContext, mock(), CrashAction.FinishCheckingForCrashes(hasUnsentCrashes = true))

        verify(cache).getAlwaysSend()
        verify(crashReporter, times(2)).submitReport(any(), any())
        verify(dispatcher, never()).invoke(CrashAction.ShowPrompt)
    }

    @Test
    fun `GIVEN FinishCheckingForCrashes action WHEN hasUnsentCrashes is true AND getAlwaysSend returns false THEN dispatch ShowPrompt`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(cache, crashReporter, mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        `when`(cache.getAlwaysSend()).thenReturn(false)
        val (_, dispatcher) = middlewareContext
        middleware.invoke(middlewareContext, mock(), CrashAction.FinishCheckingForCrashes(hasUnsentCrashes = true))

        verify(cache).getAlwaysSend()
        verify(crashReporter, never()).submitReport(any(), any())
        verify(dispatcher).invoke(CrashAction.ShowPrompt)
    }

    @Test
    fun `GIVEN CancelTapped action THEN dispatch Defer`() {
        val middleware = CrashMiddleware(mock(), mock(), { 1_000_000 }, scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (_, dispatcher) = middlewareContext
        middleware.invoke(middlewareContext, mock(), CrashAction.CancelTapped)

        verify(dispatcher).invoke(CrashAction.Defer(1_000_000))
    }

    @Test
    fun `GIVEN a Defer action THEN cache the deferred value`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val middleware = CrashMiddleware(cache, mock(), mock(), scope)
        val middlewareContext: Pair<() -> CrashState, (CrashAction) -> Unit> = Pair(mock(), mock())

        val (getState, _) = middlewareContext
        `when`(getState.invoke()).thenReturn(CrashState.Deferred(until = 1_000_000))
        middleware.invoke(middlewareContext, mock(), CrashAction.Defer(0))

        verify(cache).setDeferredUntil(1_000_000)
    }

    @Test
    fun `GIVEN ReportTapped WHEN automaticallySendChecked is true THEN submit unsent crashes and cache value`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(cache, crashReporter, mock(), scope)

        `when`(crashReporter.unsentCrashReportsSince()).thenReturn(listOf(mock<Crash.UncaughtExceptionCrash>(), mock<Crash.UncaughtExceptionCrash>()))
        middleware.invoke(mock(), mock(), CrashAction.ReportTapped(automaticallySendChecked = true))

        verify(cache).setAlwaysSend(true)
        verify(crashReporter, times(2)).submitReport(any(), any())
    }

    @Test
    fun `GIVEN ReportTapped WHEN automaticallySendChecked is false THEN submit unsent crashes`() = runTestOnMain {
        val cache: CrashReportCache = mock()
        val crashReporter: CrashReporter = mock()
        val middleware = CrashMiddleware(cache, crashReporter, mock(), scope)

        `when`(crashReporter.unsentCrashReportsSince()).thenReturn(listOf(mock<Crash.UncaughtExceptionCrash>(), mock<Crash.UncaughtExceptionCrash>()))
        middleware.invoke(mock(), mock(), CrashAction.ReportTapped(automaticallySendChecked = false))

        verify(cache, never()).setAlwaysSend(true)
        verify(crashReporter, times(2)).submitReport(any(), any())
    }
}
