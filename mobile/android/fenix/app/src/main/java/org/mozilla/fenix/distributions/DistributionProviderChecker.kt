/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import android.content.Context
import android.content.Intent
import android.database.Cursor
import androidx.core.net.toUri
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

/**
 * Default implementation for DistributionProviderChecker
 *
 * @param context application context used to get the packageManager and contentResolver
 */
class DefaultDistributionProviderChecker(private val context: Context) : DistributionProviderChecker {
    override fun queryProvider(): String? {
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
}
