/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import mozilla.components.lib.state.Action

internal sealed interface DohSettingsAction : Action

/**
 * The Store is initializing.
 */
internal data object Init : DohSettingsAction

/**
 * The user has clicked the back button.
 */
internal data object BackClicked : DohSettingsAction

/**
 * The user has clicked the "Learn More".
 *
 * @property url The "Learn More" url.
 */
internal data class LearnMoreClicked(
    val url: String,
) : DohSettingsAction

/**
 * Actions specific to the DoH Settings Root screen.
 */
internal sealed class DohSettingsRootAction : DohSettingsAction {

    /**
     * The DoH Settings have been loaded.
     *
     * @property allProtectionLevels All possible DoH protection levels.
     * @property selectedProtectionLevel Currently selected protection level.
     * @property providers List of available DoH providers.
     * @property selectedProvider Currently selected DoH provider.
     * @property exceptionsList Current list of domain exceptions.
     */
    data class DohSettingsLoaded(
        val allProtectionLevels: List<ProtectionLevel>,
        val selectedProtectionLevel: ProtectionLevel,
        val providers: List<Provider>,
        val selectedProvider: Provider?,
        val exceptionsList: List<String>,
    ) : DohSettingsAction

    /**
     * The user has opened the ExceptionsListScreen.
     */
    data object ExceptionsClicked : DohSettingsRootAction()

    /**
     * The user has selected a DoH protection level and possibly a provider.
     *
     * @property protectionLevel The chosen DoH protection level.
     * @property provider The chosen provider (may be null if the protectionLevel is Default or Off).
     */
    data class DohOptionSelected(
        val protectionLevel: ProtectionLevel,
        val provider: Provider?,
    ) : DohSettingsRootAction()

    /**
     * The user has clicked on a custom provider setting.
     */
    data object CustomClicked : DohSettingsRootAction()

    /**
     * The user has clicked to see information about the default DoH level.
     */
    data object DefaultInfoClicked : DohSettingsAction

    /**
     * The user has clicked to see information about the increased DoH level.
     */
    data object IncreasedInfoClicked : DohSettingsAction

    /**
     * The user has clicked to see information about the maximum DoH level.
     */
    data object MaxInfoClicked : DohSettingsAction

    /**
     * Actions specific to handling custom DoH provider dialogs.
     */
    sealed class DohCustomProviderDialogAction : DohSettingsRootAction() {

        /**
         * The user has clicked to add a custom provider with a specific URL.
         *
         * @property customProvider The custom DoH provider definition.
         * @property url The URL entered by the user.
         */
        data class AddCustomClicked(
            val customProvider: Provider.Custom,
            val url: String,
        ) : DohSettingsAction

        /**
         * A non-HTTPS URL has been detected for the custom DoH provider.
         */
        data object NonHttpsUrlDetected : DohSettingsAction

        /**
         * An invalid URL has been detected for the custom DoH provider.
         */
        data object InvalidUrlDetected : DohSettingsAction

        /**
         * A valid HTTPS URL has been detected for the custom DoH provider.
         *
         * @property customProvider The custom DoH provider definition.
         * @property url The valid URL.
         */
        data class ValidUrlDetected(
            val customProvider: Provider.Custom,
            val url: String,
        ) : DohSettingsAction

        /**
         * The user has clicked to cancel adding or editing a custom DoH provider.
         *
         */
        data object CancelClicked : DohSettingsAction
    }
}

/**
 * Actions specific to the DoH Settings Exceptions screen.
 */
internal sealed class ExceptionsAction : DohSettingsAction {

    /**
     * The user wants to remove a specific exception.
     *
     * @property url The URL to be removed.
     */
    data class RemoveClicked(val url: String) : DohSettingsAction

    /**
     * The user has clicked to remove all exceptions.
     */
    data object RemoveAllClicked : DohSettingsAction

    /**
     * The user has clicked to add exceptions to the list.
     */
    data object AddExceptionsClicked : DohSettingsAction

    /**
     * The exceptions list has been updated.
     *
     * @property exceptionsList The new list of exceptions.
     */
    data class ExceptionsUpdated(val exceptionsList: List<String>) : DohSettingsAction

    /**
     * The user has clicked save after typing in a new exception URL.
     *
     * @property url The URL entered by the user.
     */
    data class SaveClicked(val url: String) : DohSettingsAction

    /**
     * An invalid URL has been detected while adding an exception.
     */
    data object InvalidUrlDetected : DohSettingsAction
}
