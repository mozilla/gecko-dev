/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import mozilla.components.support.utils.ManufacturerCodes.manufacturer
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ManufacturerCodesTest {

    private val buildManufacturerChecker = BuildManufacturerChecker()

    @Test
    fun testIsHuawei() {
        manufacturer = "HuaWei" // expected value for Huawei devices
        assertTrue(ManufacturerCodes.isHuawei)
        assertTrue(buildManufacturerChecker.isHuawei())

        assertFalse(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isXiaomi)

        assertFalse(ManufacturerCodes.isOnePlus)

        assertFalse(ManufacturerCodes.isLG)

        assertFalse(ManufacturerCodes.isOppo)
    }

    @Test
    fun testIsSamsung() {
        manufacturer = "SAmsung" // expected value for Samsung devices

        assertFalse(ManufacturerCodes.isHuawei)

        assertTrue(ManufacturerCodes.isSamsung)
        assertTrue(buildManufacturerChecker.isSamsung())

        assertFalse(ManufacturerCodes.isXiaomi)

        assertFalse(ManufacturerCodes.isOnePlus)

        assertFalse(ManufacturerCodes.isLG)

        assertFalse(ManufacturerCodes.isOppo)
    }

    @Test
    fun testIsXiaomi() {
        manufacturer = "xiaomI" // expected value for Xiaomi devices

        assertFalse(ManufacturerCodes.isHuawei)

        assertFalse(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isOnePlus)

        assertTrue(ManufacturerCodes.isXiaomi)
        assertTrue(buildManufacturerChecker.isXiaomi())

        assertFalse(ManufacturerCodes.isLG)

        assertFalse(ManufacturerCodes.isOppo)
    }

    @Test
    fun testIsOnePlus() {
        manufacturer = "OnePlUs" // expected value for OnePlus devices

        assertFalse(ManufacturerCodes.isHuawei)

        assertFalse(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isXiaomi)

        assertTrue(ManufacturerCodes.isOnePlus)
        assertTrue(buildManufacturerChecker.isOnePlus())

        assertFalse(ManufacturerCodes.isLG)

        assertFalse(ManufacturerCodes.isOppo)
    }

    @Test
    fun testIsLG() {
        manufacturer = "LGE" // expected value for LG devices

        assertFalse(ManufacturerCodes.isHuawei)

        assertFalse(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isXiaomi)

        assertFalse(ManufacturerCodes.isOnePlus)

        assertTrue(ManufacturerCodes.isLG)
        assertTrue(buildManufacturerChecker.isLG())

        assertFalse(ManufacturerCodes.isOppo)
    }

    @Test
    fun testIsOppo() {
        manufacturer = "OPPO" // expected value for Oppo devices

        assertFalse(ManufacturerCodes.isHuawei)

        assertFalse(ManufacturerCodes.isSamsung)

        assertFalse(ManufacturerCodes.isXiaomi)

        assertFalse(ManufacturerCodes.isOnePlus)

        assertFalse(ManufacturerCodes.isLG)

        assertTrue(ManufacturerCodes.isOppo)
        assertTrue(buildManufacturerChecker.isOppo())
    }
}
