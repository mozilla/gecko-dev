/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.shadows.ShadowBuild

@RunWith(RobolectricTestRunner::class)
class DistributionIdManagerTest {

    private var providerValue: String? = null
    private var legacyProviderValue: String? = null
    private var storedId: String? = null

    private val testDistributionProviderChecker = object : DistributionProviderChecker {
        override fun queryProvider(): String? = providerValue
    }

    private val testLegacyDistributionProviderChecker = object : DistributionProviderChecker {
        override fun queryProvider(): String? = legacyProviderValue
    }

    private val testBrowserStoreProvider = object : DistributionBrowserStoreProvider {
        override fun getDistributionId(): String? = storedId

        override fun updateDistributionId(id: String) {
            storedId = id
        }
    }

    private val testDistributionMetricsProvider = mockk<DistributionMetricsProvider>(relaxed = true)

    @After
    fun tearDown() {
        providerValue = null
        legacyProviderValue = null
        storedId = null
        ShadowBuild.reset()
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            appPreinstalledOnVivoDevice = { true },
        )

        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = subject.getDistributionId()

        assertEquals("vivo-001", distributionId)
    }

    @Test
    fun `WHEN a device is not made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            appPreinstalledOnVivoDevice = { true },
        )

        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is not found THEN the proper id is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            appPreinstalledOnVivoDevice = { false },
        )

        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the device is not vivo AND the channel is not mozilla online THEN the proper id is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the browser stores state already has a distribution Id assigned THEN that ID gets returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        storedId = "testId"

        val distributionId = subject.getDistributionId()

        assertEquals("testId", distributionId)
    }

    @Test
    fun `WHEN the provider is digital_tubrine AND the DT app is installed THEN the proper ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        providerValue = "digital_turbine"
        val distributionId = subject.getDistributionId()

        assertEquals("dt-001", distributionId)
    }

    @Test
    fun `WHEN the provider is not digital_tubrine AND the DT app is installed THEN the proper ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        providerValue = "some_provider"
        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the provider is not digital_tubrine AND the DT app is not installed THEN the proper ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { false },
        )

        providerValue = "some_provider"
        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the provider is null AND the DT app is installed THEN the proper ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        providerValue = null
        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the provider is null AND the DT app is not installed THEN the proper ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { false },
        )

        providerValue = null
        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the distribution is not default or mozilla online THEN the distribution is from a deal`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        testBrowserStoreProvider.updateDistributionId(DistributionIdManager.Distribution.VIVO_001.id)
        assertEquals(true, subject.isPartnershipDistribution())

        testBrowserStoreProvider.updateDistributionId(DistributionIdManager.Distribution.DT_001.id)
        assertEquals(true, subject.isPartnershipDistribution())

        testBrowserStoreProvider.updateDistributionId(DistributionIdManager.Distribution.DT_002.id)
        assertEquals(true, subject.isPartnershipDistribution())

        testBrowserStoreProvider.updateDistributionId(DistributionIdManager.Distribution.AURA_001.id)
        assertEquals(true, subject.isPartnershipDistribution())
    }

    @Test
    fun `WHEN the provider is aura THEN the proper distribution ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        providerValue = "aura"
        val distributionId = subject.getDistributionId()

        assertEquals("aura-001", distributionId)
    }

    @Test
    fun `WHEN the provider is DT AND a DT USA package is installed THEN the proper distribution ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtUsaInstalled = { true },
        )

        providerValue = "digital_turbine"
        val distributionId = subject.getDistributionId()

        assertEquals("dt-002", distributionId)
    }

    @Test
    fun `WHEN the provider is not DT AND a DT USA package is installed THEN the proper distribution ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtUsaInstalled = { true },
        )

        providerValue = "some_provider"
        val distributionId = subject.getDistributionId()

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the provider is DT and telefonica and USA packages are not installed THEN the proper distribution ID is returned`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        providerValue = "digital_turbine"
        val distributionId = subject.getDistributionId()

        assertEquals("dt-003", distributionId)
    }

    @Test
    fun `WHEN the new default provider fails to detect DT telefonica THEN the legacy provider detects it`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        legacyProviderValue = "digital_turbine"
        val distributionId = subject.getDistributionId()

        assertEquals("dt-001", distributionId)
    }

    @Test
    fun `WHEN the new default provider fails to detect DT USA THEN the legacy provider detects it`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtUsaInstalled = { true },
        )

        legacyProviderValue = "digital_turbine"
        val distributionId = subject.getDistributionId()

        assertEquals("dt-002", distributionId)
    }

    @Test
    fun `WHEN the new default provider fails to detect DT ROW THEN the legacy provider detects it`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        legacyProviderValue = "digital_turbine"
        val distributionId = subject.getDistributionId()

        assertEquals("dt-003", distributionId)
    }

    @Test
    fun `WHEN DT telefonica is installed AND provider is DT and legacy provider is DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = true,
            isLegacyProviderDigitalTurbine = true,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt001Detected() }
    }

    @Test
    fun `WHEN DT telefonica is installed AND provider is not DT and legacy provider is DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = false,
            isLegacyProviderDigitalTurbine = true,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt001LegacyDetected() }
    }

    @Test
    fun `WHEN DT telefonica is installed AND provider is DT and legacy provider is not DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = true,
            isLegacyProviderDigitalTurbine = false,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt001Detected() }
    }

    @Test
    fun `WHEN DT telefonica is installed AND provider is not DT and legacy provider is not DT THEN the metrics are not sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = false,
            isLegacyProviderDigitalTurbine = false,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 0) { testDistributionMetricsProvider.recordDt001Detected() }
        verify(exactly = 0) { testDistributionMetricsProvider.recordDt001LegacyDetected() }
    }

    @Test
    fun `WHEN DT USA is installed AND provider is DT and legacy provider is DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtUsaInstalled = { true },
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = true,
            isLegacyProviderDigitalTurbine = true,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt002Detected() }
    }

    @Test
    fun `WHEN DT USA is installed AND provider is not DT and legacy provider is DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtUsaInstalled = { true },
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = false,
            isLegacyProviderDigitalTurbine = true,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt002LegacyDetected() }
    }

    @Test
    fun `WHEN DT USA is installed AND provider is DT and legacy provider is not DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtUsaInstalled = { true },
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = true,
            isLegacyProviderDigitalTurbine = false,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt002Detected() }
    }

    @Test
    fun `WHEN DT USA is installed AND provider is not DT and legacy provider is not DT THEN the metrics are not sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
            isDtTelefonicaInstalled = { true },
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = false,
            isLegacyProviderDigitalTurbine = false,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 0) { testDistributionMetricsProvider.recordDt002Detected() }
        verify(exactly = 0) { testDistributionMetricsProvider.recordDt002LegacyDetected() }
    }

    @Test
    fun `WHEN DT ROW is installed AND provider is DT and legacy provider is DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = true,
            isLegacyProviderDigitalTurbine = true,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt003Detected() }
    }

    @Test
    fun `WHEN DT ROW is installed AND provider is not DT and legacy provider is DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = false,
            isLegacyProviderDigitalTurbine = true,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt003LegacyDetected() }
    }

    @Test
    fun `WHEN DT ROW is installed AND provider is DT and legacy provider is not DT THEN the proper metrics are sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = true,
            isLegacyProviderDigitalTurbine = false,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 1) { testDistributionMetricsProvider.recordDt003Detected() }
    }

    @Test
    fun `WHEN DT ROW is installed AND provider is not DT and legacy provider is not DT THEN the metrics are not sent`() {
        val subject = DistributionIdManager(
            testContext,
            testBrowserStoreProvider,
            distributionProviderChecker = testDistributionProviderChecker,
            legacyDistributionProviderChecker = testLegacyDistributionProviderChecker,
        )

        subject.recordProviderCheckerEvents(
            isProviderDigitalTurbine = false,
            isLegacyProviderDigitalTurbine = false,
            distributionMetricsProvider = testDistributionMetricsProvider,
        )

        verify(exactly = 0) { testDistributionMetricsProvider.recordDt003Detected() }
        verify(exactly = 0) { testDistributionMetricsProvider.recordDt003LegacyDetected() }
    }
}
