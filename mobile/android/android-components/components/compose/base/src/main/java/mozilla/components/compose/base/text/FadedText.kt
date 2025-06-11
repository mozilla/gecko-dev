/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.text

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.layout.onPlaced
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.text.TextLayoutResult
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Constraints
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import mozilla.components.compose.base.modifier.FadeDirection
import mozilla.components.compose.base.modifier.horizontalFadeGradient
import mozilla.components.compose.base.modifier.thenConditional

/**
 * A [Text] Composable that will have a horizontal fade and truncate the string.
 *
 * @param text The [String] for the [Text].
 * @param modifier The [Modifier] for the [Text].
 * @param style The [TextStyle] for the [Text].
 * @param truncationDirection The [TruncationDirection] that represents which side the truncation and fade are on.
 * @param fadeLength The [Dp] length of how much of the view to fade.
 */
@Composable
fun FadedText(
    text: String,
    modifier: Modifier = Modifier,
    style: TextStyle = TextStyle.Default,
    truncationDirection: TruncationDirection = TruncationDirection.START,
    fadeLength: Dp,
) {
    Text(
        text = text,
        style = style,
        softWrap = false,
        maxLines = 1,
        modifier = modifier
            .fadeText(text, style, truncationDirection, fadeLength)
            .truncateText(truncationDirection),
    )
}

private fun Modifier.fadeText(
    text: String,
    textStyle: TextStyle,
    truncationDirection: TruncationDirection,
    fadeLength: Dp,
) = composed(
    factory = {
        val layoutDirection = LocalLayoutDirection.current
        val textMeasurer = rememberTextMeasurer()
        var textLayoutState: TextLayoutResult? by remember { mutableStateOf(null) }
        val fadeDirection = remember(truncationDirection) {
            if (
                (truncationDirection == TruncationDirection.START) ==
                (layoutDirection == LayoutDirection.Ltr)
            ) {
                FadeDirection.LEFT
            } else {
                FadeDirection.RIGHT
            }
        }

        onSizeChanged {
            textLayoutState = textMeasurer.measure(
                text = text,
                maxLines = 1,
                style = textStyle,
                softWrap = false,
                constraints = Constraints(maxWidth = it.width),
            )
        }.thenConditional(
            horizontalFadeGradient(
                fadeDirection = fadeDirection,
                fadeLength = fadeLength,
            ),
        ) { textLayoutState?.didOverflowWidth == true }
    },

    inspectorInfo = {
        name = "fade text"
        properties["key"] = text
        properties["text"] = text
        properties["textStyle"] = textStyle
        properties["truncationDirection"] = truncationDirection
        properties["fadeLength"] = fadeLength
    },
)

private fun Modifier.truncateText(
    truncationDirection: TruncationDirection,
) = composed(
    factory = {
        val scope = rememberCoroutineScope()
        val scrollState = rememberScrollState()

        // By default the text has it's beginning shown and it's end clipped it it does not fit.
        // Get the reverse of this and clip the start of the text by scrolling to the end.
        horizontalScroll(
            state = scrollState,
            enabled = false,
        ).onPlaced {
            if (truncationDirection == TruncationDirection.START) {
                scope.launch { scrollState.scrollTo(scrollState.maxValue) }
            }
        }
    },
    inspectorInfo = {
        name = "truncate text"
        properties["key"] = truncationDirection
        properties["truncationDirection"] = truncationDirection
    },
)

/**
 * Describes the direction of truncation and fade for [FadedText].
 */
enum class TruncationDirection {
    START,
    END,
}

// Text length that in combination with the value set to be displayed
// would show in previews when the text is split by word
// https://issuetracker.google.com/issues/414962882
private const val TEXT_LENGTH = 320

/**
 * Preview of a short[FadedText] with start truncation and fade.
 */
