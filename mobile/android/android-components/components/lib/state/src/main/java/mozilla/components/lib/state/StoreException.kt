/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state

/**
 * Exception for otherwise unhandled errors caught while reducing state or
 * while managing/notifying observers.
 */
class StoreException(msg: String, val e: Throwable? = null) : Exception(msg, e)
