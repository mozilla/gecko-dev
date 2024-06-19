/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.internal

import kotlinx.coroutines.CoroutineScope
import mozilla.components.lib.state.Store
import kotlin.coroutines.CoroutineContext

/**
 * The dispatcher used by a [Store].
 */
internal interface StoreDispatcher {

    /**
     * A [CoroutineContext] that is used within this dispatcher. Typical implementations create a
     * [CoroutineScope] using this context.
     */
    val coroutineContext: CoroutineContext

    /**
     * Asserts that work done is always on the [StoreDispatcher] thread.
     */
    @Throws(IllegalThreadStateException::class)
    fun assertOnThread()
}
