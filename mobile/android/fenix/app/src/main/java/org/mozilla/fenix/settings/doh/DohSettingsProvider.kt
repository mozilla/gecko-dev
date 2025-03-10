/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.Engine
import org.mozilla.fenix.utils.Settings

internal interface DohSettingsProvider {
    fun getProtectionLevels(): List<ProtectionLevel>
    fun getDefaultProviders(): List<Provider>
    fun getSelectedProtectionLevel(): ProtectionLevel
    fun getSelectedProvider(): Provider?
    fun getExceptions(): List<String>
    fun setProtectionLevel(protectionLevel: ProtectionLevel, provider: Provider?)
    fun setCustomProvider(url: String)
    fun setExceptions(exceptions: List<String>)
}

internal class DefaultDohSettingsProvider(
    val engine: Engine,
    val settings: Settings,
) : DohSettingsProvider {
    override fun getProtectionLevels(): List<ProtectionLevel> {
        return listOf(
            ProtectionLevel.Default,
            ProtectionLevel.Increased,
            ProtectionLevel.Max,
            ProtectionLevel.Off,
        )
    }

    // Bug 1946867 - Load DoH Provider List from Gecko instead of hardcoding
    private val dohDefaultProviderUrl = settings.dohDefaultProviderUrl

    private val cloudflare = Provider.BuiltIn(
        url = cloudflareUri,
        name = "Cloudflare",
        default = dohDefaultProviderUrl.isBlank() || dohDefaultProviderUrl == cloudflareUri,
    )
    private val nextDns = Provider.BuiltIn(
        url = nextDnsUri,
        name = "NextDNS",
        default = dohDefaultProviderUrl == nextDnsUri,
    )
    private val providerUrl = settings.dohProviderUrl
    private val custom = Provider.Custom(
        url = if (providerUrl != cloudflareUri && providerUrl != nextDnsUri) {
            providerUrl
        } else {
            ""
        },
    )

    override fun getDefaultProviders(): List<Provider> = listOf(
        cloudflare,
        nextDns,
        custom,
    )

    override fun getSelectedProtectionLevel(): ProtectionLevel {
        return when (settings.getDohSettingsMode()) {
            Engine.DohSettingsMode.DEFAULT -> ProtectionLevel.Default
            Engine.DohSettingsMode.INCREASED -> ProtectionLevel.Increased
            Engine.DohSettingsMode.MAX -> ProtectionLevel.Max
            Engine.DohSettingsMode.OFF -> ProtectionLevel.Off
        }
    }

    override fun getSelectedProvider(): Provider? {
        return when (settings.getDohSettingsMode()) {
            Engine.DohSettingsMode.OFF, Engine.DohSettingsMode.DEFAULT -> {
                null
            }

            else -> {
                when (settings.dohProviderUrl) {
                    cloudflareUri -> cloudflare
                    nextDnsUri -> nextDns
                    "" -> getDefaultProviders().first()
                    else -> custom
                }
            }
        }
    }

    override fun getExceptions(): List<String> {
        return settings.dohExceptionsList.toList()
    }

    override fun setProtectionLevel(protectionLevel: ProtectionLevel, provider: Provider?) {
        if (protectionLevel is ProtectionLevel.Increased || protectionLevel is ProtectionLevel.Max) {
            requireNotNull(provider) { "Provider must not be null for Increased/Max protection level" }
            settings.dohProviderUrl = provider.url
            engine.settings.dohProviderUrl = provider.url
        }

        val newMode = protectionLevel.toDohSettingsMode()
        // Update the app layer
        settings.setDohSettingsMode(newMode)
        engine.settings.dohSettingsMode = newMode
    }

    override fun setCustomProvider(url: String) {
        // Update the app layer
        settings.dohProviderUrl = url
        // validate the url, maybe throw some "known expectations" that we can handle in the middleware
        engine.settings.dohProviderUrl = url
    }

    override fun setExceptions(exceptions: List<String>) {
        // Update the app layer
        settings.dohExceptionsList = exceptions.toSet()
        engine.settings.dohExceptionsList = exceptions
    }

    companion object {
        @VisibleForTesting
        val cloudflareUri = "mozilla.cloudflare-dns.com"

        @VisibleForTesting
        val nextDnsUri = "firefox.dns.nextdns.io"
    }
}
