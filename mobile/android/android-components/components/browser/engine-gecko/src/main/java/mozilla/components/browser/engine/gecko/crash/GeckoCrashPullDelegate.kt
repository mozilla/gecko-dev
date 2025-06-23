/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.gecko.crash

import org.mozilla.geckoview.CrashPullController
import org.mozilla.geckoview.CrashPullController.Delegate

/**
 * [CrashPullController.Delegate] implementation
 *
 * An abstraction that allows a dispatcher to get the list of CrashIDs coming
 * from Remote Settings and in charge of performing the correct AppStore
 * dispatch for proper UI handling.
 *
 * @param dispatcher The dispatcher callback that is in charge of performing the
 *                   heavy lifting of dispatching the CrashIDs to the AppStore
 */
class GeckoCrashPullDelegate(
    private val dispatcher: (crashIDs: Array<String>) -> Unit,
) : CrashPullController.Delegate {
    override fun onCrashPull(crashIDs: Array<String>) {
        dispatcher(crashIDs)
    }
}
