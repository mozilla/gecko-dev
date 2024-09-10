/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import androidx.annotation.VisibleForTesting
import androidx.core.view.isVisible
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChangedBy
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.toolbar.ScrollableToolbar
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.FindInPageIntegration
import org.mozilla.fenix.ext.isToolbarAtBottom

/**
 * The feature responsible for scrolling behaviour of the bottom toolbar container.
 * When the content of a tab is being scrolled, the toolbar will react to user interactions.
 */
class BottomToolbarContainerIntegration(
    toolbar: ScrollableToolbar,
    store: BrowserStore,
    private val appStore: AppStore,
    private val bottomToolbarContainerView: BottomToolbarContainerView,
    sessionId: String?,
    private val findInPageFeature: () -> FindInPageIntegration?,
) : LifecycleAwareFeature {

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    var toolbarController = ToolbarBehaviorController(toolbar, store, sessionId)
    private var scope: CoroutineScope? = null

    override fun start() {
        toolbarController.start()

        scope = appStore.flowScoped { flow ->
            flow.distinctUntilChangedBy { it.isSearchDialogVisible }
                .collect { state ->
                    with(bottomToolbarContainerView.toolbarContainerView) {
                        // When the address bar is positioned at the bottom, we never want bottom container to be
                        // visible if the search dialog is visible or when the find in page bar is showing.
                        // If the address bar is at top and find in page is not showing then the navbar should be
                        // visible whenever the keyboard is hidden, even if the search dialog is present.
                        if (context.isToolbarAtBottom() && (findInPageFeature()?.isFeatureActive != true)) {
                            isVisible = !state.isSearchDialogVisible
                        }
                    }
                }
        }
    }

    override fun stop() {
        toolbarController.stop()
        scope?.cancel()
    }
}
