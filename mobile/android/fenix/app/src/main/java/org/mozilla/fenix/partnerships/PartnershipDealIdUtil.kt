/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.partnerships

import android.os.Build
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.GleanMetrics.Partnerships
import java.io.File
import java.util.Locale

/**
 * A util for fetching partnership deal Ids that are used in glean metrics.
 */
object PartnershipDealIdUtil {

    private val logger = Logger(PartnershipDealIdUtil.javaClass.simpleName)

    private const val VIVO_TRACK_FILE_PATH = "/data/yzfswj/another/vivo_firefox.txt"

    private const val VIVO_MANUFACTURER = "vivo"

    /**
     * @return the partnership deal ID if one exists.
     */
    suspend fun getPartnershipDealId(): String? {
        return when {
            isDeviceVivo() && doesVivoTrackingFileExist() -> PartnershipDeal.VIVO_CPA_2024.id
            else -> null
        }
    }

    private fun isDeviceVivo(): Boolean {
        return Build.MANUFACTURER.lowercase(Locale.getDefault()).contains(VIVO_MANUFACTURER)
    }

    private suspend fun doesVivoTrackingFileExist(): Boolean {
        return withContext(Dispatchers.IO) {
            val trackingFile = File(VIVO_TRACK_FILE_PATH)
            try {
                trackingFile.exists()
            } catch (e: SecurityException) {
                logger.error("File access denied", e)
                Partnerships.vivoFileCheckError.record()
                false
            }
        }
    }
}
