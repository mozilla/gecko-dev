/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.engine.DefaultSettings
import mozilla.components.concept.engine.Engine
import mozilla.components.support.test.fakes.engine.FakeEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.utils.Settings

class DefaultDohSettingsProviderTest {
    // Non-default values for testing
    private val settings =
        DefaultSettings(
            dohSettingsMode = Engine.DohSettingsMode.INCREASED,
            dohProviderUrl = DefaultDohSettingsProvider.nextDnsUri,
            dohDefaultProviderUrl = DefaultDohSettingsProvider.cloudflareUri,
            dohExceptionsList = listOf("example.com", "example2.com", "example3.com"),
        )
    private val fakeEngine = FakeEngine(expectedSettings = settings)
    private val appSettings: Settings = mockk(relaxed = true)
    private lateinit var settingsProvider: DefaultDohSettingsProvider

    @Before
    fun setUp() {
        settingsProvider = DefaultDohSettingsProvider(
            engine = fakeEngine,
            settings = appSettings,
        )
        every { appSettings.getDohSettingsMode() } returns Engine.DohSettingsMode.INCREASED
        every { appSettings.dohProviderUrl } returns DefaultDohSettingsProvider.nextDnsUri
        every { appSettings.dohDefaultProviderUrl } returns DefaultDohSettingsProvider.cloudflareUri
        every { appSettings.dohExceptionsList } returns setOf(
            "example.com",
            "example2.com",
            "example3.com",
        )
    }

    @Test
    fun `WHEN getDefaultProviders is called, the engine default provider is marked as default`() {
        // When getDefaultProviders is called
        val providers = settingsProvider.getDefaultProviders()

        // Verify that the default is what is available in the settings
        val defaultProvider = providers.filterIsInstance<Provider.BuiltIn>().find { it.default }
        assertEquals(settings.dohDefaultProviderUrl, defaultProvider?.url)
    }

    @Test
    fun `WHEN getSelectedProtectionLevel is called, the correct selected protection level is returned`() {
        // When getSelectedProtectionLevel is called
        val selectedProtectionLevel = settingsProvider.getSelectedProtectionLevel()

        // Verify that the selected protection level is what is available in the settings
        assertEquals(ProtectionLevel.Increased, selectedProtectionLevel)
    }

    @Test
    fun `WHEN getSelectedProvider is called, the correct selected provider is returned`() {
        // When getSelectedProvider is called
        val selectedProvider = settingsProvider.getSelectedProvider()

        // Verify that the selected provider is what is available in the settings
        assertEquals(settings.dohProviderUrl, selectedProvider?.url)
    }

    @Test
    fun `WHEN getExceptions is called, the correct exceptionsList is returned`() {
        // When getExceptions is called
        val selectedExceptionsList = settingsProvider.getExceptions()

        // Verify that the selected exceptionsList is what is available in the settings
        assertEquals(settings.dohExceptionsList, selectedExceptionsList)
    }

    @Test
    fun `WHEN protection level is set to Off, the app layer and engine's DoH settings mode is updated`() {
        // When we call setProtectionLevel to Off
        settingsProvider.setProtectionLevel(protectionLevel = ProtectionLevel.Off, provider = null)

        // Then verify that the DoH settings mode is Off in the app layer
        verify { appSettings.setDohSettingsMode(ProtectionLevel.Off.toDohSettingsMode()) }

        // Then verify that the DoH settings mode is Off in the engine
        assertTrue(
            "Expected DoH settings mode to be Off",
            fakeEngine.settings.dohSettingsMode == Engine.DohSettingsMode.OFF,
        )
    }

    @Test
    fun `WHEN protection level is set to Default, the app layer and engine's DoH settings mode is updated`() {
        // When we call setProtectionLevel to Default
        settingsProvider.setProtectionLevel(
            protectionLevel = ProtectionLevel.Default,
            provider = null,
        )

        // Then verify that the DoH settings mode is Off in the app layer
        verify { appSettings.setDohSettingsMode(ProtectionLevel.Default.toDohSettingsMode()) }

        // Then verify that the DoH settings mode is Default in the engine
        assertTrue(
            "Expected DoH settings mode to be Default",
            fakeEngine.settings.dohSettingsMode == Engine.DohSettingsMode.DEFAULT,
        )
    }

