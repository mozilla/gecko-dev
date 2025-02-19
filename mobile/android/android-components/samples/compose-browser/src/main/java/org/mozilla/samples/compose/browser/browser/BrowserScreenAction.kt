/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.browser

import mozilla.components.lib.state.Action

/**
 * Actions for updating the [BrowserScreenState] via [BrowserScreenStore].
 */
sealed class BrowserScreenAction : Action {
    /**
     * Shows the list of tabs on top of the web content.
     */
    object ShowTabs : BrowserScreenAction()

    /**
     * Hides the list of tabs.
     */
    object HideTabs : BrowserScreenAction()
}
