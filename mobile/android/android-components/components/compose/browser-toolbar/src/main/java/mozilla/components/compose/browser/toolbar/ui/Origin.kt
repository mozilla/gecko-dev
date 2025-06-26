/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.content.res.Configuration.UI_MODE_NIGHT_YES
import android.content.res.Configuration.UI_MODE_TYPE_NORMAL
import android.view.SoundEffectConstants
import androidx.annotation.StringRes
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Indication
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement.Center
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.LocalTextStyle
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Alignment.Companion.Bottom
import androidx.compose.ui.Alignment.Companion.Start
import androidx.compose.ui.Modifier
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role.Companion.Button
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mozilla.components.compose.base.menu.CustomPlacementPopup
import mozilla.components.compose.base.menu.CustomPlacementPopupHorizontalContent
import mozilla.components.compose.base.modifier.thenConditional
import mozilla.components.compose.base.text.FadedText
import mozilla.components.compose.base.text.TruncationDirection.END
import mozilla.components.compose.base.text.TruncationDirection.START
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.R
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.ContextualMenuOption
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.TextGravity
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.TextGravity.TEXT_GRAVITY_END
import mozilla.components.compose.browser.toolbar.concept.PageOrigin.Companion.TextGravity.TEXT_GRAVITY_START
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.utils.PageOriginContextualMenuBuilder
import mozilla.components.support.utils.ClipboardHandler

private const val URL_TEXT_SIZE_ALONE = 15
private const val URL_TEXT_SIZE_WITH_TITLE = 12
private const val FADE_LENGTH = 66

/**
 * Custom layout for showing the origin - title and url of a webpage.
 *
 * @param hint [StringRes] the string to show when the URL is not available
 * @param modifier [Modifier] to apply to this composable for further customisation.
 * @param url the URL of the webpage. Can be `null` or empty, in which vase the [hint] will be shown.
 * @param title the title of the webpage. Can be `null` or empty, in which case only [url] or [hint] will be shown.
 * @param onClick Optional [BrowserToolbarEvent] to be dispatched when this layout is clicked.
 * @param onLongClick Optional [BrowserToolbarInteraction] describing how to handle this layout being long clicked.
 * To ensure long clicks handling the normal click behavior should also be set.
 * @param onInteraction [BrowserToolbarInteraction] to be dispatched when this layout is interacted with.
 * @param onInteraction Callback for handling [BrowserToolbarEvent]s on user interactions.
 */
@OptIn(ExperimentalFoundationApi::class) // for combinedClickable
@Composable
@Suppress("LongMethod")
internal fun Origin(
    @StringRes hint: Int,
    modifier: Modifier = Modifier,
    url: String? = null,
    registrableDomainIndexRange: Pair<Int, Int>? = null,
    title: String? = null,
    textGravity: TextGravity = TEXT_GRAVITY_START,
    contextualMenuOptions: List<ContextualMenuOption> = emptyList(),
    onClick: BrowserToolbarEvent?,
    onLongClick: BrowserToolbarEvent?,
    onInteraction: (BrowserToolbarEvent) -> Unit,
) {
    val view = LocalView.current
    val haptic = LocalHapticFeedback.current
    val shouldReactToLongClicks = remember(onLongClick, contextualMenuOptions) {
        onLongClick != null || contextualMenuOptions.isNotEmpty()
    }
    var showMenu by remember { mutableStateOf(false) }
    val clipboardHandler = remember(view) { ClipboardHandler(view.context) }

    val shouldShowTitle = remember(title) { title != null && title.isNotBlank() }
    val urlTextSize = remember(shouldShowTitle) {
        when (shouldShowTitle) {
            true -> URL_TEXT_SIZE_WITH_TITLE
            false -> URL_TEXT_SIZE_ALONE
        }
    }

    val hint = stringResource(hint)
    val urlToShow: String = remember(url) {
        when (url == null || url.isBlank()) {
            true -> hint
            else -> url
        }
    }

    CompositionLocalProvider(LocalIndication provides NoRippleIndication) {
        Box(
            contentAlignment = Alignment.CenterStart,
            modifier = modifier
                .clearAndSetSemantics {
                    this.contentDescription = "${title ?: ""} $urlToShow. $hint"
                }
                .clickable(
                    enabled = onClick != null && !shouldReactToLongClicks,
                ) {
                    view.playSoundEffect(SoundEffectConstants.CLICK)
                    onInteraction(requireNotNull(onClick))
                }
                .thenConditional(
                    Modifier.combinedClickable(
                        role = Button,
                        onClick = {
                            view.playSoundEffect(SoundEffectConstants.CLICK)
                            onInteraction(requireNotNull(onClick))
                        },
                        onLongClick = {
                            haptic.performHapticFeedback(HapticFeedbackType.LongPress)
                            showMenu = true
                            onLongClick?.let { onInteraction(it) }
                        },
                    ),
                ) { onClick != null && shouldReactToLongClicks },
        ) {
            Column(
                verticalArrangement = Center,
            ) {
                Title(title, textGravity)

                Url(urlToShow, registrableDomainIndexRange, urlTextSize)
            }

            LongPressMenu(showMenu, contextualMenuOptions, clipboardHandler, onInteraction) {
                showMenu = false
            }
        }
    }
}

