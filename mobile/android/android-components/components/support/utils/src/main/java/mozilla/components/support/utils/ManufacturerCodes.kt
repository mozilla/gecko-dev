/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils

import android.os.Build
import androidx.annotation.VisibleForTesting

/**
 * Used to check if a device is from a specific manufacturer,
 * using the value returned by [android.os.Build.MANUFACTURER].
 */
internal object ManufacturerCodes {
    // Manufacturer codes taken from https://developers.google.com/zero-touch/resources/manufacturer-names
    private const val HUAWEI: String = "Huawei"
    private const val SAMSUNG = "Samsung"
    private const val XIAOMI = "Xiaomi"
    private const val ONE_PLUS = "OnePlus"
    private const val LG = "LGE"
    private const val OPPO = "OPPO"

    @VisibleForTesting
    internal var manufacturer = Build.MANUFACTURER // is a var for testing purposes

    internal val isHuawei get() = manufacturer.equals(HUAWEI, ignoreCase = true)
    internal val isSamsung get() = manufacturer.equals(SAMSUNG, ignoreCase = true)
    internal val isXiaomi get() = manufacturer.equals(XIAOMI, ignoreCase = true)
    internal val isOnePlus get() = manufacturer.equals(ONE_PLUS, ignoreCase = true)
    internal val isLG get() = manufacturer.equals(LG, ignoreCase = true)
    internal val isOppo get() = manufacturer.equals(OPPO, ignoreCase = true)
}

/**
 * Interface for checking the device's manufacturer.
 */
interface ManufacturerChecker {
    /**
     * Returns true if the device is manufactured by Huawei.
     */
    fun isHuawei(): Boolean

    /**
     * Returns true if the device is manufactured by Samsung.
     */
    fun isSamsung(): Boolean

    /**
     * Returns true if the device is a OnePlus device.
     */
    fun isOnePlus(): Boolean

    /**
     * Returns true if the device is manufactured by Xiaomi.
     */
    fun isXiaomi(): Boolean

    /**
     * Returns true if the device is manufactured by LG.
     */
    fun isLG(): Boolean

    /**
     * Returns true if the device is manufactured by OPPO.
     */
    fun isOppo(): Boolean
}

/**
 * A concrete implementation of [ManufacturerChecker] that delegates manufacturer checks
 * to the [ManufacturerCodes] object, which in turn uses [android.os.Build.MANUFACTURER].
 */
class BuildManufacturerChecker : ManufacturerChecker {
    override fun isHuawei(): Boolean = ManufacturerCodes.isHuawei
    override fun isSamsung(): Boolean = ManufacturerCodes.isSamsung
    override fun isOnePlus(): Boolean = ManufacturerCodes.isOnePlus
    override fun isXiaomi(): Boolean = ManufacturerCodes.isXiaomi
    override fun isLG(): Boolean = ManufacturerCodes.isLG
    override fun isOppo(): Boolean = ManufacturerCodes.isOppo
}
