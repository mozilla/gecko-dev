/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.button

import android.view.SoundEffectConstants
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.Interaction
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButtonColors
import androidx.compose.material3.IconButtonDefaults
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.Text
import androidx.compose.material3.minimumInteractiveComponentSize
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.ui.icons.R as iconsR

// Temporary workaround to Compose buttons not having click sounds
// see https://issuetracker.google.com/issues/218064821

private val RippleRadius = 24.dp

/**
 * A Button with the following functionalities:
 * - it has a minimum touch target size of 48dp
 * - it will play a sound effect for clicks
 * - it will use the [AcornTheme] ripple color.
 *
 * @param onClick Callback for when this button is clicked.
 * @param contentDescription Text used by accessibility services to describe what this button does.
 * @param modifier Optional modifier for further customisation of this button.
 * @param onClickLabel Semantic / accessibility label for the [onClick] action.
 * Will be read as "Double tap to [onClickLabel]".
 * @param enabled Whether or not this button will handle input events and appear enabled
 * for semantics purposes. `true` by default.
 * @param interactionSource An optional hoisted [MutableInteractionSource] for observing and
 * emitting [Interaction]s for this button. You can use this to change the button's appearance
 * or preview the button in different states. Note that if `null` is provided interactions will
 * still happen internally.
 * @param content The content to be shown inside this button.
 */
@Composable
fun IconButton(
    onClick: () -> Unit,
    contentDescription: String?,
    modifier: Modifier = Modifier,
    colors: IconButtonColors = IconButtonDefaults.iconButtonColors(
        contentColor = AcornTheme.colors.iconButton,
        disabledContentColor = AcornTheme.colors.iconDisabled,
    ),
    onClickLabel: String? = null,
    enabled: Boolean = true,
    interactionSource: MutableInteractionSource = remember { MutableInteractionSource() },
    content: @Composable () -> Unit,
) {
    val view = LocalView.current
    Box(
        modifier = modifier
            .semantics {
                if (contentDescription != null) {
                    this.contentDescription = contentDescription
                }
            }
            .minimumInteractiveComponentSize()
            .clickable(
                interactionSource = interactionSource,
                indication = ripple(
                    bounded = false,
                    radius = RippleRadius,
                    color = AcornTheme.colors.ripple,
                ),
                enabled = enabled,
                onClickLabel = onClickLabel,
                role = Role.Button,
                onClick = {
                    view.playSoundEffect(SoundEffectConstants.CLICK)
                    onClick()
                },
            ),
        contentAlignment = Alignment.Center,
    ) {
        val contentColor = if (enabled) colors.contentColor else colors.disabledContentColor
        CompositionLocalProvider(LocalContentColor provides contentColor, content = content)
    }
}

@PreviewLightDark
@Composable
private fun IconButtonPreview() {
    AcornTheme {
        IconButton(
            onClick = {},
            contentDescription = "test",
            modifier = Modifier.background(AcornTheme.colors.layer1),
        ) {
            Icon(
                painter = painterResource(iconsR.drawable.mozac_ic_bookmark_fill_24),
                contentDescription = null,
            )
        }
    }
}

@PreviewLightDark
@Composable
private fun TextButtonPreview() {
    AcornTheme {
        IconButton(
            onClick = {},
            contentDescription = "test",
            modifier = Modifier.background(AcornTheme.colors.layer1),
            colors = IconButtonDefaults.iconButtonColors(contentColor = AcornTheme.colors.textPrimary),
        ) {
            Text(text = "button")
        }
    }
}
