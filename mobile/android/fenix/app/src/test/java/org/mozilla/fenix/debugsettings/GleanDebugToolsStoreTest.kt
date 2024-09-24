/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import androidx.test.ext.junit.runners.AndroidJUnit4
import junit.framework.TestCase
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsAction
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsMiddleware
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsService
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsState
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsStore
import org.mozilla.fenix.debugsettings.gleandebugtools.PING_PREVIEW_URL
import org.mozilla.fenix.utils.ClipboardHandler

@RunWith(AndroidJUnit4::class)
class GleanDebugToolsStoreTest {

    private lateinit var gleanDebugToolsService: GleanDebugToolsService
    private lateinit var clipboardHandler: ClipboardHandler

    @Before
    fun setup() {
        gleanDebugToolsService = FakeGleanDebugToolsService()
        clipboardHandler = ClipboardHandler(testContext)
    }

    @Test
    fun `WHEN there is no debug view tag THEN the debug view tag related buttons should be disabled`() {
        val initialState = GleanDebugToolsState(
            debugViewTag = "",
        )
        assertFalse(initialState.isDebugTagButtonEnabled)
    }

    @Test
    fun `WHEN the debug view tag length is larger than the limit THEN the debug view tag related buttons should be disabled`() {
        val initialState = GleanDebugToolsState(
            debugViewTag = "123456789123456789123",
        )
        assertFalse(initialState.isDebugTagButtonEnabled)
    }

    @Test
    fun `WHEN the the debug view tag length is smaller than the limit and it is not empty THEN the debug view tag related buttons should be enabled`() {
        val initialState = GleanDebugToolsState(
            debugViewTag = "test",
        )
        assertTrue(initialState.isDebugTagButtonEnabled)
    }

