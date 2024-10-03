/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.WebExtensionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.components.menu.store.MenuAction
import org.mozilla.fenix.components.menu.store.MenuState
import org.mozilla.fenix.components.menu.store.MenuStore
import org.mozilla.fenix.components.menu.store.WebExtensionMenuItem

/**
 * Helper for observing Extension state from both [BrowserState.extensions]
 * and [TabSessionState.extensionState].
 *
 * @param browserStore Used to listen for changes to [WebExtensionState].
 * @param menuStore The [Store] for holding the [MenuState] and applying [MenuAction]s.
 * @param iconSize for [WebExtensionMenuItem].
 * @param onDismiss Callback invoked to dismiss the menu dialog.
 */
class WebExtensionsMenuBinding(
    browserStore: BrowserStore,
    private val menuStore: MenuStore,
    private val iconSize: Int,
    private val onDismiss: () -> Unit,
) : AbstractBinding<BrowserState>(browserStore) {

    override suspend fun onState(flow: Flow<BrowserState>) {
        // Browser level flows
        val browserFlow = flow.mapNotNull { state -> state }
            .distinctUntilChangedBy {
                it.extensions
            }

        // Session level flows
        val sessionFlow = flow.mapNotNull { state -> state.selectedTab }
            .distinctUntilChangedBy {
                it.extensionState
            }

        // Applying the flows together
        sessionFlow
            .combine(browserFlow) { sessionState, browserState ->
                WebExtensionsFlowState(
                    sessionState,
                    browserState,
                )
            }
            .collect { webExtensionsFlowState ->
                val webExtensionMenuItems = ArrayList<WebExtensionMenuItem>()
                webExtensionsFlowState.browserState.extensions.values.filter { it.enabled }
                    .sortedBy { it.name }
                    .forEach { extension ->
                        if (!extension.allowedInPrivateBrowsing &&
                            webExtensionsFlowState.sessionState.content.private
                        ) {
                            return@forEach
                        }

                        extension.browserAction?.let { extensionBrowserAction ->
                            var browserAction = extensionBrowserAction

                            val tabPageAction =
                                webExtensionsFlowState.sessionState.extensionState[extension.id]?.browserAction

                            tabPageAction?.let {
                                browserAction = browserAction.copyWithOverride(it)
                            }

                            if (browserAction.title == null || browserAction.enabled == false) {
                                return@collect
                            }

                            val loadIcon = browserAction.loadIcon?.invoke(iconSize)

                            webExtensionMenuItems.add(
                                WebExtensionMenuItem(
                                    label = browserAction.title!!,
                                    enabled = browserAction.enabled,
                                    icon = loadIcon,
                                    badgeText = browserAction.badgeText,
                                    badgeTextColor = browserAction.badgeTextColor,
                                    badgeBackgroundColor = browserAction.badgeBackgroundColor,
                                    onClick = {
                                        onDismiss()
                                        browserAction.onClick()
                                    },
                                ),
                            )
                        }
                    }
                menuStore.dispatch(MenuAction.UpdateWebExtensionMenuItems(webExtensionMenuItems))
            }
    }
}

/**
 * Convenience method to create a named pair for the web extensions flow.
 *
 * @property sessionState The session or tab state.
 * @property browserState The browser or global state.
 */
private data class WebExtensionsFlowState(
    val sessionState: TabSessionState,
    val browserState: BrowserState,
)
