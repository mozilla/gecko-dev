/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs.ext

import android.content.Context
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_NO
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_YES
import androidx.appcompat.app.AppCompatDelegate.NightMode
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.feature.customtabs.getConfiguredColorSchemeParams
import mozilla.components.feature.customtabs.getToolbarContrastColor
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.browser.store.BrowserScreenAction.CustomTabColorsUpdated
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.browser.store.CustomTabColors

/**
 * Update [BrowserScreenStore] with the custom colors configuration from [customTab].
 *
 * @param context [Context] used for various system interactions.
 * @param customTab [CustomTabSessionState] from where the a custom colors configuration will be extracted.
 * @param deviceUIMode [NightMode] of the device.
 * @param shouldFollowDeviceTheme Whether to follow the device theme instead of the application theme.
 * @param shouldUseLightTheme Whether to use the light theme or the dark theme when not following the device theme.
 */
fun BrowserScreenStore.updateCustomTabsColors(
    context: Context,
    customTab: CustomTabSessionState?,
    @NightMode deviceUIMode: Int,
    shouldFollowDeviceTheme: Boolean,
    shouldUseLightTheme: Boolean,
) {
    if (customTab == null) {
        dispatch(CustomTabColorsUpdated(null))
        return
    }

    val colorSchemeParams = customTab.config.getConfiguredColorSchemeParams(
        currentNightMode = deviceUIMode,
        preferredNightMode = when (shouldFollowDeviceTheme) {
            true -> MODE_NIGHT_FOLLOW_SYSTEM
            false -> {
                when (shouldUseLightTheme) {
                    true -> MODE_NIGHT_NO
                    false -> MODE_NIGHT_YES
                }
            }
        },
    )
    if (colorSchemeParams == null) {
        dispatch(CustomTabColorsUpdated(null))
        return
    }

    val readableColor = colorSchemeParams.getToolbarContrastColor(
        context = context,
        shouldUpdateTheme = true,
        fallbackColor = Color(context.getColorFromAttr(android.R.attr.textColorPrimary)).toArgb(),
    )

    dispatch(
        CustomTabColorsUpdated(
            CustomTabColors(
                toolbarColor = colorSchemeParams.toolbarColor,
                systemBarsColor = colorSchemeParams.navigationBarColor,
                navigationBarDividerColor = colorSchemeParams.navigationBarDividerColor,
                readableColor = readableColor,
            ),
        ),
    )
}
