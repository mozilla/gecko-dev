/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

/**
 * A prompt for displaying a list of options that the user can choose to be autocompleted.
 */
interface AutocompletePrompt<T> :
    SelectablePromptView<T>,
    ToggleablePrompt {

    /**
     * Populate the prompt with the provided options.
     *
     * @param options A list of options to display in the prompt.
     */
    fun populate(options: List<T>)
}