@Composable
private fun Title(
    title: String?,
    textGravity: TextGravity,
) {
    if (title != null && title.isNotBlank()) {
        FadedText(
            text = title,
            style = TextStyle(
                fontSize = URL_TEXT_SIZE_ALONE.sp,
                color = AcornTheme.colors.textSecondary,
            ),
            truncationDirection = textGravity.toTextTruncationDirection(),
            fadeLength = FADE_LENGTH.dp,
        )
    }
}

@Composable
private fun Url(
    url: String,
    registrableDomainIndexRange: Pair<Int, Int>?,
    fontSize: Int,
) {
    // Ensure compatibility with MaterialTheme attributes. See bug 1936346 for more context.
    val materialTextStyle = LocalTextStyle.current

    HighlightedDomainUrl(
        url = url,
        registrableDomainIndexRange = registrableDomainIndexRange,
        fadedTextStyle = materialTextStyle.merge(
            fontSize = fontSize.sp,
            color = AcornTheme.colors.textSecondary,
        ),
        boldedTextStyle = materialTextStyle.merge(
            fontSize = fontSize.sp,
            color = AcornTheme.colors.textPrimary,
        ),
    )
}

@Composable
private fun LongPressMenu(
    isVisible: Boolean,
    contextualMenuOptions: List<ContextualMenuOption>,
    clipboard: ClipboardHandler,
    onInteraction: (BrowserToolbarEvent) -> Unit,
    onDismiss: () -> Unit,
) {
    CustomPlacementPopup(
        isVisible = isVisible,
        onDismissRequest = onDismiss,
        horizontalAlignment = Start,
        verticalAlignment = Bottom,
    ) {
        val menuItems = PageOriginContextualMenuBuilder.buildMenuOptions(
            clipboard = clipboard,
            allowedMenuOptions = contextualMenuOptions,
        )
        CustomPlacementPopupHorizontalContent {
            items(menuItems) { menuItem ->
                menuItemComposable(menuItem) { event ->
                    onDismiss()
                    onInteraction(event)
                }.invoke()
            }
        }
    }
}

private fun TextGravity.toTextTruncationDirection() = when (this) {
    TEXT_GRAVITY_START -> END
    TEXT_GRAVITY_END -> START
}

/**
 * Custom indication disabling click ripples.
 */
private object NoRippleIndication : Indication {
    override fun equals(other: Any?): Boolean = other === this

    override fun hashCode(): Int = System.identityHashCode(this)
}

@Preview(showBackground = true)
@Composable
private fun OriginPreviewWithJustTheHint() {
    AcornTheme {
        Origin(
            hint = R.string.mozac_browser_toolbar_search_hint,
            url = null,
            title = null,
            onClick = object : BrowserToolbarEvent {},
            onLongClick = null,
            onInteraction = {},
        )
    }
}

@Preview(uiMode = UI_MODE_NIGHT_YES or UI_MODE_TYPE_NORMAL)
@Composable
private fun OriginPreviewWithTitleAndURL() {
    AcornTheme {
        Origin(
            hint = R.string.mozac_browser_toolbar_search_hint,
            modifier = Modifier.background(AcornTheme.colors.layer1),
            url = "https://mozilla.com",
            title = "Test title",
            onClick = object : BrowserToolbarEvent {},
            onLongClick = null,
            onInteraction = {},
        )
    }
}

@Preview(showBackground = true, widthDp = 160)
@Composable
private fun OriginPreviewWithTitleAndURLStart() {
    AcornTheme {
        Origin(
            hint = R.string.mozac_browser_toolbar_search_hint,
            url = "https://mozilla.com/firefox-browser",
            title = "Test title",
            onClick = object : BrowserToolbarEvent {},
            onLongClick = null,
            onInteraction = {},
        )
    }
}

@Preview(widthDp = 160, uiMode = UI_MODE_NIGHT_YES or UI_MODE_TYPE_NORMAL)
@Composable
private fun OriginPreviewWithTitleAndURLEnd() {
    AcornTheme {
        Origin(
            hint = R.string.mozac_browser_toolbar_search_hint,
            modifier = Modifier.background(AcornTheme.colors.layer1),
            url = "https://mozilla.com/firefox-browser",
            title = "Test title",
            textGravity = TEXT_GRAVITY_START,
            onClick = object : BrowserToolbarEvent {},
            onLongClick = null,
            onInteraction = {},
        )
    }
}

@Preview(widthDp = 160)
@Composable
private fun OriginPreviewWithJustURLStart() {
    AcornTheme {
        Origin(
            hint = R.string.mozac_browser_toolbar_search_hint,
            modifier = Modifier.background(AcornTheme.colors.layer1),
            url = "https://mozilla.com/firefox-browser",
            title = null,
            textGravity = TEXT_GRAVITY_END,
            onClick = object : BrowserToolbarEvent {},
            onLongClick = null,
            onInteraction = {},
        )
    }
}

@Preview(widthDp = 160, uiMode = UI_MODE_NIGHT_YES or UI_MODE_TYPE_NORMAL)
@Composable
private fun OriginPreviewWithJustURLEnd() {
    AcornTheme {
        Origin(
            hint = R.string.mozac_browser_toolbar_search_hint,
            modifier = Modifier.background(AcornTheme.colors.layer1),
            url = "https://mozilla.com/firefox-browser",
            title = null,
            textGravity = TEXT_GRAVITY_START,
            onClick = object : BrowserToolbarEvent {},
            onLongClick = null,
            onInteraction = {},
        )
    }
}
