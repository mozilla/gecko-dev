/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import io.mockk.every
import io.mockk.mockkObject
import io.mockk.unmockkObject
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.Config
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.shadows.ShadowBuild

@RunWith(FenixRobolectricTestRunner::class)
class DistributionIdManagerTest {

    private var providerValue: String? = null
    private var storedId: String? = null

    private val testDistributionProviderChecker = object : DistributionProviderChecker {
        override fun queryProvider(): String? = providerValue
    }

    private val testBrowserStoreProvider = object : DistributionBrowserStoreProvider {
        override fun getDistributionId(): String? = storedId

        override fun updateDistributionId(id: String) {
            storedId = id
        }
    }

    private val subject = DistributionIdManager(
        testContext,
        testBrowserStoreProvider,
        testDistributionProviderChecker,
    )

    @After
    fun tearDown() {
        providerValue = null
        storedId = null
        unmockkObject(Config)
        ShadowBuild.reset()
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = subject.getDistributionId(
            appPreinstalledOnVivoDevice = { true },
        )

        assertEquals("vivo-001", distributionId)
    }

    @Test
    fun `WHEN a device is not made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        val distributionId = subject.getDistributionId(
            appPreinstalledOnVivoDevice = { true },
        )

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is not found THEN the proper id is returned`() {
        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = subject.getDistributionId(
            appPreinstalledOnVivoDevice = { false },
        )

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the config channel is mozilla online THEN the proper id is returned`() {
        mockkObject(Config)
        every { Config.channel.isMozillaOnline } returns true

        val distributionId = subject.getDistributionId()

        assertEquals("MozillaOnline", distributionId)
    }

    @Test
    fun `WHEN the device is not vivo AND the channel is not mozilla online THEN the proper id is returned`() {
        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the browser stores state already has a distribution Id assigned THEN that ID gets returned`() {
        storedId = "testId"

        val distributionId = subject.getDistributionId()

        assertEquals("testId", distributionId)
    }

    @Test
    fun `WHEN the provider is digital_tubrine AND the DT app is installed THEN the proper ID is returned`() {
        providerValue = "digital_turbine"
        val distributionId = subject.getDistributionId(
            isDtTelefonicaInstalled = { true },
        )

        assertEquals("dt-001", distributionId)
    }

    @Test
    fun `WHEN the provider is not digital_tubrine AND the DT app is installed THEN the proper ID is returned`() {
        providerValue = "some_provider"
        val distributionId = subject.getDistributionId(
            isDtTelefonicaInstalled = { true },
        )

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the provider is digital_tubrine AND the DT app is not installed THEN the proper ID is returned`() {
        providerValue = "digital_turbine"
        val distributionId = subject.getDistributionId(
            isDtTelefonicaInstalled = { false },
        )

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the provider is not digital_tubrine AND the DT app is not installed THEN the proper ID is returned`() {
        providerValue = "some_provider"
        val distributionId = subject.getDistributionId(
            isDtTelefonicaInstalled = { false },
        )

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the provider is null AND the DT app is installed THEN the proper ID is returned`() {
        providerValue = null
        val distributionId = subject.getDistributionId(
            isDtTelefonicaInstalled = { true },
        )

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the provider is null AND the DT app is not installed THEN the proper ID is returned`() {
        providerValue = null
        val distributionId = subject.getDistributionId(
            isDtTelefonicaInstalled = { false },
        )

        assertEquals("Mozilla", distributionId)
    }
}
