/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

internal class FakeDohSettingsProvider(
    private var expectedProtectionLevels: List<ProtectionLevel> = listOf(
        ProtectionLevel.Default,
        ProtectionLevel.Increased,
        ProtectionLevel.Max,
        ProtectionLevel.Off,
    ),
    private var selectedProtectionLevel: ProtectionLevel = ProtectionLevel.Default,
    private var exceptionsList: List<String> = emptyList(),
    private var selectedProvider: Provider? = null,
) : DohSettingsProvider {
    override fun getProtectionLevels(): List<ProtectionLevel> = expectedProtectionLevels

    override fun getDefaultProviders(): List<Provider> = defaultProviders

    fun getBuiltInProvider(): Provider.BuiltIn = builtIn
    fun getCustomProvider(): Provider.Custom = custom

    override fun getSelectedProtectionLevel(): ProtectionLevel = selectedProtectionLevel

    override fun getSelectedProvider(): Provider? = selectedProvider

    override fun getExceptions(): List<String> = exceptionsList

    override fun setProtectionLevel(protectionLevel: ProtectionLevel, provider: Provider?) {
        selectedProtectionLevel = when (protectionLevel) {
            is ProtectionLevel.Off, ProtectionLevel.Default -> protectionLevel
            is ProtectionLevel.Increased, ProtectionLevel.Max -> {
                require(provider != null) { "Provider must not be null for Increased/Max protection level" }
                selectedProvider = provider
                protectionLevel
            }
        }
    }

    override fun setCustomProvider(url: String) {
        custom = custom.copy(url = url)
    }

    override fun setExceptions(exceptions: List<String>) {
        exceptionsList = exceptions
    }

    companion object {
        private val builtIn = Provider.BuiltIn(
            url = "built.in.provider",
            name = "BuiltIn",
            default = true,
        )
        private var custom = Provider.Custom(
            url = "",
        )
        private val defaultProviders = listOf(builtIn, custom)
    }
}
