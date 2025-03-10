/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.feature.prompts.address.AddressSelectBar
import mozilla.components.feature.prompts.concept.ExpandablePrompt
import mozilla.components.feature.prompts.concept.ToggleablePrompt
import mozilla.components.feature.prompts.creditcard.CreditCardSelectBar
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
    addressBar: AddressSelectBar,
    creditCardBar: CreditCardSelectBar,
    private val settings: Settings,
    private val onAutofillBarShown: () -> Unit,
    private val onAutofillBarHidden: () -> Unit,
) {
    init {
        passwordBar.toggleablePromptListener = passwordBar.createToggleListener()
        addressBar.toggleablePromptListener = addressBar.createToggleListener()
        addressBar.expandablePromptListener = addressBar.createExpandedListener()
        creditCardBar.toggleablePromptListener = creditCardBar.createToggleListener()
        creditCardBar.expandablePromptListener = creditCardBar.createExpandedListener()
    }

    var isVisible: Boolean = false
        private set
    var isExpanded: Boolean = false
        private set

    private fun <T : View> T.createExpandedListener() = object : ExpandablePrompt.Listener {
        override fun onExpanded() {
            (behavior as? AutofillSelectBarBehavior<*>)?.placeAtBottom(this@createExpandedListener)
            // Remove the custom behavior to ensure the autofill bar stays fixed in place
            behavior = null
            isExpanded = true
        }
        override fun onCollapsed() {
            behavior = createCustomAutofillBarBehavior()
            isExpanded = false
        }
    }

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
