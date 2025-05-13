/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.pwa

import android.content.Context
import android.content.Intent
import androidx.core.net.toUri
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.request.RequestInterceptor
import mozilla.components.feature.pwa.ext.putUrlOverride
import mozilla.components.feature.pwa.intent.WebAppIntentProcessor

/**
 * This feature will intercept requests and reopen them in the corresponding installed PWA, if any.
 *
 * @param context application context used for launching activities or accessing system services
 * @param manifestStorage  Disk storage for [WebAppManifest]. Other components use this class to
 * reload a saved manifest.
 * @param launchFromInterceptor flag to determine whether intercepted requests should directly launch
 * the PWA
 */
class WebAppInterceptor(
    private val context: Context,
    private val manifestStorage: ManifestStorage,
    private val launchFromInterceptor: Boolean = true,
) : RequestInterceptor {

    @Suppress("ReturnCount")
    override fun onLoadRequest(
        engineSession: EngineSession,
        uri: String,
        lastUri: String?,
        hasUserGesture: Boolean,
        isSameDomain: Boolean,
        isRedirect: Boolean,
        isDirectNavigation: Boolean,
        isSubframeRequest: Boolean,
    ): RequestInterceptor.InterceptionResponse? {
        val scope = manifestStorage.getInstalledScope(uri) ?: return null
        val startUrl = manifestStorage.getStartUrlForInstalledScope(scope) ?: return null
        val intent = createIntentFromUri(startUrl, uri)

        if (!launchFromInterceptor) {
            return RequestInterceptor.InterceptionResponse.AppIntent(intent, uri, null, null)
        }

        intent.flags = intent.flags or Intent.FLAG_ACTIVITY_NEW_TASK
        context.startActivity(intent)

        return RequestInterceptor.InterceptionResponse.Deny
    }

    /**
     * Creates a new VIEW_PWA intent for a URL.
     *
     * @param startUrl the original start URL associated with the PWA
     * @param urlOverride an optional override URL to open instead of the start URL; defaults to [startUrl]
     *
     * @return an [Intent] configured to launch the PWA with the given URL
     */
    private fun createIntentFromUri(startUrl: String, urlOverride: String = startUrl): Intent {
        return Intent(WebAppIntentProcessor.ACTION_VIEW_PWA, startUrl.toUri()).apply {
            this.addCategory(Intent.CATEGORY_DEFAULT)
            this.putUrlOverride(urlOverride)
        }
    }
}
