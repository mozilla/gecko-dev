/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

/**
 * A prompt that can be toggled between visible and hidden states.
 */
interface ToggleablePrompt {
    /**
     * Whether the prompt is currently visible.
     */
    val isPromptDisplayed: Boolean

    /**
     * Shows this prompt.
     */
    fun showPrompt()

    /**
     * Hide this prompt.
     */
    fun hidePrompt()
}
