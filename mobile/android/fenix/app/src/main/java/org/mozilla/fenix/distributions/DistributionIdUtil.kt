/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import android.os.Build
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.Partnerships
import java.io.File
import java.util.Locale

private val logger = Logger("DistributionIdUtil")

/**
 * This file will be present on vivo devices that have Firefox preinstalled.
 * Note: The file is added by the manufacturer.
 */
private const val VIVO_PREINSTALLED_FIREFOX_FILE_PATH = "/data/yzfswj/another/vivo_firefox.txt"

private const val VIVO_MANUFACTURER = "vivo"

/**
 * @param appPreinstalledOnVivoDevice checks if the vivo preinstalled file exists.
 *
 * @return the distribution ID if one exists.
 */
fun getDistributionId(appPreinstalledOnVivoDevice: () -> Boolean = { wasAppPreinstalledOnVivoDevice() }): String {
    return when {
        isDeviceVivo() && appPreinstalledOnVivoDevice() -> Distribution.VIVO_001.id
        Config.channel.isMozillaOnline -> Distribution.MOZILLA_ONLINE.id
        else -> Distribution.DEFAULT.id
    }
}

private fun isDeviceVivo(): Boolean {
    return Build.MANUFACTURER.lowercase(Locale.getDefault()).contains(VIVO_MANUFACTURER)
}

private fun wasAppPreinstalledOnVivoDevice(): Boolean {
    return try {
        File(VIVO_PREINSTALLED_FIREFOX_FILE_PATH).exists()
    } catch (e: SecurityException) {
        logger.error("File access denied", e)
        Partnerships.vivoFileCheckError.record()
        false
    }
}

/**
 * This enum represents distribution IDs that are used in glean metrics.
 */
enum class Distribution(val id: String) {
    DEFAULT(id = "Mozilla"),
    MOZILLA_ONLINE(id = "MozillaOnline"),
    VIVO_001(id = "vivo-001"),
}
