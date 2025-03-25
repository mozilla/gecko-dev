/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.feature.prompts.concept.PasswordPromptView
import mozilla.components.feature.prompts.concept.ToggleablePrompt
import org.mozilla.fenix.browser.AutofillSelectBarBehavior
import org.mozilla.fenix.components.toolbar.ToolbarPosition

/**
 * Fenix specific implementation of [PasswordPromptView]. This class accomplishes two things:
 * 1. Allows us to add Fenix specific CoordinatorLayout behavior when a prompt is presented.
 * 2. The `PromptFeature` delegates require a [PasswordPromptView] on initialization.
 *  With Bug 1947519 we need to have the ability to delay creating the view until we need it.
 *  This class allows us to pass a concrete [PasswordPromptView] to the `PromptFeature` and lazily
 *  initialize the view.
 *
 * @param viewProvider Closure to provide a view of type V where V is a View AND an [PasswordPromptView].
 * @param toolbarPositionProvider Closure to provide the current [ToolbarPosition].
 * @param onShow callback that is called when the prompt is presented.
 * @param onHide callback that is called when the prompt is dismissed.
 *
 * @see [FenixAutocompletePrompt]
 */
class FenixSuggestStrongPasswordPrompt<V>(
    private val viewProvider: () -> V,
    private val toolbarPositionProvider: () -> ToolbarPosition,
    private val onShow: () -> Unit,
    private val onHide: () -> Unit,
) : PasswordPromptView where V : View, V : PasswordPromptView {

    private val view: V by lazy { viewProvider() }

    private var isVisible: Boolean = false

    override var passwordPromptListener: PasswordPromptView.Listener? = null
    override var toggleablePromptListener: ToggleablePrompt.Listener? = null

    override val isPromptDisplayed: Boolean
        get() = isVisible

    override fun showPrompt() = with(view) {
        showPrompt()
        behavior = createCustomAutofillBarBehavior()
        toggleablePromptListener = this@FenixSuggestStrongPasswordPrompt.toggleablePromptListener
        passwordPromptListener = this@FenixSuggestStrongPasswordPrompt.passwordPromptListener
        this@FenixSuggestStrongPasswordPrompt.onShow()
        isVisible = true
    }

    override fun hidePrompt() = with(view) {
        hidePrompt()
        behavior = null
        toggleablePromptListener = null
        passwordPromptListener = null
        this@FenixSuggestStrongPasswordPrompt.onHide()
        isVisible = false
    }

    private fun <T : View> T.createCustomAutofillBarBehavior() = AutofillSelectBarBehavior<T>(
        context = context,
        toolbarPosition = toolbarPositionProvider(),
    )

    private var View.behavior: CoordinatorLayout.Behavior<*>?
        get() = (layoutParams as? CoordinatorLayout.LayoutParams)?.behavior
        set(value) {
            (layoutParams as? CoordinatorLayout.LayoutParams)?.behavior = value
        }
}
