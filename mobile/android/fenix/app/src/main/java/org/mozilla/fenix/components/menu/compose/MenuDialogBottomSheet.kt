/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.rememberNestedScrollInteropConnection
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.compose.cfr.CFRPopup
import mozilla.components.compose.cfr.CFRPopupLayout
import mozilla.components.compose.cfr.CFRPopupProperties
import org.mozilla.fenix.R
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.OrientationMode
import org.mozilla.fenix.compose.BottomSheetHandle
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.FirefoxTheme

private const val BOTTOM_SHEET_HANDLE_WIDTH_PERCENT = 0.1f
private const val CFR_HORIZONTAL_OFFSET = 160
private const val CFR_VERTICAL_OFFSET_LANDSCAPE = 0
private const val CFR_VERTICAL_OFFSET_PORTRAIT = -6

/**
 * The menu dialog bottom sheet.
 *
 * @param handlebarContentDescription Bottom sheet handlebar content description.
 * @param onRequestDismiss Invoked when when accessibility services or UI automation requests
 * dismissal of the bottom sheet.
 * @param appStore The [AppStore] needed to determine device orientation.
 * @param context The context needed for settings.
 * @param content The children composable to be laid out.
 */
@Composable
fun MenuDialogBottomSheet(
    handlebarContentDescription: String,
    onRequestDismiss: () -> Unit,
    appStore: AppStore,
    context: Context,
    content: @Composable () -> Unit,
) {
    Column(
        modifier = Modifier
            .background(
                color = FirefoxTheme.colors.layer3,
                shape = RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp),
            )
            .nestedScroll(rememberNestedScrollInteropConnection()),
    ) {
        val (indicatorDirection, verticalOffset) = when (appStore.state.orientation) {
            OrientationMode.Landscape -> CFRPopup.IndicatorDirection.UP to CFR_VERTICAL_OFFSET_LANDSCAPE
            else -> CFRPopup.IndicatorDirection.DOWN to CFR_VERTICAL_OFFSET_PORTRAIT
        }

        CFRPopupLayout(
            showCFR = context.settings().shouldShowMenuCFR,
            properties = CFRPopupProperties(
                popupAlignment = CFRPopup.PopupAlignment.INDICATOR_CENTERED_IN_ANCHOR,
                popupBodyColors = listOf(
                    FirefoxTheme.colors.layerGradientEnd.toArgb(),
                    FirefoxTheme.colors.layerGradientStart.toArgb(),
                ),
                dismissButtonColor = FirefoxTheme.colors.iconOnColor.toArgb(),
                indicatorDirection = indicatorDirection,
                popupVerticalOffset = verticalOffset.dp,
                indicatorArrowStartOffset = CFR_HORIZONTAL_OFFSET.dp,
            ),
            onCFRShown = {
                context.settings().shouldShowMenuCFR = false
                context.settings().lastCfrShownTimeInMillis = System.currentTimeMillis()
            },
            onDismiss = {},
            title = {
                FirefoxTheme {
                    Text(
                        text = stringResource(R.string.menu_cfr_title),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        style = FirefoxTheme.typography.subtitle2,
                    )
                }
            },
            text = {
                FirefoxTheme {
                    Text(
                        text = stringResource(R.string.menu_cfr_body),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        style = FirefoxTheme.typography.body2,
                    )
                }
            },
        ) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .wrapContentSize(Alignment.Center),
            ) {
                BottomSheetHandle(
                    onRequestDismiss = onRequestDismiss,
                    contentDescription = handlebarContentDescription,
                    modifier = Modifier
                        .padding(top = 8.dp, bottom = 5.dp)
                        .fillMaxWidth(BOTTOM_SHEET_HANDLE_WIDTH_PERCENT)
                        .verticalScroll(rememberScrollState()),
                    color = FirefoxTheme.colors.borderInverted,
                )
            }
        }

        content()
    }
}
