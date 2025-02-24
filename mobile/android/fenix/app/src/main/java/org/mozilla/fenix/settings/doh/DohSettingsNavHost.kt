/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import mozilla.components.lib.state.ext.observeAsState
import org.mozilla.fenix.settings.doh.root.DohSettingsScreen

/**
 * Nav host of the screen of DoH Settings
 */
@Composable
@Suppress("LongMethod")
internal fun DohSettingsNavHost(
    buildStore: (NavHostController) -> DohSettingsStore,
    startDestination: String = DohSettingsDestinations.ROOT,
) {
    val navController = rememberNavController()
    val store = buildStore(navController)

    NavHost(
        navController = navController,
        startDestination = startDestination,
    ) {
        composable(route = DohSettingsDestinations.ROOT) {
            val state by store.observeAsState(store.state) { it }
            DohSettingsScreen(
                state = state,
                onNavigateUp = {
                    store.dispatch(
                        BackClicked,
                    )
                },
                onLearnMoreClicked = { url ->
                    store.dispatch(
                        LearnMoreClicked(
                            url,
                        ),
                    )
                },
                onDohOptionSelected = { protectionLevel, provider ->
                    store.dispatch(
                        DohSettingsRootAction.DohOptionSelected(
                            protectionLevel = protectionLevel,
                            provider = provider,
                        ),
                    )
                },
                onExceptionsClicked = {
                    store.dispatch(
                        DohSettingsRootAction.ExceptionsClicked,
                    )
                },
                onCustomClicked = {
                    store.dispatch(
                        DohSettingsRootAction.CustomClicked,
                    )
                },
                onCustomCancelClicked = {
                    store.dispatch(
                        DohSettingsRootAction.DohCustomProviderDialogAction.CancelClicked,
                    )
                },
                onCustomAddClicked = { customProvider, url ->
                    store.dispatch(
                        DohSettingsRootAction.DohCustomProviderDialogAction.AddCustomClicked(
                            customProvider,
                            url,
                        ),
                    )
                },
                onDefaultInfoClicked = {
                    store.dispatch(
                        DohSettingsRootAction.DefaultInfoClicked,
                    )
                },
                onIncreasedInfoClicked = {
                    store.dispatch(
                        DohSettingsRootAction.IncreasedInfoClicked,
                    )
                },
                onMaxInfoClicked = {
                    store.dispatch(
                        DohSettingsRootAction.MaxInfoClicked,
                    )
                },
            )
        }
    }
}

/**
 * Destination routes within the settings screen
 */
internal object DohSettingsDestinations {
    const val ROOT = "doh:settings:root"
}