    @Test
    fun `GIVEN the log pings to console preference is off WHEN said preference is toggled THEN the preference should be enabled`() {
        gleanDebugToolsService = FakeGleanDebugToolsService(isSetLogPingsEnabled = false)
        val store = GleanDebugToolsStore(
            initialState = GleanDebugToolsState(
                logPingsToConsoleEnabled = false,
            ),
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                ),
            ),
        )
        assertFalse(store.state.logPingsToConsoleEnabled)
        assertFalse((gleanDebugToolsService as FakeGleanDebugToolsService).isSetLogPingsEnabled)
        store.dispatch(GleanDebugToolsAction.LogPingsToConsoleToggled)
        assertTrue(store.state.logPingsToConsoleEnabled)
        assertTrue((gleanDebugToolsService as FakeGleanDebugToolsService).isSetLogPingsEnabled)
    }

    @Test
    fun `GIVEN the log pings to console preference is on WHEN said preference is toggled THEN the preference should be enabled`() {
        gleanDebugToolsService = FakeGleanDebugToolsService(isSetLogPingsEnabled = true)
        val store = GleanDebugToolsStore(
            initialState = GleanDebugToolsState(
                logPingsToConsoleEnabled = true,
            ),
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                ),
            ),
        )
        assertTrue(store.state.logPingsToConsoleEnabled)
        assertTrue((gleanDebugToolsService as FakeGleanDebugToolsService).isSetLogPingsEnabled)
        store.dispatch(GleanDebugToolsAction.LogPingsToConsoleToggled)
        assertFalse(store.state.logPingsToConsoleEnabled)
        assertFalse((gleanDebugToolsService as FakeGleanDebugToolsService).isSetLogPingsEnabled)
    }

    @Test
    fun `WHEN the change debug view tag action is dispatched with an appropriate debug view tag THEN the debug view tag should be changed accordingly and hasDebugViewTagError should be false`() {
        val initialDebugViewTag = ""
        val newDebugViewTag = "Test"
        val store = GleanDebugToolsStore(
            initialState = GleanDebugToolsState(
                debugViewTag = initialDebugViewTag,
            ),
        )
        store.dispatch(GleanDebugToolsAction.DebugViewTagChanged(newTag = newDebugViewTag))
        assertEquals(newDebugViewTag, store.state.debugViewTag)
        assertFalse(store.state.hasDebugViewTagError)
    }

    @Test
    fun `WHEN the change debug view tag action is dispatched with a debug view tag that is too long THEN the debug view tag should be changed accordingly and hasDebugViewTagError should be true`() {
        val initialDebugViewTag = ""
        val newDebugViewTag = "123456789123456789123"
        val store = GleanDebugToolsStore(
            initialState = GleanDebugToolsState(
                debugViewTag = initialDebugViewTag,
            ),
        )
        store.dispatch(GleanDebugToolsAction.DebugViewTagChanged(newTag = newDebugViewTag))
        assertEquals(newDebugViewTag, store.state.debugViewTag)
        assertTrue(store.state.hasDebugViewTagError)
    }

    @Test
    fun `WHEN the send baseline ping action is dispatched THEN a baseline ping should be sent`() {
        val initialState = GleanDebugToolsState()
        val store = GleanDebugToolsStore(
            initialState = initialState,
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                ),
            ),
        )
        assertEquals(initialState, store.state)
        assertFalse((gleanDebugToolsService as FakeGleanDebugToolsService).baselinePingSent)
        store.dispatch(GleanDebugToolsAction.SendBaselinePing)
        assertEquals(initialState, store.state)
        assertTrue((gleanDebugToolsService as FakeGleanDebugToolsService).baselinePingSent)
    }

    @Test
    fun `WHEN the send baseline ping action is dispatched THEN a toast is shown`() {
        var toastShown = false
        val store = GleanDebugToolsStore(
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                    showToast = { resId ->
                        assertEquals(
                            R.string.glean_debug_tools_send_baseline_ping_toast_message,
                            resId,
                        )
                        toastShown = true
                    },
                ),
            ),
        )
        assertFalse(toastShown)
        store.dispatch(GleanDebugToolsAction.SendBaselinePing)
        assertTrue(toastShown)
    }

    @Test
    fun `WHEN the send metrics ping action is dispatched THEN a metrics ping should be sent`() {
        val initialState = GleanDebugToolsState()
        val store = GleanDebugToolsStore(
            initialState = initialState,
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                ),
            ),
        )
        assertEquals(initialState, store.state)
        assertFalse((gleanDebugToolsService as FakeGleanDebugToolsService).metricsPingSent)
        store.dispatch(GleanDebugToolsAction.SendMetricsPing)
        assertEquals(initialState, store.state)
        assertTrue((gleanDebugToolsService as FakeGleanDebugToolsService).metricsPingSent)
    }

    @Test
    fun `WHEN the send metrics ping action is dispatched THEN a toast is shown`() {
        var toastShown = false
        val store = GleanDebugToolsStore(
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                    showToast = { resId ->
                        assertEquals(
                            R.string.glean_debug_tools_send_metrics_ping_toast_message,
                            resId,
                        )
                        toastShown = true
                    },
                ),
            ),
        )
        assertFalse(toastShown)
        store.dispatch(GleanDebugToolsAction.SendMetricsPing)
        assertTrue(toastShown)
    }

    @Test
    fun `WHEN the send pending event ping action is dispatched THEN a pending event ping should be sent`() {
        val initialState = GleanDebugToolsState()
        val store = GleanDebugToolsStore(
            initialState = initialState,
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                ),
            ),
        )
        assertEquals(initialState, store.state)
        assertFalse((gleanDebugToolsService as FakeGleanDebugToolsService).pendingEventPingSent)
        store.dispatch(GleanDebugToolsAction.SendPendingEventPing)
        assertEquals(initialState, store.state)
        assertTrue((gleanDebugToolsService as FakeGleanDebugToolsService).pendingEventPingSent)
    }

    @Test
    fun `WHEN the send pending event ping action is dispatched THEN a toast is shown`() {
        var toastShown = false
        val store = GleanDebugToolsStore(
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                    showToast = { resId ->
                        assertEquals(
                            R.string.glean_debug_tools_send_baseline_ping_toast_message,
                            resId,
                        )
                        toastShown = true
                    },
                ),
            ),
        )
        assertFalse(toastShown)
        store.dispatch(GleanDebugToolsAction.SendBaselinePing)
        assertTrue(toastShown)
    }

    @Test
    fun `GIVEN the debug view tag should be used WHEN the open debug view action is dispatched THEN the appropriate lambda is called with the appropriate debug view URL`() {
        var openDebugViewInvoked = false
        val debugViewTag = "test"
        val expectedDebugViewLink = "${PING_PREVIEW_URL}pings/$debugViewTag"
        val initialState = GleanDebugToolsState(
            debugViewTag = debugViewTag,
        )
        val store = GleanDebugToolsStore(
            initialState = initialState,
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                    openDebugView = { debugViewLink ->
                        assertEquals(expectedDebugViewLink, debugViewLink)
                        openDebugViewInvoked = true
                    },
                ),
            ),
        )
        assertEquals(initialState, store.state)
        assertFalse(openDebugViewInvoked)
        store.dispatch(GleanDebugToolsAction.OpenDebugView(useDebugViewTag = true))
        assertEquals(initialState, store.state)
        assertTrue(openDebugViewInvoked)
    }

    @Test
    fun `GIVEN the debug view tag should not be used WHEN the open debug view action is dispatched THEN the appropriate lambda is called with the default debug view URL`() {
        val initialState = GleanDebugToolsState()
        var openDebugViewInvoked = false
        val store = GleanDebugToolsStore(
            initialState = initialState,
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                    openDebugView = { debugViewLink ->
                        TestCase.assertEquals(PING_PREVIEW_URL, debugViewLink)
                        openDebugViewInvoked = true
                    },
                ),
            ),
        )
        assertEquals(initialState, store.state)
        assertFalse(openDebugViewInvoked)
        store.dispatch(GleanDebugToolsAction.OpenDebugView(useDebugViewTag = false))
        assertEquals(initialState, store.state)
        assertTrue(openDebugViewInvoked)
    }

    @Test
    fun `GIVEN the debug view tag should be used WHEN the copy debug view action is dispatched THEN the state should remain the same`() {
        val debugViewTag = "test"
        val expectedDebugViewLink = "${PING_PREVIEW_URL}pings/$debugViewTag"
        clipboardHandler.text = null
        val initialState = GleanDebugToolsState(
            debugViewTag = debugViewTag,
        )
        val store = GleanDebugToolsStore(
            initialState = initialState,
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                ),
            ),
        )
        assertEquals(initialState, store.state)
        assertNotEquals(expectedDebugViewLink, clipboardHandler.text)
        store.dispatch(GleanDebugToolsAction.CopyDebugViewLink(useDebugViewTag = true))
        assertEquals(initialState, store.state)
        assertEquals(expectedDebugViewLink, clipboardHandler.text)
    }

    @Test
    fun `GIVEN the debug view tag should not be used WHEN the copy debug view action is dispatched THEN the state should remain the same`() {
        val initialState = GleanDebugToolsState()
        clipboardHandler.text = null
        val store = GleanDebugToolsStore(
            initialState = initialState,
            middlewares = listOf(
                createMiddleware(
                    gleanDebugToolsService = gleanDebugToolsService,
                ),
            ),
        )
        assertEquals(initialState, store.state)
        assertNotEquals(PING_PREVIEW_URL, clipboardHandler.text)
        store.dispatch(GleanDebugToolsAction.CopyDebugViewLink(useDebugViewTag = false))
        assertEquals(initialState, store.state)
        assertEquals(PING_PREVIEW_URL, clipboardHandler.text)
    }

    private fun createMiddleware(
        gleanDebugToolsService: GleanDebugToolsService,
        openDebugView: (String) -> Unit = { _ -> },
        showToast: (Int) -> Unit = { _ -> },
    ) = GleanDebugToolsMiddleware(
        gleanDebugToolsService = gleanDebugToolsService,
        clipboardHandler = clipboardHandler,
        openDebugView = openDebugView,
        showToast = showToast,
    )
}
