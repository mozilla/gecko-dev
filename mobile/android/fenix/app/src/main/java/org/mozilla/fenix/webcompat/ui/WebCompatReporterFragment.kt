/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.ui

import androidx.compose.runtime.Composable
import androidx.fragment.app.Fragment
import org.mozilla.fenix.components.lazyStore
import org.mozilla.fenix.compose.ComposeFragment
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.webcompat.store.WebCompatReporterStore

/**
 * [Fragment] for displaying the WebCompat Reporter.
 */
class WebCompatReporterFragment : ComposeFragment() {

    private val webCompatReporterStore by lazyStore {
        WebCompatReporterStore()
    }

    @Composable
    override fun UI() {
        FirefoxTheme {
            WebCompatReporter(
                store = webCompatReporterStore,
            )
        }
    }
}
