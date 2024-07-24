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
import mozilla.components.feature.prompts.concept.SelectablePromptView

/**
 * A customizable multiple login selection bar implementing [SelectablePromptView].
 */
class LoginSelectBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : AbstractComposeView(context, attrs, defStyleAttr), SelectablePromptView<Login> {
    private var logins by mutableStateOf(listOf<Login>())
    private var isExpanded by mutableStateOf(false)
    private val loginPickerColors = LoginPickerColors(context)

    override var listener: SelectablePromptView.Listener<Login>? = null

    @Composable
    override fun Content() {
        LoginPicker(
            logins = logins,
            isExpanded = isExpanded,
            onExpandToggleClick = { isExpanded = it },
            onLoginSelected = { listener?.onOptionSelect(it) },
            onManagePasswordClicked = { listener?.onManageOptions() },
            loginPickerColors = loginPickerColors,
        )
    }

    override fun showPrompt(options: List<Login>) {
        isVisible = true
        logins = options
    }

    override fun hidePrompt() {
        this.isVisible = false
        this.isExpanded = false
        logins = listOf()
    }
}
