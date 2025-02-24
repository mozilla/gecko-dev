/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import androidx.compose.runtime.Composable
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.rememberNavController

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

    NavHost(
        navController = navController,
        startDestination = startDestination,
    ) {
    }
}
