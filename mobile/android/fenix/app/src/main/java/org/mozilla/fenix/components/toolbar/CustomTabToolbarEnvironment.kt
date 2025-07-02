/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import mozilla.components.compose.browser.toolbar.store.Environment

/**
 * The current environment in which the browser toolbar is used allowing access to various
 * other application features that the toolbar integrates with.
 *
 * This is Activity/Fragment lifecycle dependent and should be handled carefully to avoid memory leaks.
 *
 * @property context [Context] to access application resources and interact with other system functionalities.
 * @property viewLifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
 * @property navController [NavController] to use for navigating to other in-app destinations.
 * @property closeTabDelegate Callback for when the current custom tab needs to be closed.
 */
data class CustomTabToolbarEnvironment(
    val context: Context,
    val viewLifecycleOwner: LifecycleOwner,
    val navController: NavController,
    val closeTabDelegate: () -> Unit,
) : Environment
