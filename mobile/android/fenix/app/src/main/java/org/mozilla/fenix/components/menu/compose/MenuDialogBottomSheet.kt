/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.annotation.StringRes
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
import org.mozilla.fenix.components.appstate.OrientationMode
import org.mozilla.fenix.compose.BottomSheetHandle
import org.mozilla.fenix.theme.FirefoxTheme

private const val BOTTOM_SHEET_HANDLE_WIDTH_PERCENT = 0.1f
private const val CFR_HORIZONTAL_OFFSET = 160
private const val CFR_VERTICAL_OFFSET_LANDSCAPE = 0
private const val CFR_VERTICAL_OFFSET_PORTRAIT = -6

/**
 * The menu dialog bottom sheet.
 *
 * @param onRequestDismiss Invoked when when accessibility services or UI automation requests
 * dismissal of the bottom sheet.
 * @param handlebarContentDescription Bottom sheet handlebar content description.
 * @param menuCfrState An optional [MenuCFRState] that describes how to display a
 * contextual feature recommendation (CFR) popup in the menu.
 * @param content The children composable to be laid out.
 */
@Composable
fun MenuDialogBottomSheet(
    onRequestDismiss: () -> Unit,
    handlebarContentDescription: String,
    menuCfrState: MenuCFRState? = null,
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
        if (menuCfrState?.showCFR == true) {
            CFRBottomSheetHandle(
                state = menuCfrState,
                onRequestDismiss = onRequestDismiss,
                contentDescription = handlebarContentDescription,
            )
        } else {
            MenuBottomSheetHandle(
                onRequestDismiss = onRequestDismiss,
                contentDescription = handlebarContentDescription,
            )
        }

        content()
    }
}

@Composable
private fun MenuBottomSheetHandle(
    onRequestDismiss: () -> Unit,
    contentDescription: String,
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .wrapContentSize(Alignment.Center),
    ) {
        BottomSheetHandle(
            onRequestDismiss = onRequestDismiss,
            contentDescription = contentDescription,
            modifier = Modifier
                .padding(top = 8.dp, bottom = 5.dp)
                .fillMaxWidth(BOTTOM_SHEET_HANDLE_WIDTH_PERCENT)
                .verticalScroll(rememberScrollState()),
            color = FirefoxTheme.colors.borderInverted,
        )
    }
}

/**
 * A handle present on top of a bottom sheet that will also serves as a anchor for the CFR.
 */
@Composable
private fun CFRBottomSheetHandle(
    state: MenuCFRState,
    contentDescription: String,
    onRequestDismiss: () -> Unit,
) {
    val (indicatorDirection, verticalOffset) = when (state.orientation) {
        OrientationMode.Landscape -> CFRPopup.IndicatorDirection.UP to CFR_VERTICAL_OFFSET_LANDSCAPE
        else -> CFRPopup.IndicatorDirection.DOWN to CFR_VERTICAL_OFFSET_PORTRAIT
    }

    CFRPopupLayout(
        showCFR = state.showCFR,
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
        onCFRShown = state.onShown,
        onDismiss = state.onDismiss,
        title = {
            FirefoxTheme {
                Text(
                    text = stringResource(id = state.titleRes),
                    color = FirefoxTheme.colors.textOnColorPrimary,
                    style = FirefoxTheme.typography.subtitle2,
                )
            }
        },
        text = {
            FirefoxTheme {
                Text(
                    text = stringResource(id = state.messageRes),
                    color = FirefoxTheme.colors.textOnColorPrimary,
                    style = FirefoxTheme.typography.body2,
                )
            }
        },
    ) {
        MenuBottomSheetHandle(
            onRequestDismiss = onRequestDismiss,
            contentDescription = contentDescription,
        )
    }
}

/**
 * State object that describe the contextual feature recommendation (CFR) popup in the menu.
 *
 * @property showCFR Whether or not to display the CFR.
 * @property titleRes The string resource ID of the title to display in the CFR.
 * @property messageRes The string resource ID of the message to display in the CFR body.
 * @property orientation The [OrientationMode] of the device.
 * @property onShown Invoked when the CFR is shown.
 * @property onDismiss Invoked when the CFR is dismissed. Returns true if the dismissal was
 * explicit (e.g. user clicked on the "X" close button).
 */
data class MenuCFRState(
    val showCFR: Boolean,
    @StringRes val titleRes: Int,
    @StringRes val messageRes: Int,
    val orientation: OrientationMode,
    val onShown: () -> Unit,
    val onDismiss: (Boolean) -> Unit,
)
