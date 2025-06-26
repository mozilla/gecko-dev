/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.tooling.preview.PreviewParameter
import androidx.compose.ui.tooling.preview.PreviewParameterProvider
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.progressbar.AnimatedProgressBar
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButtonRes
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.ContentDescription.StringResContentDescription
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.Icon.DrawableResIcon
import mozilla.components.compose.browser.toolbar.concept.PageOrigin
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.ProgressBarConfig
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity.Bottom
import mozilla.components.compose.browser.toolbar.store.ProgressBarGravity.Top
import mozilla.components.compose.browser.toolbar.ui.Origin
import mozilla.components.ui.icons.R as iconsR

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(8.dp)
private const val NO_TOOLBAR_PADDING_DP = 0
private const val TOOLBAR_PADDING_DP = 8

/**
 * Sub-component of the [BrowserToolbar] responsible for displaying the URL and related
 * controls ("display mode").
 *
 * @param pageOrigin Details about the website origin.
 * @param progressBarConfig [ProgressBarConfig] configuration for the progress bar.
 * If `null` a progress bar will not be displayed.
 * @param browserActionsStart List of browser [Action]s to be displayed at the start of the
 * toolbar, outside of the URL bounding box.
 * These should be actions relevant to the browser as a whole.
 * See [MDN docs](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction).
 * @param pageActionsStart List of navigation [Action]s to be displayed between [browserActionsStart]
 * and [pageOrigin], inside of the URL bounding box.
 * These should be actions relevant to specific webpages as opposed to [browserActionsStart].
 * See [MDN docs](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction).
 * @param pageActionsEnd List of page [Action]s to be displayed between [pageOrigin] and [browserActionsEnd],
 * inside of the URL bounding box.
 * These should be actions relevant to specific webpages as opposed to [browserActionsStart].
 * See [MDN docs](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction).
 * @param browserActionsEnd List of browser [Action]s to be displayed at the end of the toolbar,
 * outside of the URL bounding box.
 * These should be actions relevant to the browser as a whole.
 * See [MDN docs](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction).
 * @param onInteraction Callback for handling [BrowserToolbarEvent]s on user interactions.
 */
@Composable
@Suppress("LongMethod")
fun BrowserDisplayToolbar(
    pageOrigin: PageOrigin,
    progressBarConfig: ProgressBarConfig?,
    browserActionsStart: List<Action> = emptyList(),
    pageActionsStart: List<Action> = emptyList(),
    pageActionsEnd: List<Action> = emptyList(),
    browserActionsEnd: List<Action> = emptyList(),
    onInteraction: (BrowserToolbarEvent) -> Unit,
) {
    Box(
        modifier = Modifier
            .background(color = AcornTheme.colors.layer1)
            .fillMaxWidth(),
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
        ) {
            if (browserActionsStart.isNotEmpty()) {
                ActionContainer(
                    actions = browserActionsStart,
                    onInteraction = onInteraction,
                )
            }

            Row(
                modifier = Modifier
                    .padding(
                        start = when (browserActionsStart.isEmpty()) {
                            true -> TOOLBAR_PADDING_DP.dp
                            false -> NO_TOOLBAR_PADDING_DP.dp
                        },
                        top = TOOLBAR_PADDING_DP.dp,
                        end = when (browserActionsEnd.isEmpty()) {
                            true -> TOOLBAR_PADDING_DP.dp
                            false -> NO_TOOLBAR_PADDING_DP.dp
                        },
                        bottom = TOOLBAR_PADDING_DP.dp,
                    )
                    .height(40.dp)
                    .background(
                        color = AcornTheme.colors.layer3,
                        shape = ROUNDED_CORNER_SHAPE,
                    )
                    .padding(
                        start = when (pageActionsStart.isEmpty()) {
                            true -> TOOLBAR_PADDING_DP.dp
                            false -> NO_TOOLBAR_PADDING_DP.dp
                        },
                        top = NO_TOOLBAR_PADDING_DP.dp,
                        end = when (pageActionsEnd.isEmpty()) {
                            true -> TOOLBAR_PADDING_DP.dp
                            false -> NO_TOOLBAR_PADDING_DP.dp
                        },
                        bottom = NO_TOOLBAR_PADDING_DP.dp,
                    )
                    .weight(1f),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                if (pageActionsStart.isNotEmpty()) {
                    ActionContainer(
                        actions = pageActionsStart,
                        onInteraction = onInteraction,
                    )
                }

                Origin(
                    hint = pageOrigin.hint,
                    modifier = Modifier
                        .height(48.dp)
                        .weight(1f),
                    url = pageOrigin.url,
                    registrableDomainIndexRange = pageOrigin.registrableDomainIndexRange,
                    title = pageOrigin.title,
                    textGravity = pageOrigin.textGravity,
                    contextualMenuOptions = pageOrigin.contextualMenuOptions,
                    onClick = pageOrigin.onClick,
                    onLongClick = pageOrigin.onLongClick,
                    onInteraction = onInteraction,
                )

                if (pageActionsEnd.isNotEmpty()) {
                    ActionContainer(
                        actions = pageActionsEnd,
                        onInteraction = onInteraction,
                    )
                }
            }

            if (browserActionsEnd.isNotEmpty()) {
                ActionContainer(
                    actions = browserActionsEnd,
                    onInteraction = onInteraction,
                )
            }
        }

        if (progressBarConfig != null) {
            AnimatedProgressBar(
                progress = progressBarConfig.progress,
                color = progressBarConfig.color,
                modifier = when (progressBarConfig.gravity) {
                    Top -> Modifier.align(Alignment.TopCenter)
                    Bottom -> Modifier.align(Alignment.BottomCenter)
                },
            )
        }
    }
}

