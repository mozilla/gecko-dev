/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.os.Bundle
import android.view.Menu
import android.view.MenuInflater
import android.view.MenuItem
import android.view.View
import androidx.core.view.MenuHost
import androidx.core.view.MenuProvider
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.lifecycle.Lifecycle
import androidx.preference.PreferenceFragmentCompat

abstract class BaseSettingsFragment : PreferenceFragmentCompat(), MenuProvider {
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val menuHost: MenuHost = requireHost() as MenuHost
        menuHost.addMenuProvider(this, viewLifecycleOwner, Lifecycle.State.RESUMED)

        val originalBottomPadding = view.paddingBottom

        ViewCompat.setOnApplyWindowInsetsListener(view) { _, windowInsets ->
            val systemBarsInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.updatePadding(bottom = originalBottomPadding + systemBarsInsets.bottom)

            windowInsets
        }
    }

    override fun onCreateMenu(menu: Menu, menuInflater: MenuInflater) {
        // no-op
    }

    override fun onMenuItemSelected(menuItem: MenuItem): Boolean = false
}
