/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.tabcounter

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.dimensionResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.base.utils.toLocaleString
import mozilla.components.ui.tabcounter.TabCounterTestTags.NORMAL_TABS_COUNTER
import mozilla.components.ui.icons.R as iconsR

private const val MAX_SINGLE_DIGIT = 9
private const val MAX_VISIBLE_TABS = 99
private const val ONE_DIGIT_SIZE_RATIO = 0.5f
private const val TWO_DIGITS_SIZE_RATIO = 0.4f

/**
 * UI for displaying the number of opened tabs.
 *
 * This composable uses LocalContentColor, provided by CompositionLocalProvider,
 * to set the color of its icons and text.
 *
 * @param tabCount the number to be displayed inside the counter.
 * @param showPrivacyBadge if true, show the privacy badge.
 * @param textColor the color of the text inside of tab counter.
 * @param iconColor the border color of the tab counter.
 */
@Composable
fun TabCounter(
    tabCount: Int,
    showPrivacyBadge: Boolean = false,
    textColor: Color = AcornTheme.colors.textPrimary,
    iconColor: Color = AcornTheme.colors.iconPrimary,
) {
    val formattedTabCount = remember(tabCount) { tabCount.toLocaleString() }
    val normalTabCountText by remember(tabCount) {
        derivedStateOf {
            // Showing more than 99 tabs will be done through a different drawable / background
            // so we don't need to show any text.
            when (tabCount > MAX_VISIBLE_TABS) {
                true -> ""
                false -> formattedTabCount
            }
        }
    }
    val tabCountTextRatio by remember(tabCount) {
        derivedStateOf {
            when (tabCount > MAX_SINGLE_DIGIT) {
                true -> TWO_DIGITS_SIZE_RATIO
                false -> ONE_DIGIT_SIZE_RATIO
            }
        }
    }
    val counterBoxBackground by remember(tabCount) {
        derivedStateOf {
            when (tabCount > MAX_VISIBLE_TABS) {
                true -> R.drawable.mozac_ui_infinite_tabcounter_box
                false -> R.drawable.mozac_ui_tabcounter_box
            }
        }
    }

    val normalTabsContentDescription = stringResource(
        R.string.mozac_open_tab_counter_tab_tray,
        formattedTabCount,
    )

    val counterBoxWidthDp =
        dimensionResource(id = R.dimen.mozac_tab_counter_box_width_height)
    val counterBoxWidthPx = LocalDensity.current.run { counterBoxWidthDp.roundToPx() }
    val counterTabsTextSize by remember(tabCountTextRatio) {
        mutableIntStateOf((tabCountTextRatio * counterBoxWidthPx).toInt())
    }

    Box(
        modifier = Modifier
            .semantics(mergeDescendants = true) {
                testTag = NORMAL_TABS_COUNTER
            },
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            painter = painterResource(
                id = counterBoxBackground,
            ),
            contentDescription = normalTabsContentDescription,
            tint = iconColor,
        )

        if (tabCount <= MAX_VISIBLE_TABS) {
            Text(
                text = normalTabCountText,
                modifier = Modifier.clearAndSetSemantics {},
                color = textColor,
                fontSize = with(LocalDensity.current) { counterTabsTextSize.toDp().toSp() },
                fontWeight = FontWeight.W700,
                textAlign = TextAlign.Center,
            )
        }

        if (showPrivacyBadge) {
            Image(
                painter = painterResource(id = iconsR.drawable.mozac_ic_private_mode_circle_fill_stroke_20),
                contentDescription = null,
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(0.dp)
                    .offset(x = 8.dp, y = (-8).dp),
            )
        }
    }
}

/**
 * Test tags for the [TabCounter] composable.
 */
object TabCounterTestTags {
    private const val TAG = "TabCounterTestTags"
    const val NORMAL_TABS_COUNTER = "$TAG.tabCounter"
}

@PreviewLightDark
@Preview(locale = "ar")
@Composable
private fun TabCounterPreview() {
    AcornTheme {
        Box(
            modifier = Modifier.background(color = AcornTheme.colors.layer1),
        ) {
            TabCounter(tabCount = 55)
        }
    }
}

@PreviewLightDark
@Preview(locale = "ar")
@Composable
private fun InfiniteTabCounterPreview() {
    AcornTheme {
        Box(
            modifier = Modifier.background(color = AcornTheme.colors.layer1),
        ) {
            TabCounter(tabCount = 100)
        }
    }
}
