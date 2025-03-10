/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.UndoAction
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
internal class SessionsPendingDeletion {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var sessions: MutableMap<String, EngineSession> = mutableMapOf()
    private var mutex = Mutex()

    suspend fun add(id: String, session: EngineSession) = mutex.withLock {
        sessions[id] = session
    }

    suspend fun remove(id: String) = mutex.withLock {
        sessions.remove(id)
    }

    suspend fun removeAll(callback: (EngineSession) -> Unit) = mutex.withLock {
        val keys = sessions.keys.toList()
        for (key in keys) {
            sessions[key]?.also(callback)
            sessions.remove(key)
        }
    }
}

/**
 * [Middleware] responsible for closing and unlinking [EngineSession] instances whenever tabs get
 * removed.
 */
internal class TabsRemovedMiddleware(
    private val scope: CoroutineScope,
) : Middleware<BrowserState, BrowserAction> {
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    val sessionsPendingDeletion = SessionsPendingDeletion()

    @Suppress("ComplexMethod")
    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        when (action) {
            is TabListAction.RemoveAllNormalTabsAction -> onTabsRemoved(context, context.state.normalTabs)
            is TabListAction.RemoveAllPrivateTabsAction -> onTabsRemoved(context, context.state.privateTabs)
            is TabListAction.RemoveAllTabsAction -> onTabsRemoved(context, context.state.tabs)
            is TabListAction.RemoveTabAction -> context.state.findTab(action.tabId)?.let {
                onTabsRemoved(context, listOf(it))
            }
            is TabListAction.RemoveTabsAction -> action.tabIds.mapNotNull { context.state.findTab(it) }.let {
                onTabsRemoved(context, it)
            }
            is CustomTabListAction.RemoveAllCustomTabsAction -> onTabsRemoved(context, context.state.customTabs)
            is CustomTabListAction.RemoveCustomTabAction -> context.state.findCustomTab(action.tabId)?.let {
                onTabsRemoved(context, listOf(it))
            }
            is UndoAction.ClearRecoverableTabs, UndoAction.RestoreRecoverableTabs -> clearSessionsPendingDeletion()
            else -> {
                // no-op
            }
        }

        next(action)
    }

    private fun onTabsRemoved(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        tabs: List<SessionState>,
    ) {
        tabs.forEach { tab ->
            if (tab.engineState.engineSession != null) {
                // We don't have a way to recover custom tabs, so let's not observe and just close
                // the session.
                if (tab is CustomTabSessionState) {
                    scope.launch { tab.engineState.engineSession?.close() }
                } else {
                    // With the addition of [SHIP](https://bugzilla.mozilla.org/show_bug.cgi?id=1736121)
                    // Our tab state may be out of sync with GeckoView. Let's wait for one more `onStateUpdated`
                    // event before we close the engine session.
                    waitForFinalStateUpdate(context, tab)
                }

                context.dispatch(
                    EngineAction.UnlinkEngineSessionAction(
                        tab.id,
                    ),
                )
            }
        }
    }

    private fun waitForFinalStateUpdate(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        sessionState: SessionState,
    ) {
        sessionState.engineState.engineSession?.also {
            it.register(object : EngineSession.Observer {
                override fun onStateUpdated(state: EngineSessionState) {
                    context.store.dispatch(UndoAction.UpdateEngineStateForRecoverableTab(sessionState.id, state))
                    scope.launch {
                        sessionsPendingDeletion.remove(sessionState.id)
                    }
                    it.close()
                }
            })
            scope.launch {
                sessionsPendingDeletion.add(sessionState.id, it)
            }
        }
    }

    private fun clearSessionsPendingDeletion() = scope.launch {
        sessionsPendingDeletion.removeAll {
            it.close()
        }
    }
}
