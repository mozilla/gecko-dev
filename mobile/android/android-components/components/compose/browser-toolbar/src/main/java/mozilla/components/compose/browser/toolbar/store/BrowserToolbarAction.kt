/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import mozilla.components.lib.state.Action
import mozilla.components.compose.browser.toolbar.concept.Action as ToolbarAction

/**
 * [Action]s for updating the [BrowserToolbarState] via [BrowserToolbarStore].
 */
sealed interface BrowserToolbarAction : Action {
    /**
     * Updates whether the toolbar is in "display" or "edit" mode.
     *
     * @property editMode Whether or not the toolbar is in "edit" mode.
     */
    data class ToggleEditMode(val editMode: Boolean) : BrowserToolbarAction

    /**
     * Initialize the toolbar with the provided data.
     *
     * @property mode The initial mode of the toolbar.
     * @property displayState The initial state of the display toolbar.
     * @property editState The initial state of the edit toolbar.
     */
    data class Init(
        val mode: Mode = Mode.DISPLAY,
        val displayState: DisplayState = DisplayState(),
        val editState: EditState = EditState(),
    ) : BrowserToolbarAction
}

/**
 * [BrowserToolbarAction] implementations related to updating the browser display toolbar.
 */
sealed class BrowserDisplayToolbarAction : BrowserToolbarAction {
    /**
     * Adds a navigation [Action] to be displayed to the left side of the URL of the display toolbar
     * (outside of the URL bounding box).
     *
     * @property action The [Action] to be added.
     */
    data class AddNavigationAction(val action: ToolbarAction) : BrowserDisplayToolbarAction()

    /**
     * Adds a page [Action] to be displayed to the right side of the URL of the display toolbar.
     *
     * @property action The [Action] to be added.
     */
    data class AddPageAction(val action: ToolbarAction) : BrowserDisplayToolbarAction()

    /**
     * Adds a browser [Action] to be displayed to the right side of the URL of the display toolbar
     * (outside of the URL bounding box).
     *
     * @property action The [Action] to be added.
     */
    data class AddBrowserAction(val action: ToolbarAction) : BrowserDisplayToolbarAction()
}

/**
 * [BrowserToolbarAction] implementations related to updating the browser edit toolbar.
 */
sealed class BrowserEditToolbarAction : BrowserToolbarAction {
    /**
     * Updates the text of the toolbar that is currently being edited (in "edit" mode).
     *
     * @property text The text in the toolbar that is being edited.
     */
    data class UpdateEditText(val text: String) : BrowserEditToolbarAction()

    /**
     * Adds an [Action] to be displayed at the start of the URL in the browser edit toolbar.
     *
     * @property action The [Action] to be added.
     */
    data class AddEditActionStart(val action: ToolbarAction) : BrowserEditToolbarAction()

    /**
     * Adds an [Action] to be displayed at the end of the URL in the browser edit toolbar.
     *
     * @property action The [Action] to be added.
     */
    data class AddEditActionEnd(val action: ToolbarAction) : BrowserEditToolbarAction()
}
