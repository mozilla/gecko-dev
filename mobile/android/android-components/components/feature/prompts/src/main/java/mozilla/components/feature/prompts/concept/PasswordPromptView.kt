/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.concept

/**
 * An interface for views that can display a generated strong password prompt.
 */
interface PasswordPromptView {

    var listener: Listener?

    /**
     * Shows a simple prompt for using a generated password.
     */
    fun showPrompt()

    /**
     * Hides the prompt.
     */
    fun hidePrompt()

    /**
     * Returns true if the prompt is visible and false otherwise.
     */
    fun isVisible(): Boolean

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
