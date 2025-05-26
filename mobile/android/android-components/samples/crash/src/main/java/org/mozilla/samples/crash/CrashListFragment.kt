/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.crash

import android.widget.Toast
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.ui.AbstractCrashListFragment

/**
 * Activity showing list of past crashes.
 */
class CrashListFragment : AbstractCrashListFragment() {
    override val reporter: CrashReporter
        get() = (requireActivity().application as CrashApplication).crashReporter

    override fun onCrashServiceSelected(url: String) {
        Toast.makeText(requireActivity(), "Go to: $url", Toast.LENGTH_SHORT).show()
    }
}
