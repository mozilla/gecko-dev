/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

/**
 * A prompt that can display a set of options for the user to choose from.
 */
interface SelectablePromptView<T> {

    /**
     * Listener for user interactions with the prompt.
     */
    var selectablePromptListener: Listener<T>?

    /**
     * Interface to allow a class to listen to the option selection prompt events.
     */
    interface Listener<in T> {
        /**
         * Called when an user selects an options from the prompt.
         *
         * @param option The selected option.
         */
        fun onOptionSelect(option: T)

        /**
         * Called when the user invokes the option to manage the list of options.
         */
        fun onManageOptions()
    }
}
