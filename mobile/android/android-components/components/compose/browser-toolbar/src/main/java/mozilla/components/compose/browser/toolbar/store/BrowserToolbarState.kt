/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.lib.state.State

/**
 * The state of the browser toolbar.
 *
 * @property mode The display [Mode] of the browser toolbar.
 * @property displayState Wrapper containing the toolbar display state.
 * @property editState Wrapper containing the toolbar edit state.
 */
data class BrowserToolbarState(
    val mode: Mode = Mode.DISPLAY,
    val displayState: DisplayState = DisplayState(),
    val editState: EditState = EditState(),
) : State {

    /**
     * Returns true if the browser toolbar is in edit mode and false otherwise.
     */
    fun isEditMode() = this.mode == Mode.EDIT
}

/**
 * The various display mode of the browser toolbar.
 */
enum class Mode {
    /**
     * Display mode - Shows the URL and related toolbar actions.
     */
    DISPLAY,

    /**
     * Edit mode - Allows the user to edit the URL.
     */
    EDIT,

    /**
     * Custom tab - Displays the URL and title of a custom tab.
     */
    CUSTOM_TAB,
}

/**
 * Wrapper containing the toolbar display state.
 *
 * @property hint Text displayed in the toolbar when there's no URL to display
 * (no tab or empty URL).
 * @param navigationActions List of navigation [Action]s to be displayed on left side of the
 * display toolbar (outside of the URL bounding box).
 * @param pageActions List of page [Action]s to be displayed to the right side of the URL of the
 * display toolbar. Also see:
 * [MDN docs](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction)
 * @param browserActions List of browser [Action]s to be displayed on the right side of the
 * display toolbar (outside of the URL bounding box). Also see:
 * [MDN docs](https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Browser_action)
 */
data class DisplayState(
    val hint: String = "",
    val navigationActions: List<Action> = emptyList(),
    val pageActions: List<Action> = emptyList(),
    val browserActions: List<Action> = emptyList(),
) : State

/**
 * Wrapper containing the toolbar edit state.
 *
 * @property editText The text the user is editing in "edit" mode.
 * @property editActionsStart List of [Action]s to be displayed at the start of the URL of
 * the edit toolbar.
 * @property editActionsEnd List of [Action]s to be displayed at the end of the URL of
 * the edit toolbar.
 */
data class EditState(
    val editText: String? = null,
    val editActionsStart: List<Action> = emptyList(),
    val editActionsEnd: List<Action> = emptyList(),
) : State
