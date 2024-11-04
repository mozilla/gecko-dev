/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.doAnswer
import org.mockito.Mockito.doReturn

private const val NORMAL_TAB_ID = "tab"
private const val CUSTOM_TAB_ID = "customTab"

class PdfStateMiddlewareTest {
    private val captureActionsMiddleware = CaptureActionsMiddleware<BrowserState, BrowserAction>()

    @Test(expected = AssertionError::class)
    fun `GIVEN navigating to a pdf in the current normal tab WHEN the page is not fully loaded THEN don't update the pdf status`() = runTest {
        val store = BrowserStore(
            initialState = buildState(newPdfStatusForNormalTab = true),
            middleware = listOf(PdfStateMiddleware(this), captureActionsMiddleware),
        )

        store.dispatch(ContentAction.UpdateProgressAction(NORMAL_TAB_ID, 10)).join()
        store.dispatch(ContentAction.UpdateProgressAction(NORMAL_TAB_ID, 50)).join()
        store.dispatch(ContentAction.UpdateProgressAction(NORMAL_TAB_ID, 99)).join()
        store.waitUntilIdle() // wait for the actions dispatched from PdfStateMiddleware to be handled in CaptureActionsMiddleware

        // If the action is not dispatched then the below call would throw an AssertionError.
        assertNull(captureActionsMiddleware.findLastAction(ContentAction.EnteredPdfViewer::class))
    }

    @Test
    fun `GIVEN navigating to a pdf in the current normal tab WHEN the page is fully loaded THEN inform about viewing a pdf`() = runTest {
        val store = BrowserStore(
            initialState = buildState(newPdfStatusForNormalTab = true),
            middleware = listOf(PdfStateMiddleware(this), captureActionsMiddleware),
        )

        store.dispatch(ContentAction.UpdateProgressAction(NORMAL_TAB_ID, 100)).join()
        store.waitUntilIdle() // wait for the actions dispatched from PdfStateMiddleware to be handled in CaptureActionsMiddleware

        assertTrue(captureActionsMiddleware.findFirstAction(ContentAction.EnteredPdfViewer::class).tabId == NORMAL_TAB_ID)
    }

    @Test
    fun `GIVEN navigating to a pdf in the custom normal tab WHEN the tab state is processed THEN inform about viewing a pdf`() = runTest {
        val store = BrowserStore(
            initialState = buildState(newPdfStatusForCustomTab = true),
            middleware = listOf(PdfStateMiddleware(this), captureActionsMiddleware),
        )

        store.dispatch(ContentAction.UpdateProgressAction(CUSTOM_TAB_ID, 100)).join()
        store.waitUntilIdle() // wait for the actions dispatched from PdfStateMiddleware to be handled in CaptureActionsMiddleware

        assertTrue(captureActionsMiddleware.findFirstAction(ContentAction.EnteredPdfViewer::class).tabId == CUSTOM_TAB_ID)
    }

    @Test
    fun `GIVEN navigating off from a pdf in the current normal tab WHEN the tab state is processed THEN inform about exiting from a pdf`() = runTest {
        val store = BrowserStore(
            initialState = buildState(
                previousPdfStatusForNormalTab = true,
                newPdfStatusForNormalTab = false,
            ),
            middleware = listOf(PdfStateMiddleware(this), captureActionsMiddleware),
        )

        store.dispatch(ContentAction.UpdateProgressAction(NORMAL_TAB_ID, 100)).join()
        store.waitUntilIdle() // wait for the actions dispatched from PdfStateMiddleware to be handled in CaptureActionsMiddleware

        assertTrue(captureActionsMiddleware.findFirstAction(ContentAction.ExitedPdfViewer::class).tabId == NORMAL_TAB_ID)
    }

    @Test
    fun `GIVEN navigating off from a pdf in the custom normal tab WHEN the tab state is processed THEN inform about exiting from a pdf`() = runTest {
        val store = BrowserStore(
            initialState = buildState(
                previousPdfStatusForCustomTab = true,
                newPdfStatusForCustomTab = false,
            ),
            middleware = listOf(PdfStateMiddleware(this), captureActionsMiddleware),
        )

        store.dispatch(ContentAction.UpdateProgressAction(CUSTOM_TAB_ID, 100)).join()
        store.waitUntilIdle() // wait for the actions dispatched from PdfStateMiddleware to be handled in CaptureActionsMiddleware

        assertTrue(captureActionsMiddleware.findFirstAction(ContentAction.ExitedPdfViewer::class).tabId == CUSTOM_TAB_ID)
    }

    @Test
    fun `GIVEN already viewing a pdf and the page is updated WHEN cannot infer whether still viewing a pdf THEN inform about exiting from a pdf`() = runTest {
        val state = BrowserState(
            tabs = listOf(
                TabSessionState(
                    id = NORMAL_TAB_ID,
                    engineState = buildEngineState(
                        isPdf = true,
                        throwException = true,
                    ),
                    content = ContentState(
                        url = "https://mozilla.org",
                        isPdf = true,
                    ),
                ),
            ),
        )
        val store = BrowserStore(
            initialState = state,
            middleware = listOf(PdfStateMiddleware(this), captureActionsMiddleware),
        )

        store.dispatch(ContentAction.UpdateProgressAction(NORMAL_TAB_ID, 100)).join()
        store.waitUntilIdle() // wait for the actions dispatched from PdfStateMiddleware to be handled in CaptureActionsMiddleware

        assertTrue(captureActionsMiddleware.findFirstAction(ContentAction.ExitedPdfViewer::class).tabId == NORMAL_TAB_ID)
    }

    private fun buildState(
        tabId: String = NORMAL_TAB_ID,
        customTabId: String = CUSTOM_TAB_ID,
        previousPdfStatusForNormalTab: Boolean = false,
        newPdfStatusForNormalTab: Boolean = false,
        previousPdfStatusForCustomTab: Boolean = false,
        newPdfStatusForCustomTab: Boolean = false,
    ): BrowserState {
        val tab = TabSessionState(
            id = tabId,
            engineState = buildEngineState(newPdfStatusForNormalTab),
            content = ContentState(
                url = "https://mozilla.org",
                isPdf = previousPdfStatusForNormalTab,
            ),
        )

        val customTab = CustomTabSessionState(
            id = customTabId,
            engineState = buildEngineState(newPdfStatusForCustomTab),
            content = ContentState(
                url = "https://mozilla.org",
                isPdf = previousPdfStatusForCustomTab,
            ),
            config = mock(),
        )

        return BrowserState(
            tabs = listOf(mock(), tab),
            customTabs = listOf(mock(), customTab),
        )
    }

    private fun buildEngineState(
        isPdf: Boolean,
        throwException: Boolean = false,
    ): EngineState {
        val session: EngineSession = mock()
        val resultCaptor = argumentCaptor<(Boolean) -> Unit>()
        val exceptionCaptor = argumentCaptor<(Throwable) -> Unit>()
        doAnswer {
            when (throwException) {
                true -> exceptionCaptor.value.invoke(Exception())
                false -> resultCaptor.value.invoke(isPdf)
            }
        }.`when`(session).checkForPdfViewer(resultCaptor.capture(), exceptionCaptor.capture())
        val engineState: EngineState = mock()
        doReturn(session).`when`(engineState).engineSession

        return engineState
    }
}
