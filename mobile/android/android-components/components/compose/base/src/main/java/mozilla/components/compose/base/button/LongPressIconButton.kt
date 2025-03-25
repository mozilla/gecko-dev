/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.button

import android.view.SoundEffectConstants
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.Interaction
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.material.ContentAlpha
import androidx.compose.material.Icon
import androidx.compose.material.LocalContentAlpha
import androidx.compose.material.Text
import androidx.compose.material.minimumInteractiveComponentSize
import androidx.compose.material.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.LightDarkPreview
import mozilla.components.compose.base.modifier.rightClickable
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.ui.icons.R as iconsR

// Temporary workaround to Compose buttons not having click sounds
// see https://issuetracker.google.com/issues/219984415

private val RippleRadius = 24.dp

/**
 * A button with the following functionalities:
 * - it has a minimum touch target size of 48dp
 * - it will perform a haptic feedback for long clicks or right clicks
 * - it will play a sound effect for clicks
 * - it will use the [AcornTheme] ripple color.
 *
 * @param onClick Callback for when this button is clicked.
 * @param onLongClick Callback for when this button is long clicked or right click.
 * @param contentDescription Text used by accessibility services to describe what this button does.
 * @param modifier Optional modifier for further customisation of this button.
 * @param onClickLabel Semantic / accessibility label for the [onClick] action.
 * Will be read as "Double tap to [onLongClick]". Leave `null` for "activate" to be read.
 * @param onLongClickLabel Semantic / accessibility label for the [onLongClick] action.
 * Will be read as "Double tap and hold to [onLongClickLabel]". Leave `null` for "long press" to be read.
 * @param enabled Whether or not this button will handle input events and appear enabled
 * for semantics purposes. `true` by default.
 * @param interactionSource An optional hoisted [MutableInteractionSource] for observing and
 * emitting [Interaction]s for this button. You can use this to change the button's appearance
 * or preview the button in different states. Note that if `null` is provided, interactions will
 * still happen internally.
 * @param content The content to be shown inside this button.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun LongPressIconButton(
    onClick: () -> Unit,
    onLongClick: (() -> Unit),
    contentDescription: String,
    modifier: Modifier = Modifier,
    onClickLabel: String? = null,
    onLongClickLabel: String? = null,
    enabled: Boolean = true,
    interactionSource: MutableInteractionSource = remember { MutableInteractionSource() },
    content: @Composable () -> Unit,
) {
    val haptic = LocalHapticFeedback.current
    val view = LocalView.current
    Box(
        modifier = modifier
            .semantics { this.contentDescription = contentDescription }
            .minimumInteractiveComponentSize()
            .combinedClickable(
                interactionSource = interactionSource,
                indication = ripple(bounded = false, radius = RippleRadius, color = AcornTheme.colors.ripple),
                enabled = enabled,
                onClickLabel = onClickLabel,
                role = Role.Button,
                onLongClickLabel = onLongClickLabel,
                onLongClick = {
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                    onLongClick()
                },
                onClick = {
                    view.playSoundEffect(SoundEffectConstants.CLICK)
                    onClick()
                },
            )
            .rightClickable(
                interactionSource = interactionSource,
                indication = ripple(bounded = false, radius = RippleRadius, color = AcornTheme.colors.ripple),
                onRightClick = {
                    haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                    onLongClick()
                },
            ),
        contentAlignment = Alignment.Center,
    ) {
        val contentAlpha = if (enabled) LocalContentAlpha.current else ContentAlpha.disabled
        CompositionLocalProvider(LocalContentAlpha provides contentAlpha, content = content)
    }
}

@LightDarkPreview
@Composable
private fun LongPressIconButtonPreview() {
    AcornTheme {
        LongPressIconButton(
            onClick = {},
            onLongClick = {},
            contentDescription = "test",
            modifier = Modifier.background(AcornTheme.colors.layer1),
        ) {
            Icon(
                painter = painterResource(iconsR.drawable.mozac_ic_bookmark_fill_24),
                contentDescription = null,
                tint = AcornTheme.colors.iconButton,
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun LongPressTextButtonPreview() {
    AcornTheme {
        LongPressIconButton(
            onClick = {},
            onLongClick = {},
            contentDescription = "test",
            modifier = Modifier.background(AcornTheme.colors.layer1),
        ) {
            Text(
                text = "button",
                color = AcornTheme.colors.textPrimary,
            )
        }
    }
}
