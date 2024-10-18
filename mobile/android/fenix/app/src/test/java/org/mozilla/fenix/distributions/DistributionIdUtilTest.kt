package org.mozilla.fenix.distributions

import io.mockk.every
import io.mockk.mockkObject
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.Config
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.robolectric.shadows.ShadowBuild

@RunWith(FenixRobolectricTestRunner::class)
class DistributionIdUtilTest {

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = getDistributionId { true }

        assertEquals("vivo-001", distributionId)
    }

    @Test
    fun `WHEN a device is not made by vivo AND the vivo distribution file is found THEN the proper id is returned`() {
        val distributionId = getDistributionId { true }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN a device is made by vivo AND the vivo distribution file is not found THEN the proper id is returned`() {
        // Mock Build.MANUFACTURER to simulate a Vivo device
        ShadowBuild.setManufacturer("vivo")

        val distributionId = getDistributionId { false }

        assertEquals("Mozilla", distributionId)
    }

    @Test
    fun `WHEN the config channel is mozilla online THEN the proper id is returned`() {
        mockkObject(Config)
        every { Config.channel.isMozillaOnline } returns true

        val distributionId = getDistributionId()

        assertEquals("MozillaOnline", distributionId)
    }

    @Test
    fun `WHEN the device is not vivo AND the channel is not mozilla online THEN the proper id is returned`() {
        val distributionId = getDistributionId()

        assertEquals("Mozilla", distributionId)
    }
}
