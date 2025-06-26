/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.store

import mozilla.components.compose.browser.toolbar.concept.PageOrigin
import mozilla.components.concept.toolbar.AutocompleteProvider
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

    /**
     * Commits the currently edited URL/text and typically switches back to display mode.
     * This action is dispatched when the user submits their input (e.g., by pressing enter
     * or tapping a submit button) in the edit toolbar.
     *
     * @property text The text to commit as the final URL or search query.
     */
    data class CommitUrl(val text: String) : BrowserToolbarAction
}

/**
 * [BrowserToolbarAction] implementations related to updating the browser display toolbar.
 */
sealed class BrowserDisplayToolbarAction : BrowserToolbarAction {
    /**
     * Replaces the currently displayed list of start browser actions with the provided list of actions.
     * These are displayed to the left side of the URL, outside of the URL bounding box.
     *
     * @property actions The new list of [ToolbarAction]s.
     */
    data class BrowserActionsStartUpdated(val actions: List<ToolbarAction>) : BrowserDisplayToolbarAction()

    /**
     * Replaces the currently displayed list of start page actions with the provided list of actions.
     * These are displayed to the left side of the URL, inside of the URL bounding box.
     *
     * @property actions The new list of [ToolbarAction]s.
     */
    data class PageActionsStartUpdated(val actions: List<ToolbarAction>) : BrowserDisplayToolbarAction()

    /**
     * Replace the currently displayed details of the page with the newly provided details.
     *
     * @property pageOrigin The new details of the current webpage.
     */
    data class PageOriginUpdated(val pageOrigin: PageOrigin) : BrowserDisplayToolbarAction()

    /**
     * Replaces the currently displayed list of end page actions with the provided list of actions.
     * These are displayed to the right side of the URL, inside of the URL bounding box.
     *
     * @property actions The new list of [ToolbarAction]s.
     */
    data class PageActionsEndUpdated(val actions: List<ToolbarAction>) : BrowserDisplayToolbarAction()

    /**
     * Replaces the currently displayed list of end browser actions with the provided list of actions.
     * These are displayed to the right side of the URL, outside of the URL bounding box.
     *
     * @property actions The new list of [ToolbarAction]s.
     */
    data class BrowserActionsEndUpdated(val actions: List<ToolbarAction>) : BrowserDisplayToolbarAction()

    /**
     * Updates the [ProgressBarConfig] of the display toolbar.
     *
     * @property config The new configuration for what progress bar to show.
     */
    data class UpdateProgressBarConfig(val config: ProgressBarConfig?) : BrowserDisplayToolbarAction()
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
     * Indicates that a new url suggestion has been autocompleted in the search toolbar.
     */
    data class UrlSuggestionAutocompleted(val url: String) : BrowserEditToolbarAction()

    /**
     * Indicates that a new list of toolbar autocomplete providers is available.
     *
     * @property autocompleteProviders The new list of [AutocompleteProvider]s.
     */
    data class AutocompleteProvidersUpdated(
        val autocompleteProviders: List<AutocompleteProvider>,
    ) : BrowserEditToolbarAction()

    /**
     * Replaces the currently displayed list of start actions while searching with the provided list of actions.
     * These are displayed to the start of the input query, in the same bounding box.
     *
     * @property actions The new list of [ToolbarAction]s.
     */
    data class SearchActionsStartUpdated(val actions: List<ToolbarAction>) : BrowserEditToolbarAction()

    /**
     * Replaces the currently displayed list of end actions while searching with the provided list of actions.
     * These are displayed to the end of the input query, in the same bounding box.
     *
     * @property actions The new list of [ToolbarAction]s.
     */
    data class SearchActionsEndUpdated(val actions: List<ToolbarAction>) : BrowserEditToolbarAction()
}
