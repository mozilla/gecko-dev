/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import mozilla.components.concept.engine.Engine
import mozilla.components.lib.state.State

/**
 * Represents the current state of the DoH (DNS-over-HTTPS) settings in the application.
 *
 * @property allProtectionLevels List of all possible DoH protection levels.
 * @property selectedProtectionLevel The currently selected DoH protection level.
 * @property providers List of available DoH providers (can be built-in or custom).
 * @property selectedProvider The currently selected DoH provider (null if selectedProtectionLevel is Default or Off).
 * @property exceptionsList A list of domain exceptions that bypass DoH.
 * @property isUserExceptionValid Indicates whether the user's input for an exception is valid.
 * @property isCustomProviderDialogOn Indicates whether the UI dialog for editing custom provider is shown.
 * @property customProviderErrorState The current validation/error state of the custom provider URL.
 */
internal data class DohSettingsState(
    val allProtectionLevels: List<ProtectionLevel> = emptyList(),
    val selectedProtectionLevel: ProtectionLevel = ProtectionLevel.Default,
    val providers: List<Provider> = emptyList(),
    val selectedProvider: Provider? = null,
    val exceptionsList: List<String> = emptyList(),
    val isUserExceptionValid: Boolean = true,
    val isCustomProviderDialogOn: Boolean = false,
    val customProviderErrorState: CustomProviderErrorState = CustomProviderErrorState.Valid,
) : State

/**
 * Represents the protection level for DoH settings.
 */
internal sealed class ProtectionLevel {
    abstract fun toDohSettingsMode(): Engine.DohSettingsMode

    data object Default : ProtectionLevel() {
        override fun toDohSettingsMode() = Engine.DohSettingsMode.DEFAULT
    }

    data object Increased : ProtectionLevel() {
        override fun toDohSettingsMode() = Engine.DohSettingsMode.INCREASED
    }

    data object Max : ProtectionLevel() {
        override fun toDohSettingsMode() = Engine.DohSettingsMode.MAX
    }

    data object Off : ProtectionLevel() {
        override fun toDohSettingsMode() = Engine.DohSettingsMode.OFF
    }
}

internal sealed class Provider {
    abstract val url: String

    /**
     * A built-in DoH provider.
     *
     * @property url The built-in DoH provider's endpoint URL.
     * @property name The name of the provider (e.g. Cloudflare).
     * @property default Whether this provider is the default.
     */
    data class BuiltIn(
        override val url: String,
        val name: String,
        val default: Boolean = false,
    ) : Provider()

    /**
     * A custom provider specified by the user.
     *
     * @property url The custom built-in DoH provider's endpoint URL.
     */
    data class Custom(
        override val url: String,
    ) : Provider()
}

/**
 * Represents the validation state for a custom provider URL.
 */
internal enum class CustomProviderErrorState {
    /**
     * The URL is non-HTTPS.
     */
    NonHttps,

    /**
     * The URL is invalid or cannot be parsed correctly).
     */
    Invalid,

    /**
     * The URL is valid.
     */
    Valid,
}
