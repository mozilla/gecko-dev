/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import android.app.Activity
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler

/**
 * A feature that removes the tab and selects the parent, if one exists.
 */
class LastTabFeature(
    private val store: BrowserStore,
    private val tabId: String? = null,
    private val removeTabUseCase: TabsUseCases.RemoveTabUseCase,
    private val activity: Activity,
) : LifecycleAwareFeature, UserInteractionHandler {

    override fun start() = Unit
    override fun stop() = Unit

    /**
     * Removes the session if it was opened by an ACTION_VIEW intent
     * or if it has a parent session and no more history.
     */
    override fun onBackPressed(): Boolean {
        val tab = store.state.findTabOrCustomTabOrSelectedTab(tabId) ?: return false
        val isExternalOrCustomTab = tab.source is SessionState.Source.External ||
            tab.source is SessionState.Source.Internal.CustomTab

        return if (isExternalOrCustomTab && !tab.restored) {
            activity.finish()
            removeTabUseCase(tab.id)
            true
        } else {
            val hasParent = tab is TabSessionState && tab.parentId != null
            if (hasParent) {
                removeTabUseCase(
                    tab.id,
                    selectParentIfExists = true,
                )
            }
            // We want to return to home if this session didn't have a parent session to select.
            hasParent
        }
    }
}
