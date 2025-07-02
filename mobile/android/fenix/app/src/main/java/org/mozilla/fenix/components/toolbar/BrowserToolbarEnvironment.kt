/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import mozilla.components.browser.thumbnails.BrowserThumbnails
import mozilla.components.compose.browser.toolbar.store.Environment
import org.mozilla.fenix.browser.BrowserAnimator
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.browser.readermode.ReaderModeController

/**
 * The current environment in which the browser toolbar is used allowing access to various
 * other application features that the toolbar integrates with.
 *
 * This is Activity/Fragment lifecycle dependent and should be handled carefully to avoid memory leaks.
 *
 * @property context [Context] used for various system interactions.
 * @property viewLifecycleOwner [LifecycleOwner] depending on which lifecycle related operations will be scheduled.
 * @property navController [NavController] to use for navigating to other in-app destinations.
 * @property browsingModeManager [BrowsingModeManager] for querying the current browsing mode.
 * @property browserAnimator Helper for animating the browser content when navigating to other screens.
 * @property thumbnailsFeature [BrowserThumbnails] for requesting screenshots of the current tab.
 * @property readerModeController [ReaderModeController] for showing or hiding the reader view UX.
 */
data class BrowserToolbarEnvironment(
    val context: Context,
    val viewLifecycleOwner: LifecycleOwner,
    val navController: NavController,
    val browsingModeManager: BrowsingModeManager,
    val browserAnimator: BrowserAnimator,
    val thumbnailsFeature: BrowserThumbnails?,
    val readerModeController: ReaderModeController,
) : Environment
