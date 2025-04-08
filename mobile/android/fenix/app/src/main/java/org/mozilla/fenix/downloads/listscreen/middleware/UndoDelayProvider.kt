/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen.middleware

import org.mozilla.fenix.utils.Settings

/**
 * Interface for providing undo delay value.
 */
interface UndoDelayProvider {

    /**
     * Get the undo delay value.
     */
    val undoDelay: Long
}

/**
 * Real Implementation of [UndoDelayProvider] that uses Settings to define the undo delay value.
 */
class DefaultUndoDelayProvider(settings: Settings) : UndoDelayProvider {
    override val undoDelay: Long = if (settings.accessibilityServicesEnabled) {
        ACCESSIBLE_UNDO_DELAY
    } else {
        UNDO_DELAY
    }

    /**
     * Constants for the [DefaultUndoDelayProvider]
     */
    companion object {
        private const val UNDO_DELAY = 3000L
        private const val ACCESSIBLE_UNDO_DELAY = 15000L
    }
}
