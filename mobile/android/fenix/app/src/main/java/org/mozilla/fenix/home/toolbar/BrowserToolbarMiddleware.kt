/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.toolbar

import androidx.lifecycle.ViewModel
import androidx.navigation.NavController
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.store.BrowserDisplayToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.MenuAccessPoint
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.home.HomeFragmentDirections

private sealed class DisplayActions : BrowserToolbarEvent {
    data object MenuClicked : DisplayActions()
}

/**
 * [Middleware] responsible for configuring and handling interactions with the composable toolbar.
 *
 * This is also a [ViewModel] allowing to be easily persisted between activity restarts.
 */
class BrowserToolbarMiddleware : Middleware<BrowserToolbarState, BrowserToolbarAction>, ViewModel() {
    private lateinit var dependencies: LifecycleDependencies

    /**
     * Updates the [LifecycleDependencies] of this middleware.
     *
     * @param dependencies The new [LifecycleDependencies].
     */
    fun updateLifecycleDependencies(dependencies: LifecycleDependencies) {
        this.dependencies = dependencies
    }

    override fun invoke(
        context: MiddlewareContext<BrowserToolbarState, BrowserToolbarAction>,
        next: (BrowserToolbarAction) -> Unit,
        action: BrowserToolbarAction,
    ) {
        when (action) {
            is BrowserToolbarAction.Init -> {
                context.dispatch(
                    BrowserDisplayToolbarAction.AddBrowserAction(
                        Action.ActionButton(
                            icon = R.drawable.mozac_ic_ellipsis_vertical_24,
                            contentDescription = R.string.content_description_menu,
                            tint = R.attr.actionPrimary,
                            onClick = DisplayActions.MenuClicked,
                        ),
                    ),
                )
            }
            is DisplayActions.MenuClicked -> {
                dependencies.navController.nav(
                    R.id.homeFragment,
                    HomeFragmentDirections.actionGlobalMenuDialogFragment(
                        accesspoint = MenuAccessPoint.Home,
                    ),
                )
            }

            else -> next(action)
        }
    }

    /**
     * Lifecycle dependencies for the [BrowserToolbarMiddleware].
     *
     * @property navController [NavController] to use for navigating to other in-app destinations.
     */
    data class LifecycleDependencies(
        val navController: NavController,
    )
}
