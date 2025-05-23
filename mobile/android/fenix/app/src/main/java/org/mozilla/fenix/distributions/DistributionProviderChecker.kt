/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import android.content.Context
import android.content.Intent
import android.database.Cursor
import androidx.core.net.toUri
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONException
import org.json.JSONObject

private const val FIREFOX_PACKAGE_NAME = "org.mozilla.firefox"
private const val FIREFOX_BETA_PACKAGE_NAME = "org.mozilla.firefox_beta"
private const val FIREFOX_NIGHTLY_PACKAGE_NAME = "org.mozilla.fenix"

private const val ADJUST_CONTENT_PROVIDER_INTENT_ACTION = "com.attribution.REFERRAL_PROVIDER"

private const val PACKAGE_NAME_COLUMN = "package_name"
private const val ENCRYPTED_DATA_COLUMN = "encrypted_data"

/**
 * A tool for trying to get a provider from a content resolver meant for adjust.
 */
interface DistributionProviderChecker {
    /**
     * Looks for the provider value
     */
    fun queryProvider(): String?
}

private val logger = Logger(DistributionProviderChecker::class.simpleName)

/**
 * Default implementation for DistributionProviderChecker
 *
 * @param context application context used to get the packageManager and contentResolver
 */
class DefaultDistributionProviderChecker(private val context: Context) : DistributionProviderChecker {

    override fun queryProvider(): String? {
        logger.info("1 - Starting check...")
        val adjustProviderIntent = Intent(ADJUST_CONTENT_PROVIDER_INTENT_ACTION)
        val contentProviders = context.packageManager.queryIntentContentProviders(adjustProviderIntent, 0)
        val contentResolver = context.contentResolver

        for (resolveInfo in contentProviders) {
            val authority = resolveInfo.providerInfo.authority
            val uri = "content://$authority/trackers".toUri()

            if (!authority.contains("google")) {
                logger.info("1 - Authority found: $authority")
            }

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
        logger.info("1 - Cursor available")
        while (moveToNext()) {
            logger.info("1 - Checking next cursor...")
            val packageNameColumnIndex = getColumnIndex(PACKAGE_NAME_COLUMN)
            val dataColumnIndex = getColumnIndex(ENCRYPTED_DATA_COLUMN)

            // Check if columns exist
            if (packageNameColumnIndex == -1 || dataColumnIndex == -1) {
                logger.info("1 - missing columns, packageName: $packageNameColumnIndex; data: $dataColumnIndex")
                break
            }

            logger.info("1 - packageName: ${getString(packageNameColumnIndex)}")

            val packageName = getString(packageNameColumnIndex) ?: break

            if (packageName == FIREFOX_PACKAGE_NAME ||
                packageName == FIREFOX_BETA_PACKAGE_NAME ||
                packageName == FIREFOX_NIGHTLY_PACKAGE_NAME
            ) {
                logger.info("1 - data: ${getString(dataColumnIndex)}")
                val data = getString(dataColumnIndex) ?: break
                try {
                    val jsonObject = JSONObject(data)
                    val provider = jsonObject.getString("provider")
                    logger.info("1 - provider found: $provider")
                    return provider
                } catch (e: JSONException) {
                    logger.info("1 - JSON expection: $e")
                    break
                }
            }
        }
        return null
    }
}

/**
 * Second implementation for DistributionProviderChecker
 *
 * @param context application context used to get the packageManager and contentResolver
 */
class SecondaryDistributionProviderChecker(private val context: Context) : DistributionProviderChecker {
    override fun queryProvider(): String? {
        logger.info("2 - Starting check...")
        val adjustProviderIntent = Intent(ADJUST_CONTENT_PROVIDER_INTENT_ACTION)
        val contentProviders = context.packageManager.queryIntentContentProviders(adjustProviderIntent, 0)
        val contentResolver = context.contentResolver

        for (resolveInfo in contentProviders) {
            val authority = resolveInfo.providerInfo.authority
            val uri = "content://$authority/trackers".toUri()

            val projection = arrayOf(ENCRYPTED_DATA_COLUMN)

            val contentResolverCursor = contentResolver.query(
                uri,
                projection,
                "package_name=?",
                arrayOf(context.packageName),
                null,
            )

            contentResolverCursor?.use { cursor ->
                cursor.getProvider()?.let { return it }
            }
        }

        return null
    }

    private fun Cursor.getProvider(): String? {
        logger.info("2 - Cursor available")
        while (moveToNext()) {
            val dataColumnIndex = getColumnIndex(ENCRYPTED_DATA_COLUMN)

            // Check if columns exist
            if (dataColumnIndex == -1) {
                break
            }

            val data = getString(dataColumnIndex) ?: break
            try {
                val jsonObject = JSONObject(data)
                val provider = jsonObject.getString("provider")
                logger.info("2 - Provider found: $provider")
                return provider
            } catch (e: JSONException) {
                logger.info("2 - JSON expection: $e")
                break
            }
        }
        return null
    }
}

/**
 * Third implementation for DistributionProviderChecker
 *
 * @param context application context used to get the packageManager and contentResolver
 */
class ThirdDistributionProviderChecker(private val context: Context) : DistributionProviderChecker {
    override fun queryProvider(): String? {
        logger.info("3 - Starting check...")
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
        logger.info("3 - Cursor available")
        while (moveToNext()) {
            val dataColumnIndex = getColumnIndex(ENCRYPTED_DATA_COLUMN)

            // Check if columns exist
            if (dataColumnIndex == -1) {
                break
            }

            val data = getString(dataColumnIndex) ?: break
            try {
                val jsonObject = JSONObject(data)
                val provider = jsonObject.getString("provider")

                logger.info("3 - data: $data")

                val packageNameColumnIndex = getColumnIndex(PACKAGE_NAME_COLUMN)

                if (packageNameColumnIndex == -1) {
                    logger.info("3 - no package name column")
                    return provider
                }
                logger.info("3 - package name: ${getString(packageNameColumnIndex)}")

                return provider
            } catch (e: JSONException) {
                logger.info("3 - JSON exception: $e")
                break
            }
        }
        return null
    }
}
