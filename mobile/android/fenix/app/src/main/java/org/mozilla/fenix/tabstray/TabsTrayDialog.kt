/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import androidx.activity.ComponentDialog
import androidx.activity.OnBackPressedCallback
import androidx.annotation.VisibleForTesting

/**
 * Default tabs tray dialog implementation for overriding the default on back pressed.
 */
class TabsTrayDialog(
    context: Context,
    theme: Int,
    private val interactor: () -> TabsTrayInteractor,
) : ComponentDialog(context, theme) {

    @VisibleForTesting
    internal val onBackPressedCallback = object : OnBackPressedCallback(true) {
        override fun handleOnBackPressed() {
            if (interactor().onBackPressed()) {
                return
            }

            dismiss()
        }
    }

    init {
        onBackPressedDispatcher.addCallback(
            owner = this,
            onBackPressedCallback = onBackPressedCallback,
        )
    }
}
