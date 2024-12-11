/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.ui

import android.content.Intent
import android.net.Uri
import android.os.StrictMode
import android.widget.Toast
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.navigation.compose.rememberNavController
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.storage.LoginsStorage
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.debugsettings.addresses.AddressesDebugLocalesRepository
import org.mozilla.fenix.debugsettings.addresses.AddressesTools
import org.mozilla.fenix.debugsettings.addresses.FakeAddressesDebugLocalesRepository
import org.mozilla.fenix.debugsettings.addresses.SharedPrefsAddressesDebugLocalesRepository
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsPreferencesMiddleware
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsState
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsStore
import org.mozilla.fenix.debugsettings.cfrs.DefaultCfrPreferencesRepository
import org.mozilla.fenix.debugsettings.gleandebugtools.DefaultGleanDebugToolsStorage
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
    val lifecycleOwner = LocalLifecycleOwner.current

    FenixOverlay(
        browserStore = browserStore,
        cfrToolsStore = CfrToolsStore(
            middlewares = listOf(
                CfrToolsPreferencesMiddleware(
                    cfrPreferencesRepository = DefaultCfrPreferencesRepository(
                        context = LocalContext.current,
                        lifecycleOwner = lifecycleOwner,
                        coroutineScope = lifecycleOwner.lifecycleScope,
                    ),
                    coroutineScope = lifecycleOwner.lifecycleScope,
                ),
            ),
        ),
        gleanDebugToolsStore = GleanDebugToolsStore(
            middlewares = listOf(
                GleanDebugToolsMiddleware(
                    gleanDebugToolsStorage = DefaultGleanDebugToolsStorage(),
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
        loginsStorage = loginsStorage,
        addressesDebugLocalesRepository = context.components.strictMode.resetAfter(StrictMode.allowThreadDiskReads()) {
            SharedPrefsAddressesDebugLocalesRepository(
                context,
            )
        },
        inactiveTabsEnabled = inactiveTabsEnabled,
    )
}

/**
 * Overlay for presenting Fenix-wide debugging content.
 *
 * @param browserStore [BrowserStore] used to access [BrowserState].
 * @param cfrToolsStore [CfrToolsStore] used to access [CfrToolsState].
 * @param gleanDebugToolsStore [GleanDebugToolsStore] used to access [GleanDebugToolsState].
 * @param loginsStorage [LoginsStorage] used to access logins for [LoginsTools].
 * @param addressesDebugLocalesRepository used to control storage for [AddressesTools].
 * @param inactiveTabsEnabled Whether the inactive tabs feature is enabled.
 */
@Composable
private fun FenixOverlay(
    browserStore: BrowserStore,
    cfrToolsStore: CfrToolsStore,
    gleanDebugToolsStore: GleanDebugToolsStore,
    loginsStorage: LoginsStorage,
    addressesDebugLocalesRepository: AddressesDebugLocalesRepository,
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
            browserStore = browserStore,
            cfrToolsStore = cfrToolsStore,
            gleanDebugToolsStore = gleanDebugToolsStore,
            inactiveTabsEnabled = inactiveTabsEnabled,
            loginsStorage = loginsStorage,
            addressesDebugLocalesRepository = addressesDebugLocalesRepository,
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
        cfrToolsStore = CfrToolsStore(),
        gleanDebugToolsStore = GleanDebugToolsStore(),
        inactiveTabsEnabled = true,
        loginsStorage = FakeLoginsStorage(),
        addressesDebugLocalesRepository = FakeAddressesDebugLocalesRepository(),
    )
}
