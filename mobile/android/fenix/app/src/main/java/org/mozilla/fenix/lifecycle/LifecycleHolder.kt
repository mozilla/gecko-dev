/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lifecycle

import android.content.Context
import androidx.navigation.NavController
import org.mozilla.fenix.HomeActivity

/**
 * A helper class to be able to change the reference to objects that get replaced when the activity
 * gets recreated.
 *
 * @property context the android [Context]
 * @property navController A [NavController] for interacting with the androidx navigation library.
 * @property composeNavController A [NavController] for navigating within the local Composable nav graph.
 * @property homeActivity so that we can reference openToBrowserAndLoad and browsingMode :(
 */
class LifecycleHolder(
    var context: Context,
    var navController: NavController,
    var composeNavController: NavController,
    var homeActivity: HomeActivity,
)
