/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.View
import android.widget.ToggleButton
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager

/**
 * Sets up the private browsing toggle button on the [HomeFragment].
 */
class PrivateBrowsingButtonView(
    button: ToggleButton,
    private val browsingModeManager: BrowsingModeManager,
    private val onClick: (BrowsingMode) -> Unit,
) : View.OnClickListener {

    init {
        button.contentDescription = button.context.getString(R.string.content_description_private_browsing_button)
        button.isChecked = browsingModeManager.mode.isPrivate
        button.setOnClickListener(this)
    }

    /**
     * Calls [onClick] with the new [BrowsingMode] and updates the [browsingModeManager].
     */
    override fun onClick(v: View) {
        val invertedMode = BrowsingMode.fromBoolean(!browsingModeManager.mode.isPrivate)
        onClick(invertedMode)
    }
}
