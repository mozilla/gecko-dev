/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import android.content.res.Configuration.UI_MODE_NIGHT_NO
import android.content.res.Configuration.UI_MODE_NIGHT_UNDEFINED
import android.content.res.Configuration.UI_MODE_NIGHT_YES
import android.graphics.Color
import mozilla.components.browser.state.state.ColorSchemeParams
import mozilla.components.browser.state.state.ColorSchemes
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.store.BrowserScreenStore
import org.mozilla.fenix.browser.store.CustomTabColors
import org.mozilla.fenix.customtabs.ext.updateCustomTabsColors
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CustomTabColorsDelegateTest {
    private lateinit var browserScreenStore: BrowserScreenStore
    private lateinit var customTab: CustomTabSessionState

    private val defaultColorSchemeParams = ColorSchemeParams(
        toolbarColor = Color.CYAN,
        navigationBarColor = Color.WHITE,
        navigationBarDividerColor = Color.MAGENTA,
    )

    private val lightColorSchemeParams = ColorSchemeParams(
        toolbarColor = Color.BLACK,
        navigationBarColor = Color.BLUE,
        navigationBarDividerColor = Color.YELLOW,
    )

    private val darkColorSchemeParams = ColorSchemeParams(
        toolbarColor = Color.DKGRAY,
        navigationBarColor = Color.GRAY,
        navigationBarDividerColor = Color.WHITE,
    )

    @Test
    fun `GIVEN no configured colors WHEN computing the color scheme THEN return null`() {
        updateCustomColorsConfiguration()

        assertNull(browserScreenStore.state.customTabColors)
    }

    @Test
    fun `GIVEN only default colors scheme configured WHEN computing the color scheme THEN use the default`() {
        updateCustomColorsConfiguration(
            ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
            ),
        )

        assertEquals(
            CustomTabColors(
                defaultColorSchemeParams.toolbarColor,
                defaultColorSchemeParams.navigationBarColor,
                defaultColorSchemeParams.navigationBarDividerColor,
                Color.BLACK,
            ),
            browserScreenStore.state.customTabColors,
        )
    }

    @Test
    fun `GIVEN following light system theme WHEN computing the color scheme THEN use the light color scheme`() {
        updateCustomColorsConfiguration(
            customTabColorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
            deviceUiMode = UI_MODE_NIGHT_NO,
            shouldFollowDeviceTheme = true,
            shouldUseLightTheme = true,
        )

        assertEquals(
            CustomTabColors(
                lightColorSchemeParams.toolbarColor,
                lightColorSchemeParams.navigationBarColor,
                lightColorSchemeParams.navigationBarDividerColor,
                Color.WHITE,
            ),
            browserScreenStore.state.customTabColors,
        )
    }

    @Test
    fun `GIVEN following light system theme but no light color scheme configured WHEN computing the color scheme THEN use the default color scheme`() {
        updateCustomColorsConfiguration(
            customTabColorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
            deviceUiMode = UI_MODE_NIGHT_NO,
            shouldFollowDeviceTheme = true,
            shouldUseLightTheme = true,
        )

        assertEquals(
            CustomTabColors(
                defaultColorSchemeParams.toolbarColor,
                defaultColorSchemeParams.navigationBarColor,
                defaultColorSchemeParams.navigationBarDividerColor,
                Color.BLACK,
            ),
            browserScreenStore.state.customTabColors,
        )
    }

    @Test
    fun `GIVEN following dark system theme WHEN computing the color scheme THEN use the dark color scheme`() {
        updateCustomColorsConfiguration(
            customTabColorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
            deviceUiMode = UI_MODE_NIGHT_YES,
            shouldFollowDeviceTheme = true,
            shouldUseLightTheme = false,
        )

        assertEquals(
            CustomTabColors(
                darkColorSchemeParams.toolbarColor,
                darkColorSchemeParams.navigationBarColor,
                darkColorSchemeParams.navigationBarDividerColor,
                Color.WHITE,
            ),
            browserScreenStore.state.customTabColors,
        )
    }

    @Test
    fun `GIVEN following dark system theme but no dark color scheme configured WHEN computing the color scheme THEN use the default color scheme`() {
        updateCustomColorsConfiguration(
            customTabColorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
            ),
            deviceUiMode = UI_MODE_NIGHT_YES,
            shouldFollowDeviceTheme = true,
            shouldUseLightTheme = false,
        )

        assertEquals(
            CustomTabColors(
                defaultColorSchemeParams.toolbarColor,
                defaultColorSchemeParams.navigationBarColor,
                defaultColorSchemeParams.navigationBarDividerColor,
                Color.BLACK,
            ),
            browserScreenStore.state.customTabColors,
        )
    }

    @Test
    fun `GIVEN device light theme WHEN computing the color scheme THEN use the light color scheme`() {
        updateCustomColorsConfiguration(
            customTabColorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
            shouldFollowDeviceTheme = false,
            shouldUseLightTheme = true,
        )

        assertEquals(
            CustomTabColors(
                lightColorSchemeParams.toolbarColor,
                lightColorSchemeParams.navigationBarColor,
                lightColorSchemeParams.navigationBarDividerColor,
                Color.WHITE,
            ),
            browserScreenStore.state.customTabColors,
        )
    }

    @Test
    fun `GIVEN device dark theme WHEN computing the color scheme THEN use the dark color scheme`() {
        updateCustomColorsConfiguration(
            customTabColorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
            shouldFollowDeviceTheme = false,
            shouldUseLightTheme = false,
        )

        assertEquals(
            CustomTabColors(
                darkColorSchemeParams.toolbarColor,
                darkColorSchemeParams.navigationBarColor,
                darkColorSchemeParams.navigationBarDividerColor,
                Color.WHITE,
            ),
            browserScreenStore.state.customTabColors,
        )
    }

    private fun updateCustomColorsConfiguration(
        customTabColorSchemes: ColorSchemes? = null,
        deviceUiMode: Int = UI_MODE_NIGHT_UNDEFINED,
        shouldFollowDeviceTheme: Boolean = true,
        shouldUseLightTheme: Boolean = false,
    ) {
        customTab = createCustomTab(
            "https://www.mozilla.org",
            id = "mozilla",
            config = CustomTabConfig(
                colorSchemes = customTabColorSchemes,
            ),
        )
        browserScreenStore = BrowserScreenStore()
        browserScreenStore.updateCustomTabsColors(
            context = testContext,
            customTab = customTab,
            deviceUIMode = deviceUiMode,
            shouldFollowDeviceTheme = shouldFollowDeviceTheme,
            shouldUseLightTheme = shouldUseLightTheme,
        )
    }
}
