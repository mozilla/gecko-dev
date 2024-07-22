/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.readerview

/**
 * The state of the reader view to display.
 */
sealed class ReaderViewState {

    /**
     * Reader view is active.
     */
    data object Active : ReaderViewState()

    /**
     * Dismiss reader view.
     */
    data object Dismiss : ReaderViewState()

    /**
     * Reader view is active and the reader view controls should be displayed.
     */
    data object ShowControls : ReaderViewState()

    /**
     * No reader view state to display.
     */
    data object None : ReaderViewState()
}