@Preview(showBackground = true)
@Composable
fun ShortStartFadedTextPreview() {
    FadedText(
        text = "127.0.0.1",
        modifier = Modifier.width(TEXT_LENGTH.dp),
        style = TextStyle(fontSize = 16.sp),
        truncationDirection = TruncationDirection.START,
        fadeLength = 60.dp,
    )
}

/**
 * Preview of [FadedText] with start truncation and fade.
 */
@Preview(showBackground = true)
@Composable
fun StartFadedTextPreview() {
    FadedText(
        text = "https://data.stackexchange.com/stackoverflow/query/58883/test-long-url",
        modifier = Modifier.width(TEXT_LENGTH.dp),
        style = TextStyle(fontSize = 16.sp),
        truncationDirection = TruncationDirection.START,
        fadeLength = 60.dp,
    )
}

/**
 * Preview of a short [FadedText] with end truncation and fade
 */
@Preview(showBackground = true)
@Composable
fun ShortEndFadedTextPreview() {
    FadedText(
        text = "127.0.0.1",
        modifier = Modifier.width(TEXT_LENGTH.dp),
        style = TextStyle(fontSize = 16.sp),
        truncationDirection = TruncationDirection.END,
        fadeLength = 60.dp,
    )
}

/**
 * Preview of [FadedText] with end truncation and fade
 */
@Preview(showBackground = true)
@Composable
fun EndFadedTextPreview() {
    FadedText(
        text = "https://data.stackexchange.com/stackoverflow/query/58883/test-long-url/",
        modifier = Modifier.width(TEXT_LENGTH.dp),
        style = TextStyle(fontSize = 16.sp),
        truncationDirection = TruncationDirection.END,
        fadeLength = 60.dp,
    )
}

/**
 * Preview of a short [FadedText] with start truncation and fade and RTL language
 */
@Preview(showBackground = true)
@Composable
fun ShortRTLStartFadedTextPreview() {
    CompositionLocalProvider(
        LocalLayoutDirection provides LayoutDirection.Rtl,
    ) {
        FadedText(
            text = "127.0.0.1",
            modifier = Modifier.width(TEXT_LENGTH.dp),
            style = TextStyle(fontSize = 16.sp),
            truncationDirection = TruncationDirection.START,
            fadeLength = 60.dp,
        )
    }
}

/**
 * Preview of [FadedText] with start truncation and fade and RTL language
 */
@Preview(showBackground = true)
@Composable
fun RTLStartFadedTextPreview() {
    CompositionLocalProvider(
        LocalLayoutDirection provides LayoutDirection.Rtl,
    ) {
        FadedText(
            text = "https://data.stackexchange.com/stackoverflow/query/58883/test-long-url/",
            modifier = Modifier.width(TEXT_LENGTH.dp),
            style = TextStyle(fontSize = 16.sp),
            truncationDirection = TruncationDirection.START,
            fadeLength = 60.dp,
        )
    }
}

/**
 * Preview of a short [FadedText] with end truncation and fade and RTL language
 */
@Preview(showBackground = true)
@Composable
fun ShortRTLEndFadedTextPreview() {
    CompositionLocalProvider(
        LocalLayoutDirection provides LayoutDirection.Rtl,
    ) {
        FadedText(
            text = "127.0.0.1",
            modifier = Modifier.width(TEXT_LENGTH.dp),
            style = TextStyle(fontSize = 16.sp),
            truncationDirection = TruncationDirection.END,
            fadeLength = 60.dp,
        )
    }
}

/**
 * Preview of [FadedText] with end truncation and fade and RTL language
 */
@Preview(showBackground = true)
@Composable
fun RTLEndFadedTextPreview() {
    CompositionLocalProvider(
        LocalLayoutDirection provides LayoutDirection.Rtl,
    ) {
        FadedText(
            text = "https://data.stackexchange.com/stackoverflow/query/58883/test-long-url/",
            modifier = Modifier.width(TEXT_LENGTH.dp),
            style = TextStyle(fontSize = 16.sp),
            truncationDirection = TruncationDirection.END,
            fadeLength = 60.dp,
        )
    }
}
