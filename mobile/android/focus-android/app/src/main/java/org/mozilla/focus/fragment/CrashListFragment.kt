/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.core.net.toUri
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.ui.AbstractCrashListFragment
import org.mozilla.focus.ext.components

/**
 * Fragment showing list of past crashes.
 */
class CrashListFragment(private val paddingNeeded: Boolean = false) : AbstractCrashListFragment() {
    override val reporter: CrashReporter by lazy { requireContext().components.crashReporter }

    override fun onCrashServiceSelected(url: String) {
        val intent = Intent(Intent.ACTION_VIEW).apply {
            data = url.toUri()
            `package` = requireContext().packageName
        }
        startActivity(intent)
        requireActivity().finish()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        if (paddingNeeded) {
            val originalTopPadding = view.paddingTop

            ViewCompat.setOnApplyWindowInsetsListener(view) { _, windowInsets ->
                val systemBarsInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
                view.updatePadding(top = originalTopPadding + systemBarsInsets.top)

                windowInsets
            }
        }
    }
}
