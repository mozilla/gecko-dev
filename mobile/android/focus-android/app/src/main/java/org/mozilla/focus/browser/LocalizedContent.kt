/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.browser

import android.content.Context
import org.mozilla.focus.R
import org.mozilla.focus.utils.HtmlLoader

object LocalizedContent {
    // We can't use "about:" because webview silently swallows about: pages, hence we use
    // a custom scheme.
    const val URL_GPL = "focus:gpl"
    const val URL_LICENSES = "focus:licenses"

    fun loadLicenses(context: Context): String {
        return HtmlLoader.loadResourceFile(context, R.raw.licenses, emptyMap())
    }

    fun loadGPL(context: Context): String {
        return HtmlLoader.loadResourceFile(context, R.raw.gpl, emptyMap())
    }
}
