/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import mozilla.components.support.base.log.logger.Logger
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.Partnerships
import java.io.File
import java.util.Locale

/**
 * This file will be present on vivo devices that have Firefox preinstalled.
 * Note: The file is added by the manufacturer.
 */
private const val VIVO_PREINSTALLED_FIREFOX_FILE_PATH = "/data/yzfswj/another/vivo_firefox.txt"
private const val VIVO_MANUFACTURER = "vivo"

private const val DT_PROVIDER = "digital_turbine"
private const val DT_TELEFONICA_PACKAGE = "com.dti.telefonica"

/**
 * Class used to manage distribution Ids for distribution deals with third parties
 *
 * @param context application context
 * @param browserStoreProvider used to update and fetch the stored distribution Id
 * @param distributionProviderChecker used for checking content providers for a distribution provider
 */
class DistributionIdManager(
    private val context: Context,
    private val browserStoreProvider: DistributionBrowserStoreProvider,
    private val distributionProviderChecker: DistributionProviderChecker,
) {
    private val logger = Logger(DistributionIdManager::class.simpleName)

    /**
     * Gets the distribution Id that is used to specify which distribution deal this install
     * is associated with.
     *
     * @param appPreinstalledOnVivoDevice checks if the vivo preinstalled file exists.
     * @param isDtTelefonicaInstalled checks if the DT telefonica app is installed on the device
     *
     * @return the distribution ID if one exists.
     */
    fun getDistributionId(
        appPreinstalledOnVivoDevice: () -> Boolean = { wasAppPreinstalledOnVivoDevice() },
        isDtTelefonicaInstalled: () -> Boolean = { isDtTelefonicaInstalled(context) },
    ): String {
        browserStoreProvider.getDistributionId()?.let { return it }

        val provider = distributionProviderChecker.queryProvider()

        val distributionId = when {
            isProviderDigitalTurbine(provider) && isDtTelefonicaInstalled() -> Distribution.DT_001.id
            isDeviceVivo() && appPreinstalledOnVivoDevice() -> Distribution.VIVO_001.id
            Config.channel.isMozillaOnline -> Distribution.MOZILLA_ONLINE.id
            else -> Distribution.DEFAULT.id
        }

        browserStoreProvider.updateDistributionId(distributionId)

        return distributionId
    }

    private fun isDeviceVivo(): Boolean {
        return Build.MANUFACTURER?.lowercase(Locale.getDefault())?.contains(VIVO_MANUFACTURER) ?: false
    }

    private fun isProviderDigitalTurbine(provider: String?): Boolean = provider == DT_PROVIDER

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
     * Checks if the Digital Turbine Telefonica app exists on the device
     */
    private fun isDtTelefonicaInstalled(context: Context): Boolean {
        val packages = context.packageManager.getInstalledPackages(PackageManager.GET_META_DATA)
        return packages.any { it.packageName == DT_TELEFONICA_PACKAGE }
    }

    /**
     * This enum represents distribution IDs that are used in glean metrics.
     */
    private enum class Distribution(val id: String) {
        DEFAULT(id = "Mozilla"),
        MOZILLA_ONLINE(id = "MozillaOnline"),
        VIVO_001(id = "vivo-001"),
        DT_001(id = "dt-001"),
    }
}
