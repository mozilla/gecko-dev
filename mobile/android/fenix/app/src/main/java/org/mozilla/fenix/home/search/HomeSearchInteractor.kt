/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.search

/**
 * Homepage actions related to search.
 */
interface HomeSearchInteractor {
    /**
     * Indicate the home content was focused while a browser search is in progress.
     */
    fun onHomeContentFocusedWhileSearchIsActive()
}
