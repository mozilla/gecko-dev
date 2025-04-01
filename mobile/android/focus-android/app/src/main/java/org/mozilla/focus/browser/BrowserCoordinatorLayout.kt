/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.browser

import android.content.Context
import android.util.AttributeSet
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat.Type.displayCutout
import androidx.core.view.WindowInsetsCompat.Type.systemBars
import androidx.core.view.updatePadding

/**
 * A [CoordinatorLayout] implementation used in the browser.
 */
class BrowserCoordinatorLayout @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : CoordinatorLayout(context, attrs, defStyleAttr) {
    val persistentInsetsTypes = systemBars() or displayCutout()

    init {
        ViewCompat.setOnApplyWindowInsetsListener(this) { _, windowInsets ->
            val persistentInsets = windowInsets.getInsets(persistentInsetsTypes)
            updatePadding(top = persistentInsets.top)
            return@setOnApplyWindowInsetsListener windowInsets
        }
    }

    override fun requestDisallowInterceptTouchEvent(b: Boolean) {
        // As this is a direct parent of EngineView, we don't want to propagate this request to the parent
        // because that would prevent the hiding of the toolbar.
    }
}
