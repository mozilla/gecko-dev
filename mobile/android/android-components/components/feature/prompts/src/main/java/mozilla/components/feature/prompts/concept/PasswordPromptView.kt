/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

/**
 * A prompt for displaying a generated strong password.
 */
interface PasswordPromptView : ToggleablePrompt {

    /**
     * Listener for user interactions with the prompt.
     *
     */
    var passwordPromptListener: Listener?

    /**
     * Interface to allow a class to listen to generated strong password event events.
     */
    interface Listener {
        /**
         * Called when a user clicks on the password generator prompt
         */
        fun onGeneratedPasswordPromptClick()
    }
}
