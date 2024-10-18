package org.mozilla.fenix.distributions

import io.mockk.clearMocks
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.unmockkObject
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.Config
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.shadows.ShadowBuild

@RunWith(FenixRobolectricTestRunner::class)
class DistributionIdUtilTest {

    private val browserStoreMock: BrowserStore = mockk(relaxed = true)
    private val browserStateMock: BrowserState = mockk(relaxed = true)

    @Before
    fun setup() {
        every { browserStoreMock.state } returns browserStateMock
        every { browserStateMock.distributionId } returns null
    }

    @After
    fun tearDown() {
        clearMocks(browserStoreMock, browserStateMock)
        unmockkObject(Config)
        ShadowBuild.reset()
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = getDistributionId(browserStoreMock) { true }

        assertEquals("vivo-001", distributionId)
    }

    @Test
    fun `WHEN a device is not made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        val distributionId = getDistributionId(browserStoreMock) { true }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is not found THEN the proper id is returned`() {
        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = getDistributionId(browserStoreMock) { false }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the config channel is mozilla online THEN the proper id is returned`() {
        mockkObject(Config)
        every { Config.channel.isMozillaOnline } returns true

        val distributionId = getDistributionId(browserStoreMock)

        assertEquals("MozillaOnline", distributionId)
    }

    @Test
    fun `WHEN the device is not vivo AND the channel is not mozilla online THEN the proper id is returned`() {
        val distributionId = getDistributionId(browserStoreMock)

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the browser stores state already has a distribution Id assigned THEN that ID gets returned`() {
        every { browserStateMock.distributionId } returns "testId"

        val distributionId = getDistributionId(browserStoreMock)

        assertEquals("testId", distributionId)
    }
}
