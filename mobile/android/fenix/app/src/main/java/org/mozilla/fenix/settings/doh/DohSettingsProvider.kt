/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import androidx.annotation.VisibleForTesting
import mozilla.components.concept.engine.Engine

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
    private val dohDefaultProviderUrl = engine.settings.dohDefaultProviderUrl

    private val cloudflare = Provider.BuiltIn(
        url = cloudflareUri,
        name = "Cloudflare",
        default = dohDefaultProviderUrl.isNullOrBlank() || dohDefaultProviderUrl == cloudflareUri,
    )
    private val nextDns = Provider.BuiltIn(
        url = nextDnsUri,
        name = "NextDNS",
        default = dohDefaultProviderUrl == nextDnsUri,
    )
    private val providerUrl = engine.settings.dohProviderUrl
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
        return when (engine.settings.dohSettingsMode) {
            Engine.DohSettingsMode.DEFAULT -> ProtectionLevel.Default
            Engine.DohSettingsMode.INCREASED -> ProtectionLevel.Increased
            Engine.DohSettingsMode.MAX -> ProtectionLevel.Max
            Engine.DohSettingsMode.OFF -> ProtectionLevel.Off
        }
    }

    override fun getSelectedProvider(): Provider? {
        return when (engine.settings.dohSettingsMode) {
            Engine.DohSettingsMode.OFF, Engine.DohSettingsMode.DEFAULT -> {
                null
            }

            else -> {
                when (engine.settings.dohProviderUrl) {
                    cloudflareUri -> cloudflare
                    nextDnsUri -> nextDns
                    "" -> getDefaultProviders().first()
                    else -> custom
                }
            }
        }
    }

    override fun getExceptions(): List<String> {
        return engine.settings.dohExceptionsList
    }

    override fun setProtectionLevel(protectionLevel: ProtectionLevel, provider: Provider?) {
        engine.settings.dohSettingsMode = when (protectionLevel) {
            is ProtectionLevel.Off -> Engine.DohSettingsMode.OFF
            is ProtectionLevel.Default -> Engine.DohSettingsMode.DEFAULT
            is ProtectionLevel.Increased -> {
                require(provider != null) { "Provider must not be null for Increased protection level" }
                engine.settings.dohProviderUrl = provider.url
                Engine.DohSettingsMode.INCREASED
            }

            is ProtectionLevel.Max -> {
                require(provider != null) { "Provider must not be null for Max protection level" }
                engine.settings.dohProviderUrl = provider.url
                Engine.DohSettingsMode.MAX
            }
        }
    }

    override fun setCustomProvider(url: String) {
        // validate the url, maybe throw some "known expectations" that we can handle in the middleware
        engine.settings.dohProviderUrl = url
    }

    override fun setExceptions(exceptions: List<String>) {
        engine.settings.dohExceptionsList = exceptions
    }

    companion object {
        @VisibleForTesting
        val cloudflareUri = "mozilla.cloudflare-dns.com"

        @VisibleForTesting
        val nextDnsUri = "firefox.dns.nextdns.io"
    }
}
