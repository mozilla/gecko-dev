/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import mozilla.components.lib.state.Action

/**
 * [Action]s for updating the [BrowserToolbarState] via [BrowserToolbarStore].
 */
sealed class BrowserToolbarAction : Action {
    /**
     * Updates whether the toolbar is in "display" or "edit" mode.
     *
     * @property editMode Whether or not the toolbar is in "edit" mode.
     */
    data class ToggleEditMode(val editMode: Boolean) : BrowserToolbarAction()
}

/**
 * [BrowserToolbarAction] implementations related to updating the browser edit toolbar.
 */
sealed class BrowserEditToolbarAction : BrowserToolbarAction() {
    /**
     * Updates the text of the toolbar that is currently being edited (in "edit" mode).
     *
     * @property text The text in the toolbar that is being edited.
     */
    data class UpdateEditText(val text: String) : BrowserEditToolbarAction()
}
