/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.content.Intent
import androidx.core.net.toUri
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.ui.AbstractCrashListFragment
import org.mozilla.fenix.ext.components

/**
 * Fragment showing the list of past crashes.
 */
class CrashListFragment : AbstractCrashListFragment() {
    override val reporter: CrashReporter by lazy { requireContext().components.analytics.crashReporter }

    override fun onCrashServiceSelected(url: String) {
        val intent = Intent(Intent.ACTION_VIEW)
        intent.data = url.toUri()
        intent.`package` = requireContext().packageName
        startActivity(intent)
        requireActivity().finish()
    }
}
