/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.app.Activity
import androidx.lifecycle.ViewModel
import androidx.navigation.NavController
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction.AddBrowserAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.components.toolbar.DisplayActions.MenuClicked
import org.mozilla.fenix.ext.nav

private sealed class DisplayActions : BrowserToolbarEvent {
    data object MenuClicked : DisplayActions()
}

/**
 * [Middleware] responsible for configuring and handling interactions with the composable toolbar.
 *
 * This is also a [ViewModel] allowing to be easily persisted between activity restarts.
 */
class BrowserToolbarMiddleware : Middleware<BrowserToolbarState, BrowserToolbarAction>, ViewModel() {
    private lateinit var navController: NavController

    /**
     * Update dependencies tied to the lifecycle of the [Activity] to prevent these leaking.
     *
     * @param navController [NavController] to use for navigating to other in-app destinations.
     */
    fun updateLifecycleDependencies(navController: NavController) {
        this.navController = navController
    }

    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        when (action) {
            is BrowserToolbarAction.Init -> {
                context.dispatch(
                    AddBrowserAction(
                        Action.ActionButton(
                            icon = R.drawable.mozac_ic_ellipsis_vertical_24,
                            contentDescription = R.string.content_description_menu,
                            tint = R.attr.actionPrimary,
                            onClick = MenuClicked,
                        ),
                    ),
                )
            }
            is MenuClicked -> {
                navController.nav(
                    R.id.browserFragment,
                    BrowserFragmentDirections.actionGlobalMenuDialogFragment(
                        accesspoint = MenuAccessPoint.Browser,
                    ),
                )
            }

            else -> next(action)
        }
    }
}
