/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import android.net.Uri
import androidx.annotation.VisibleForTesting
import org.json.JSONObject

private const val SEARCH_TYPE_SAP_FOLLOW_ON = "sap-follow-on"
private const val SEARCH_TYPE_SAP = "sap"
private const val SEARCH_TYPE_ORGANIC = "organic"
private const val CHANNEL_KEY = "channel"
private val validChannelSet = setOf("ts")

/**
 * Get a String in a specific format allowing to identify how an ads/search provider was used.
 *
 * @see [TrackKeyInfo.createTrackKey]
 */
@Suppress("NestedBlockDepth", "ComplexMethod")
internal fun getTrackKey(
    provider: SearchProviderModel,
    uri: Uri,
    cookies: List<JSONObject>,
): String {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1930629 for more context
    val cleanUri = uri.lowercaseQueryParameterKeys()
    var type = SEARCH_TYPE_ORGANIC
    val paramSet = cleanUri.queryParameterNames
    var code: String? = "none"

    if (provider.codeParamName.isNotEmpty()) {
        code = cleanUri.getQueryParameter(provider.codeParamName.lowercase())
        if (code.isNullOrEmpty() &&
            provider.telemetryId == "baidu" &&
            cleanUri.toString().contains("from=")
        ) {
            code = cleanUri.toString().substringAfter("from=", "")
                .substringBefore("/", "")
        }
        if (code != null) {
            // The code is only included if it matches one of the specific ones.
            if (provider.taggedCodes.contains(code)) {
                type = SEARCH_TYPE_SAP
                if (provider.followOnParamNames?.any { p -> paramSet.contains(p) } == true) {
                    type = SEARCH_TYPE_SAP_FOLLOW_ON
                }
            } else if (provider.organicCodes?.contains(code) == true) {
                type = SEARCH_TYPE_ORGANIC
            } else if (provider.expectedOrganicCodes?.contains(code) == true) {
                code = "none"
            } else {
                code = "other"
            }
        } else if (provider.followOnCookies != null) {
            // Try cookies first because Bing has followOnCookies and valid code, but no
            // followOnParams => would track organic instead of sap-follow-on
            getTrackKeyFromCookies(provider, cleanUri, cookies)?.let {
                return it.createTrackKey()
            }
        }

        // For Bing if it didn't have a valid cookie and for all the other search engines
        if (hasValidCode(cleanUri.getQueryParameter(provider.codeParamName), provider)) {
            var channel = cleanUri.getQueryParameter(CHANNEL_KEY)

            // For Bug 1751955
            if (!validChannelSet.contains(channel)) {
                channel = null
            }
            return TrackKeyInfo(provider.telemetryId, type, code, channel).createTrackKey()
        }
    }
    return TrackKeyInfo(provider.telemetryId, type, code).createTrackKey()
}

private fun getTrackKeyFromCookies(
    provider: SearchProviderModel,
    uri: Uri,
    cookies: List<JSONObject>,
): TrackKeyInfo? {
    // Especially Bing requires lots of extra work related to cookies.
    provider.followOnCookies?.forEach { followOnCookie ->
        val eCode = uri.getQueryParameter(followOnCookie.extraCodeParamName.lowercase())

        if (eCode == null || !followOnCookie.extraCodePrefixes.any { prefix ->
                eCode.startsWith(prefix)
            }
        ) {
            return@forEach
        }

        // If this cookie is present, it's probably an SAP follow-on.
        // This might be an organic follow-on in the same session, but there
        // is no way to tell the difference.
        for (cookie in cookies) {
            if (cookie.getString("name") != followOnCookie.name) {
                continue
            }
            // Cookie values may take the form of "foo=bar&baz=1".
            // Get the value of the follow-on cookie parameter or continue searching in the next cookie
            val followOnParamName = cookie.getString("value")
                .split("&")
                .map { it.split("=") }
                .firstOrNull { it[0] == followOnCookie.codeParamName }
                ?.get(1)
                ?: continue

            provider.taggedCodes.firstOrNull { it == followOnParamName }?.let {
                return TrackKeyInfo(provider.telemetryId, SEARCH_TYPE_SAP_FOLLOW_ON, it)
            }
        }
    }
    return null
}

private fun hasValidCode(code: String?, provider: SearchProviderModel): Boolean =
    code != null && provider.taggedCodes.any { prefix -> code == prefix }

@VisibleForTesting
internal fun Uri.lowercaseQueryParameterKeys(): Uri {
    val newQueryParameters = queryParameterNames.flatMap { key ->
        getQueryParameters(key).map { value -> "${key.lowercase()}=$value" }
    }.joinToString("&")

    return this@lowercaseQueryParameterKeys.buildUpon()
        .encodedQuery(newQueryParameters)
        .build()
}
