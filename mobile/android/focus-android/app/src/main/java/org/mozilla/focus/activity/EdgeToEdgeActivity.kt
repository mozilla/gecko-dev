/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.WindowManager
import androidx.activity.enableEdgeToEdge
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat.Type.displayCutout
import androidx.core.view.WindowInsetsCompat.Type.systemBars
import mozilla.components.support.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.fragment.BrowserFragment

/**
 * Base [LocaleAwareAppCompatActivity] that handles adapting the UI to edge to edge display.
 */
open class EdgeToEdgeActivity : LocaleAwareAppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.isNavigationBarContrastEnforced = false
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            enableEdgeToEdge()
            setupPersistentInsets()
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility =
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
        }
    }

    private fun setupPersistentInsets() {
        val rootView = window.decorView.findViewById<View>(android.R.id.content)
        val persistentInsetsTypes = systemBars() or displayCutout()

        ViewCompat.setOnApplyWindowInsetsListener(rootView) { view, windowInsets ->
            val isInImmersiveMode =
                window.attributes.flags and WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS != 0
            if (isInImmersiveMode) {
                // remove padding in immersive mode
                view.setPadding(0, 0, 0, 0)
            } else {
                val persistentInsets = windowInsets.getInsets(persistentInsetsTypes)
                val currentFragment = supportFragmentManager.fragments.firstOrNull { it.isVisible }
                val bottomPadding =
                    when (currentFragment) {
                        is BrowserFragment -> persistentInsets.bottom
                        else -> 0 // no need for bottom padding on other screens.
                    }

                view.setPadding(
                    persistentInsets.left,
                    0, // we want to draw behind status bar
                    persistentInsets.right,
                    bottomPadding,
                )
            }
            windowInsets
        }
    }
}
