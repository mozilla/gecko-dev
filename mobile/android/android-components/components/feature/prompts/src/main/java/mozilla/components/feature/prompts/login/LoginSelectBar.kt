/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.content.Context
import android.util.AttributeSet
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.AbstractComposeView
import androidx.core.view.isVisible
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.concept.AutocompletePrompt
import mozilla.components.feature.prompts.concept.ExpandablePrompt
import mozilla.components.feature.prompts.concept.SelectablePromptView
import mozilla.components.feature.prompts.concept.ToggleablePrompt

/**
 * A customizable multiple login selection bar implementing [SelectablePromptView].
 */
class LoginSelectBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : AbstractComposeView(context, attrs, defStyleAttr),
    AutocompletePrompt<Login>,
    ExpandablePrompt {

    private var logins by mutableStateOf(listOf<Login>())
    private var isExpanded by mutableStateOf(false)
    private val loginPickerColors = LoginPickerColors(context)
    override var isPromptDisplayed: Boolean = false
        private set

    override var toggleablePromptListener: ToggleablePrompt.Listener? = null
    override var selectablePromptListener: SelectablePromptView.Listener<Login>? = null
    override var expandablePromptListener: ExpandablePrompt.Listener? = null

    @Composable
    override fun Content() {
        LoginPicker(
            logins = logins,
            isExpanded = isExpanded,
            onExpandToggleClick = {
                when (it) {
                    true -> expandablePromptListener?.onExpanded()
                    false -> expandablePromptListener?.onCollapsed()
                }
                isExpanded = it
            },
            onLoginSelected = { selectablePromptListener?.onOptionSelect(it) },
            onManagePasswordClicked = { selectablePromptListener?.onManageOptions() },
            loginPickerColors = loginPickerColors,
        )
    }

    override fun populate(options: List<Login>) {
        logins = options
    }

    override fun showPrompt() {
        isVisible = true
        isPromptDisplayed = true
        toggleablePromptListener?.onShown()
    }

    override fun hidePrompt() {
        this.isVisible = false
        this.isExpanded = false
        isPromptDisplayed = false
        toggleablePromptListener?.onHidden()
        logins = listOf()
    }

    override fun expand() {
        isExpanded = true
        expandablePromptListener?.onExpanded()
    }

    override fun collapse() {
        isExpanded = false
        expandablePromptListener?.onCollapsed()
    }
}
