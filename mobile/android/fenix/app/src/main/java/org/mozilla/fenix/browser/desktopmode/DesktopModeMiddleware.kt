/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.desktopmode

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.DefaultDesktopModeAction
import mozilla.components.browser.state.action.InitAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] for handling side effects related to the Desktop Mode feature.
 *
 * @param scope [CoroutineScope] used for writing settings changes to disk.
 * @param repository [DesktopModeRepository] used to interact with the desktop mode preference.
 * @param desktopModeFeatureFlag [DefaultDesktopModeFeatureFlag] used to determine if the desktop mode
 * feature is enabled.
 */
class DesktopModeMiddleware(
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
    private val repository: DesktopModeRepository,
    private val desktopModeFeatureFlag: DefaultDesktopModeFeatureFlag = DefaultDesktopModeFeatureFlagImpl(),
) : Middleware<BrowserState, BrowserAction> {

    override fun invoke(
        context: MiddlewareContext<BrowserState, BrowserAction>,
        next: (BrowserAction) -> Unit,
        action: BrowserAction,
    ) {
        next(action)

        if (!desktopModeFeatureFlag.isDesktopModeEnabled()) {
            return
        }

        when (action) {
            InitAction -> {
                scope.launch {
                    context.store.dispatch(
                        DefaultDesktopModeAction.DesktopModeUpdated(
                            newValue = repository.getDesktopBrowsingEnabled(),
                        ),
                    )
                }
            }

            DefaultDesktopModeAction.ToggleDesktopMode -> {
                scope.launch {
                    val updatedDesktopMode = context.state.desktopMode
                    val preferenceWriteSucceeded = repository.setDesktopBrowsingEnabled(updatedDesktopMode)

                    if (!preferenceWriteSucceeded) {
                        // If the preference write fails, revert the state change.
                        context.store.dispatch(
                            DefaultDesktopModeAction.DesktopModeUpdated(
                                newValue = !updatedDesktopMode,
                            ),
                        )
                    }
                }
            }
            else -> {
                // no-op
            }
        }
    }
}