    @Test
    fun `WHEN protection level is set to Increased, the app layer and engine's DoH settings mode is set to increased with the supplied provider`() {
        // When we call setProtectionLevel to Increased
        val dohProvider = Provider.Custom(url = "https://foo.bar")
        settingsProvider.setProtectionLevel(
            protectionLevel = ProtectionLevel.Increased,
            provider = dohProvider,
        )

        // Then verify that the DoH settings are updated accordingly in the app layer
        verify { appSettings.setDohSettingsMode(ProtectionLevel.Increased.toDohSettingsMode()) }
        verify { appSettings.dohProviderUrl = dohProvider.url }

        // Then verify that the DoH setting are updated accordingly in the engine
        assertTrue(
            "Expected DoH settings mode to be Increased",
            fakeEngine.settings.dohSettingsMode == Engine.DohSettingsMode.INCREASED,
        )
        assertEquals(
            "Expected DoH settings provider url to be ${dohProvider.url}",
            dohProvider.url,
            fakeEngine.settings.dohProviderUrl,
        )
    }

    @Test
    fun `WHEN protection level is set to Max, the app layer and engine's DoH settings mode is set to increased with the supplied provider`() {
        // When we call setProtectionLevel to Max
        val dohProvider = Provider.Custom(url = "https://foo.bar")
        settingsProvider.setProtectionLevel(
            protectionLevel = ProtectionLevel.Max,
            provider = dohProvider,
        )

        // Then verify that the DoH settings are updated accordingly in the app layer
        verify { appSettings.setDohSettingsMode(ProtectionLevel.Max.toDohSettingsMode()) }
        verify { appSettings.dohProviderUrl = dohProvider.url }

        // Then verify that the DoH setting are updated accordingly in the engine
        assertTrue(
            "Expected DoH settings mode to be Max",
            fakeEngine.settings.dohSettingsMode == Engine.DohSettingsMode.MAX,
        )
        assertEquals(
            "Expected DoH settings provider url to be ${dohProvider.url}",
            dohProvider.url,
            fakeEngine.settings.dohProviderUrl,
        )
    }

    @Test
    fun `WHEN protection level is set to Increased without a provider, an exception is thrown`() {
        assertThrows(IllegalArgumentException::class.java) {
            // When we call setProtectionLevel to Increased without a provider
            // We expect an illegal argument exception
            settingsProvider.setProtectionLevel(
                protectionLevel = ProtectionLevel.Increased,
                provider = null,
            )
        }
    }

    @Test
    fun `WHEN protection level is set to Max without a provider, an exception is thrown`() {
        assertThrows(IllegalArgumentException::class.java) {
            // When we call setProtectionLevel to Max without a provider
            // We expect an illegal argument exception
            settingsProvider.setProtectionLevel(
                protectionLevel = ProtectionLevel.Max,
                provider = null,
            )
        }
    }

    @Test
    fun `WHEN setCustomProvider is called with a url, the app layer and engine's DoH provider url is also updated`() {
        // When setCustomProvider is called with a url
        val customUrl = "https://foo.bar"
        settingsProvider.setCustomProvider(
            url = customUrl,
        )

        // Then verify that the engine DoH provider url is also updated in the app layer
        verify { appSettings.dohProviderUrl = customUrl }

        // Then verify that the engine DoH provider url is also updated in the engine
        assertTrue(
            "Expected exceptions should match",
            customUrl == fakeEngine.settings.dohProviderUrl,
        )
    }

    @Test
    fun `WHEN exceptions are set, the app layer and engine's DoH settings exceptions are also updated`() {
        // When excepts are set
        val dohExceptions = listOf("foo.bar", "foo2.bar", "foo3.bar", "foo4.bar", "foo5.bar")
        settingsProvider.setExceptions(
            exceptions = dohExceptions,
        )

        // Then verify that the engine DoH settings exceptions are also updated in the app layer
        verify { appSettings.dohExceptionsList = dohExceptions.toSet() }

        // Then verify that the engine DoH settings exceptions are also updated in the engine
        every { appSettings.dohExceptionsList } returns dohExceptions.toSet()
        assertTrue(
            "Expected exceptions should match",
            dohExceptions == settingsProvider.getExceptions(),
        )
    }
}
