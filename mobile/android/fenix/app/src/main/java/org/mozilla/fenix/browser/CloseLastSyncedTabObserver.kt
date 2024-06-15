/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import androidx.navigation.NavController
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.feature.accounts.push.CloseTabsCommandReceiver

/**
 * A [CloseTabsCommandReceiver.Observer] that navigates back to the
 * home screen when the last tab is closed, so that the user
 * doesn't see a ghost web content view for the closed tab.
 *
 * @param scope The [CoroutineScope] to use for launching coroutines.
 * @param navController The [NavController] to use for navigation.
 */
class CloseLastSyncedTabObserver(
    private val scope: CoroutineScope,
    private val navController: NavController,
) : CloseTabsCommandReceiver.Observer {
    override fun onLastTabClosed() {
        // Observers aren't guaranteed to be called on a specific thread,
        // and `NavController` is main thread-only.
        scope.launch(Dispatchers.Main) {
            val directions = BrowserFragmentDirections.actionGlobalHome(focusOnAddressBar = true)
            navController.navigate(directions)
        }
    }
}
