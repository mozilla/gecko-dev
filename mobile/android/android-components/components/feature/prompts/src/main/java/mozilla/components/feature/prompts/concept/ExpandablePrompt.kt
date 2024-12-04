/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

/**
 * A prompt that can be expanded and collapsed.
 */
interface ExpandablePrompt {
    /**
     * Listener for when this prompt is expanded or collapsed.
     */
    var expandablePromptListener: Listener?

    /**
     * Expand this prompt.
     */
    fun expand()

    /**
     * Collapse this prompt.
     */
    fun collapse()

    /**
     * Listener for when this prompt is expanded or collapsed.
     */
    interface Listener {
        /**
         * Informs when the prompt has been expanded.
         */
        fun onExpanded()

        /**
         * Informs when the prompt has been collapsed.
         */
        fun onCollapsed()
    }
}
