/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.database.Cursor
import android.os.Build
import androidx.annotation.VisibleForTesting
import androidx.core.net.toUri
import mozilla.components.browser.state.action.UpdateDistribution
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.Partnerships
import java.io.File
import java.util.Locale

private val logger = Logger("DistributionIdUtil")

private const val FIREFOX_PACKAGE_NAME = "org.mozilla.firefox"
private const val FIREFOX_BETA_PACKAGE_NAME = "org.mozilla.firefox_beta"
private const val FIREFOX_NIGHTLY_PACKAGE_NAME = "org.mozilla.fenix"

/**
 * This file will be present on vivo devices that have Firefox preinstalled.
 * Note: The file is added by the manufacturer.
 */
private const val VIVO_PREINSTALLED_FIREFOX_FILE_PATH = "/data/yzfswj/another/vivo_firefox.txt"

private const val VIVO_MANUFACTURER = "vivo"

private const val DT_PROVIDER = "digital_turbine"
private const val DT_TELEFONICA_PACKAGE = "com.dti.telefonica"
private const val ADJUST_CONTENT_PROVIDER_INTENT_ACTION = "com.attribution.REFERRAL_PROVIDER"

private const val PACKAGE_NAME_COLUMN = "package_name"
private const val ENCRYPTED_DATA_COLUMN = "encrypted_data"

/**
 * @param context App context
 * @param browserStore the browser store
 * @param appPreinstalledOnVivoDevice checks if the vivo preinstalled file exists.
 *
 * @return the distribution ID if one exists.
 */
fun getDistributionId(
    context: Context,
    browserStore: BrowserStore,
    appPreinstalledOnVivoDevice: () -> Boolean = { wasAppPreinstalledOnVivoDevice() },
): String {
    browserStore.state.distributionId?.let { return it }

    val provider = queryProvider(context)

    val distributionId = when {
        isProviderDigitalTurbine(provider) && isDtTelefonicaInstalled(context) -> Distribution.DT_001.id
        isDeviceVivo() && appPreinstalledOnVivoDevice() -> Distribution.VIVO_001.id
        Config.channel.isMozillaOnline -> Distribution.MOZILLA_ONLINE.id
        else -> Distribution.DEFAULT.id
    }

    browserStore.dispatch(UpdateDistribution(distributionId))

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
 * Try to get a provider from a content resolver meant for adjust.
 */
@VisibleForTesting
internal fun queryProvider(context: Context): String? {
    val adjustProviderIntent = Intent(ADJUST_CONTENT_PROVIDER_INTENT_ACTION)
    val contentProviders = context.packageManager.queryIntentContentProviders(adjustProviderIntent, 0)
    val contentResolver = context.contentResolver

    for (resolveInfo in contentProviders) {
        val authority = resolveInfo.providerInfo.authority
        val uri = "content://$authority/trackers".toUri()

        val projection = arrayOf(PACKAGE_NAME_COLUMN, ENCRYPTED_DATA_COLUMN)

        val contentResolverCursor = contentResolver.query(
            uri,
            projection,
            null,
            null,
            null,
        )

        contentResolverCursor?.use { cursor ->
            cursor.getProvider()?.let { return it }
        }
    }

    return null
}

private fun Cursor.getProvider(): String? {
    while (moveToNext()) {
        val packageNameColumnIndex = getColumnIndex(PACKAGE_NAME_COLUMN)
        val dataColumnIndex = getColumnIndex(ENCRYPTED_DATA_COLUMN)

        // Check if columns exist
        if (packageNameColumnIndex == -1 || dataColumnIndex == -1) {
            break
        }

        val packageName = getString(packageNameColumnIndex) ?: break

        if (packageName == FIREFOX_PACKAGE_NAME ||
            packageName == FIREFOX_BETA_PACKAGE_NAME ||
            packageName == FIREFOX_NIGHTLY_PACKAGE_NAME
        ) {
            val data = getString(dataColumnIndex) ?: break
            try {
                val jsonObject = JSONObject(data)
                val provider = jsonObject.getString("provider")
                return provider
            } catch (e: JSONException) {
                break
            }
        }
    }
    return null
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
enum class Distribution(val id: String) {
    DEFAULT(id = "Mozilla"),
    MOZILLA_ONLINE(id = "MozillaOnline"),
    VIVO_001(id = "vivo-001"),
    DT_001(id = "dt-001"),
}
