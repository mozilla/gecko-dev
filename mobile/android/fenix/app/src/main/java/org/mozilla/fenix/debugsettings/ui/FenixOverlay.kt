/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.ui

import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.compose.rememberNavController
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.debugsettings.gleandebugtools.DefaultGleanDebugToolsService
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsMiddleware
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsState
import org.mozilla.fenix.debugsettings.gleandebugtools.GleanDebugToolsStore
import org.mozilla.fenix.debugsettings.logins.FakeLoginsStorage
import org.mozilla.fenix.debugsettings.logins.LoginsTools
import org.mozilla.fenix.debugsettings.navigation.DebugDrawerRoute
import org.mozilla.fenix.debugsettings.store.DebugDrawerAction
import org.mozilla.fenix.debugsettings.store.DebugDrawerNavigationMiddleware
import org.mozilla.fenix.debugsettings.store.DebugDrawerStore
import org.mozilla.fenix.debugsettings.store.DrawerStatus
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * Overlay for presenting Fenix-wide debugging content.
 *
 * @param browserStore [BrowserStore] used to access [BrowserState].
 * @param loginsStorage [LoginsStorage] used to access logins for [LoginsTools].
 * @param inactiveTabsEnabled Whether the inactive tabs feature is enabled.
 */
@Composable
fun FenixOverlay(
    browserStore: BrowserStore,
    loginsStorage: LoginsStorage,
    inactiveTabsEnabled: Boolean,
) {
    val context = LocalContext.current

    FenixOverlay(
        browserStore = browserStore,
        loginsStorage = loginsStorage,
        gleanDebugToolsStore = GleanDebugToolsStore(
            middlewares = listOf(
                GleanDebugToolsMiddleware(
                    gleanDebugToolsService = DefaultGleanDebugToolsService(),
                    clipboardHandler = context.components.clipboardHandler,
                    openDebugView = { debugViewLink ->
                        val intent = Intent(Intent.ACTION_VIEW)
                        intent.data = Uri.parse(debugViewLink)
                        context.startActivity(intent)
                    },
                    showToast = { resId ->
                        val toast = Toast.makeText(
                            context,
                            context.getString(resId),
                            Toast.LENGTH_LONG,
                        )
                        toast.show()
                    },
                ),
            ),
        ),
        inactiveTabsEnabled = inactiveTabsEnabled,
    )
}

/**
 * Overlay for presenting Fenix-wide debugging content.
 *
 * @param browserStore [BrowserStore] used to access [BrowserState].
 * @param gleanDebugToolsStore [GleanDebugToolsStore] used to access [GleanDebugToolsState].
 * @param loginsStorage [LoginsStorage] used to access logins for [LoginsTools].
 * @param inactiveTabsEnabled Whether the inactive tabs feature is enabled.
 */
@Composable
private fun FenixOverlay(
    browserStore: BrowserStore,
    gleanDebugToolsStore: GleanDebugToolsStore,
    loginsStorage: LoginsStorage,
    inactiveTabsEnabled: Boolean,
) {
    val navController = rememberNavController()
    val coroutineScope = rememberCoroutineScope()

    val debugDrawerStore = remember {
        DebugDrawerStore(
            middlewares = listOf(
                DebugDrawerNavigationMiddleware(
                    navController = navController,
                    scope = coroutineScope,
                ),
            ),
        )
    }

    val debugDrawerDestinations = remember {
        DebugDrawerRoute.generateDebugDrawerDestinations(
            debugDrawerStore = debugDrawerStore,
            gleanDebugToolsStore = gleanDebugToolsStore,
            browserStore = browserStore,
            inactiveTabsEnabled = inactiveTabsEnabled,
            loginsStorage = loginsStorage,
        )
    }
    val drawerStatus by debugDrawerStore.observeAsState(initialValue = DrawerStatus.Closed) { state ->
        state.drawerStatus
    }

    FirefoxTheme(theme = Theme.getTheme(allowPrivateTheme = false)) {
        DebugOverlay(
            navController = navController,
            drawerStatus = drawerStatus,
            debugDrawerDestinations = debugDrawerDestinations,
            onDrawerOpen = {
                debugDrawerStore.dispatch(DebugDrawerAction.DrawerOpened)
            },
            onDrawerClose = {
                debugDrawerStore.dispatch(DebugDrawerAction.DrawerClosed)
            },
            onDrawerBackButtonClick = {
                debugDrawerStore.dispatch(DebugDrawerAction.OnBackPressed)
            },
        )
    }
}

@LightDarkPreview
@Composable
private fun FenixOverlayPreview() {
    val selectedTab = createTab("https://mozilla.org")
    FenixOverlay(
        browserStore = BrowserStore(
            BrowserState(selectedTabId = selectedTab.id, tabs = listOf(selectedTab)),
        ),
        gleanDebugToolsStore = GleanDebugToolsStore(),
        inactiveTabsEnabled = true,
        loginsStorage = FakeLoginsStorage(),
    )
}
