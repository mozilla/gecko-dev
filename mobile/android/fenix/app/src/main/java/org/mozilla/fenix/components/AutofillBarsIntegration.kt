/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.feature.prompts.concept.ToggleablePrompt
import mozilla.components.feature.prompts.login.SuggestStrongPasswordBar
import org.mozilla.fenix.browser.AutofillSelectBarBehavior
import org.mozilla.fenix.utils.Settings

/**
 * Helper for ensuring that
 * - Autofill bars are always on top of the bottom toolbar
 * - Callers are notified when Autofill bars are shown/hidden.
 */
@Suppress("LongParameterList")
class AutofillBarsIntegration(
    passwordBar: SuggestStrongPasswordBar,
    private val settings: Settings,
    private val onAutofillBarShown: () -> Unit,
    private val onAutofillBarHidden: () -> Unit,
) {
    init {
        passwordBar.toggleablePromptListener = passwordBar.createToggleListener()
    }

    var isVisible: Boolean = false
        private set

    private fun <T : View> T.createToggleListener() = object : ToggleablePrompt.Listener {
        override fun onShown() {
            behavior = createCustomAutofillBarBehavior()
            isVisible = true
            onAutofillBarShown()
        }

        override fun onHidden() {
            // Remove the custom behavior to prevent layout evaluations while autofill bars are hidden
            behavior = null
            isVisible = false
            onAutofillBarHidden()
        }
    }

    private fun <T : View> T.createCustomAutofillBarBehavior() = AutofillSelectBarBehavior<T>(
        context = context,
        toolbarPosition = settings.toolbarPosition,
    )

    private var View.behavior: CoordinatorLayout.Behavior<*>?
        get() = (layoutParams as? CoordinatorLayout.LayoutParams)?.behavior
        set(value) {
            (layoutParams as? CoordinatorLayout.LayoutParams)?.behavior = value
        }
}
