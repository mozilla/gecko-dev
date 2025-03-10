/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components

import android.view.View
import androidx.coordinatorlayout.widget.CoordinatorLayout
import mozilla.components.feature.prompts.concept.AutocompletePrompt
import mozilla.components.feature.prompts.concept.ExpandablePrompt
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.concept.ToggleablePrompt
import org.mozilla.fenix.browser.AutofillSelectBarBehavior
import org.mozilla.fenix.components.toolbar.ToolbarPosition

/**
 * Fenix specific implementation of [AutocompletePrompt]. This class accomplishes two things:
 * 1. Allows us to add Fenix specific CoordinatorLayout behavior when a prompt is presented.
 * 2. The `PromptFeature` delegates require an [AutocompletePrompt] on initialization.
 *  With Bug 1947519 we need to have the ability to delay creating the view until we need it.
 *  This class allows us to pass a concrete [AutocompletePrompt] to the `PromptFeature` and lazily
 *  initialize the view.
 *
 * @param viewProvider Closure to provide a view of type V where V is a View AND an [AutocompletePrompt].
 * @param toolbarPositionProvider Closure to provide the current [ToolbarPosition].
 * @param onShow callback that is called when the prompt is presented.
 * @param onHide callback that is called when the prompt is dismissed.
 */
class FenixAutocompletePrompt<T, V>(
    private val viewProvider: () -> V,
    private val toolbarPositionProvider: () -> ToolbarPosition,
    private val onShow: () -> Unit,
    private val onHide: () -> Unit,
) : AutocompletePrompt<T>, ExpandablePrompt where V : View, V : AutocompletePrompt<T> {
    private val view: V by lazy {
        viewProvider()
    }

    override var expandablePromptListener: ExpandablePrompt.Listener? = null
    override var selectablePromptListener: SelectablePromptView.Listener<T>? = null
    override var toggleablePromptListener: ToggleablePrompt.Listener? = null

    override fun expand() = with(view) {
        if (this is ExpandablePrompt) {
            expand()
            (behavior as? AutofillSelectBarBehavior<*>)?.placeAtBottom(this)
            behavior = null
        }
    }

    override fun collapse() = with(view) {
        if (this is ExpandablePrompt) {
            collapse()
            behavior = createCustomAutofillBarBehavior()
        }
    }

    override val isPromptDisplayed: Boolean = false
    override fun populate(options: List<T>) = view.populate(options)

    override fun showPrompt() = with(view) {
        showPrompt()
        selectablePromptListener = this@FenixAutocompletePrompt.selectablePromptListener
        behavior = createCustomAutofillBarBehavior()
        this@FenixAutocompletePrompt.onShow()
    }

    override fun hidePrompt() = with(view) {
        hidePrompt()
        behavior = null
        this@FenixAutocompletePrompt.onHide()
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
