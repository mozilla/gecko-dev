/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

/**
 * A prompt that can be toggled between visible and hidden states.
 */
interface ToggleablePrompt {
    /**
     * Listener for when this prompt is shown or hidden.
     */
    var toggleablePromptListener: Listener?

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

    /**
     * Listener for when this prompt is shown or hidden.
     */
    interface Listener {
        /**
         * Informs when the prompt has changed from hidden to visible.
         */
        fun onShown()

        /**
         * Informs when the prompt has changed from visible to hidden.
         */
        fun onHidden()
    }
}