@PreviewLightDark
@Composable
private fun BrowserDisplayToolbarPreview(
    @PreviewParameter(DisplayToolbarDataProvider::class) config: DisplayToolbarPreviewModel,
) {
    AcornTheme {
        BrowserDisplayToolbar(
            progressBarConfig = ProgressBarConfig(
                progress = 66,
                gravity = config.progressBarGravity,
            ),
            browserActionsStart = config.browserStartActions,
            pageActionsStart = config.pageActionsStart,
            pageOrigin = PageOrigin(
                hint = R.string.mozac_browser_toolbar_search_hint,
                title = config.title,
                url = config.url,
                onClick = object : BrowserToolbarEvent {},
            ),
            pageActionsEnd = config.pageActionsEnd,
            browserActionsEnd = config.browserEndActions,
            onInteraction = {},
        )
    }
}

private data class DisplayToolbarPreviewModel(
    val browserStartActions: List<Action>,
    val pageActionsStart: List<Action>,
    val title: String?,
    val url: String?,
    val progressBarGravity: ProgressBarGravity,
    val pageActionsEnd: List<Action>,
    val browserEndActions: List<Action>,
)
private class DisplayToolbarDataProvider : PreviewParameterProvider<DisplayToolbarPreviewModel> {
    val browserStartActions = listOf(
        ActionButtonRes(
            drawableResId = iconsR.drawable.mozac_ic_home_24,
            contentDescription = android.R.string.untitled,
            onClick = object : BrowserToolbarEvent {},
        ),
    )
    val pageActionsStart = listOf(
        SearchSelectorAction(
            icon = DrawableResIcon(iconsR.drawable.mozac_ic_search_24),
            contentDescription = StringResContentDescription(resourceId = android.R.string.untitled),
            menu = { emptyList() },
            onClick = null,
        ),
    )
    val pageActionsEnd = listOf(
        ActionButtonRes(
            drawableResId = iconsR.drawable.mozac_ic_arrow_clockwise_24,
            contentDescription = android.R.string.untitled,
            onClick = object : BrowserToolbarEvent {},
        ),
    )
    val browserActionsEnd = listOf(
        ActionButtonRes(
            drawableResId = iconsR.drawable.mozac_ic_ellipsis_vertical_24,
            contentDescription = android.R.string.untitled,
            onClick = object : BrowserToolbarEvent {},
        ),
    )
    val title = "Firefox"
    val url = "mozilla.com/firefox"

    override val values = sequenceOf(
        DisplayToolbarPreviewModel(
            browserStartActions = browserStartActions,
            pageActionsStart = pageActionsStart,
            title = title,
            url = url,
            progressBarGravity = Top,
            pageActionsEnd = pageActionsEnd,
            browserEndActions = browserActionsEnd,
        ),
        DisplayToolbarPreviewModel(
            browserStartActions = emptyList(),
            pageActionsStart = pageActionsStart,
            title = null,
            url = url,
            progressBarGravity = Bottom,
            pageActionsEnd = pageActionsEnd,
            browserEndActions = emptyList(),
        ),
        DisplayToolbarPreviewModel(
            browserStartActions = browserStartActions,
            pageActionsStart = emptyList(),
            title = title,
            url = url,
            progressBarGravity = Top,
            pageActionsEnd = emptyList(),
            browserEndActions = browserActionsEnd,
        ),
        DisplayToolbarPreviewModel(
            browserStartActions = emptyList(),
            pageActionsStart = emptyList(),
            title = null,
            url = null,
            progressBarGravity = Bottom,
            pageActionsEnd = emptyList(),
            browserEndActions = emptyList(),
        ),
    )
}
