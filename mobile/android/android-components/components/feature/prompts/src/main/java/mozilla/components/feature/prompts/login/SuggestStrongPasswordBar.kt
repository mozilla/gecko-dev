/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.content.Context
import android.util.AttributeSet
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.AbstractComposeView
import androidx.core.view.isVisible
import mozilla.components.feature.prompts.concept.PasswordPromptView
import mozilla.components.feature.prompts.concept.ToggleablePrompt

/**
 * A prompt bar implementing [PasswordPromptView] to display the strong generated password.
 */
class SuggestStrongPasswordBar @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : AbstractComposeView(context, attrs, defStyleAttr), PasswordPromptView {
    private val colors = PasswordGeneratorPromptColors(context)

    override var passwordPromptListener: PasswordPromptView.Listener? = null
    override var toggleablePromptListener: ToggleablePrompt.Listener? = null
    override val isPromptDisplayed
        get() = isVisible

    @Composable
    override fun Content() {
        PasswordGeneratorPrompt(
            onGeneratedPasswordPromptClick = { passwordPromptListener?.onGeneratedPasswordPromptClick() },
            colors = colors,
        )
    }

    override fun showPrompt() {
        isVisible = true
        toggleablePromptListener?.onShown()
    }

    override fun hidePrompt() {
        isVisible = false
        toggleablePromptListener?.onHidden()
    }
}
