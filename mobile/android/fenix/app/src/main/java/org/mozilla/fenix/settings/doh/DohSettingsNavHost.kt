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
import org.mozilla.fenix.settings.doh.addexception.AddExceptionScreen
import org.mozilla.fenix.settings.doh.exceptionslist.ExceptionsListScreen
import org.mozilla.fenix.settings.doh.info.InfoScreen
import org.mozilla.fenix.settings.doh.info.InfoScreenTopic
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

        composable(route = DohSettingsDestinations.INFO_DEFAULT) {
            InfoScreen(
                infoScreenTopic = InfoScreenTopic.DEFAULT,
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
            )
        }
        composable(route = DohSettingsDestinations.INFO_INCREASED) {
            InfoScreen(
                infoScreenTopic = InfoScreenTopic.INCREASED,
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
            )
        }
        composable(route = DohSettingsDestinations.INFO_MAX) {
            InfoScreen(
                infoScreenTopic = InfoScreenTopic.MAX,
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
            )
        }

        composable(route = DohSettingsDestinations.EXCEPTIONS_LIST) {
            val state by store.observeAsState(store.state) { it }
            ExceptionsListScreen(
                state = state,
                onNavigateUp = {
                    store.dispatch(
                        BackClicked,
                    )
                },
                onAddExceptionsClicked = {
                    store.dispatch(
                        ExceptionsAction.AddExceptionsClicked,
                    )
                },
                onRemoveClicked = { url ->
                    store.dispatch(
                        ExceptionsAction.RemoveClicked(url),
                    )
                },
                onRemoveAllClicked = {
                    store.dispatch(
                        ExceptionsAction.RemoveAllClicked,
                    )
                },
            )
        }

        composable(route = DohSettingsDestinations.ADD_EXCEPTION) {
            val state by store.observeAsState(store.state) { it }
            AddExceptionScreen(
                state = state,
                onNavigateUp = {
                    store.dispatch(
                        BackClicked,
                    )
                },
                onSaveClicked = { url ->
                    store.dispatch(
                        ExceptionsAction.SaveClicked(url),
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
    const val INFO_DEFAULT = "doh:settings:info"
    const val INFO_INCREASED = "doh:settings:info-increased"
    const val INFO_MAX = "doh:settings:info-max"
    const val EXCEPTIONS_LIST = "doh:settings:list-exceptions"
    const val ADD_EXCEPTION = "doh:settings:add-exception"
}
