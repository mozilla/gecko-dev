/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

/**
 * Reader mode status of the current page.
 *
 * @property isAvailable Whether or not reader mode is available for the current page.
 * @property isActive Whether or not reader mode is active for the current page.
 */
data class ReaderModeStatus(
    val isAvailable: Boolean,
    val isActive: Boolean,
) {
    /**
     * Static configuration and properties of [ReaderModeStatus].
     */
    companion object {
        /**
         * [ReaderModeStatus] for when reader mode is not available for the current page.
         */
        val UNKNOWN = ReaderModeStatus(false, false)
    }
}
