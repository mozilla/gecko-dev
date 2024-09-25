/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.PendingIntent
import android.content.Intent
import android.content.res.Configuration
import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.Color
import android.os.Binder
import android.os.Build
import android.os.Bundle
import android.util.SparseArray
import androidx.appcompat.app.AppCompatDelegate
import androidx.browser.customtabs.CustomTabColorSchemeParams
import androidx.browser.customtabs.CustomTabsIntent
import androidx.browser.customtabs.TrustedWebUtils
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuCheckbox
import mozilla.components.browser.menu.item.BrowserMenuDivider
import mozilla.components.browser.menu.item.BrowserMenuSwitch
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ColorSchemeParams
import mozilla.components.browser.state.state.ColorSchemes
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.utils.toSafeIntent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.`when`
import org.robolectric.annotation.Config

@RunWith(AndroidJUnit4::class)
class CustomTabConfigHelperTest {

    private lateinit var resources: Resources

    @Before
    fun setup() {
        resources = spy(testContext.resources)
        doReturn(24f).`when`(resources).getDimension(R.dimen.mozac_feature_customtabs_max_close_button_size)
    }

    @Test
    fun isCustomTabIntent() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        assertTrue(isCustomTabIntent(customTabsIntent.intent))
        assertFalse(isCustomTabIntent(mock<Intent>()))
    }

    @Test
    fun isTrustedWebActivityIntent() {
        val customTabsIntent = CustomTabsIntent.Builder().build().intent
        val trustedWebActivityIntent = Intent(customTabsIntent)
            .putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true)
        assertTrue(isTrustedWebActivityIntent(trustedWebActivityIntent))
        assertFalse(isTrustedWebActivityIntent(customTabsIntent))
        assertFalse(isTrustedWebActivityIntent(mock<Intent>()))
        assertFalse(
            isTrustedWebActivityIntent(
                Intent().putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true),
            ),
        )
    }

    @Test
    fun createFromIntentNoColorScheme() {
        val customTabsIntent = CustomTabsIntent.Builder().build()

        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(null, result.colorScheme)
    }

    @Test
    fun createFromIntentWithColorScheme() {
        val colorScheme = CustomTabsIntent.COLOR_SCHEME_SYSTEM
        val customTabsIntent = CustomTabsIntent.Builder().setColorScheme(colorScheme).build()

        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(colorScheme, result.colorScheme)
    }

    @Test
    fun createFromIntentNoColorSchemeParams() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(null, result.colorSchemes)
    }

    @Test
    fun createFromIntentWithDefaultColorSchemeParams() {
        val colorSchemeParams = createColorSchemeParams()
        val customTabsIntent = CustomTabsIntent.Builder().setDefaultColorSchemeParams(
            createCustomTabColorSchemeParamsFrom(colorSchemeParams),
        ).build()

        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(colorSchemeParams, result.colorSchemes!!.defaultColorSchemeParams)
    }

    @Test
    fun createFromIntentWithDefaultColorSchemeParamsWithNoProperties() {
        val customTabsIntent = CustomTabsIntent.Builder().setDefaultColorSchemeParams(
            CustomTabColorSchemeParams.Builder().build(),
        ).build()

        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(null, result.colorSchemes?.defaultColorSchemeParams)
    }

    @Test
    fun createFromIntentWithLightColorSchemeParams() {
        val colorSchemeParams = createColorSchemeParams()
        val customTabsIntent = CustomTabsIntent.Builder().setColorSchemeParams(
            CustomTabsIntent.COLOR_SCHEME_LIGHT,
            createCustomTabColorSchemeParamsFrom(colorSchemeParams),
        ).build()

        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(colorSchemeParams, result.colorSchemes!!.lightColorSchemeParams)
    }

    @Test
    fun createFromIntentWithLightColorSchemeParamsWithNoProperties() {
        val customTabsIntent = CustomTabsIntent.Builder().setColorSchemeParams(
            CustomTabsIntent.COLOR_SCHEME_LIGHT,
            CustomTabColorSchemeParams.Builder().build(),
        ).build()

        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(null, result.colorSchemes?.lightColorSchemeParams)
    }

    @Test
    fun createFromIntentWithDarkColorSchemeParams() {
        val colorSchemeParams = createColorSchemeParams()
        val customTabsIntent = CustomTabsIntent.Builder().setColorSchemeParams(
            CustomTabsIntent.COLOR_SCHEME_DARK,
            createCustomTabColorSchemeParamsFrom(colorSchemeParams),
        ).build()

        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(colorSchemeParams, result.colorSchemes!!.darkColorSchemeParams)
    }

    @Test
    fun createFromIntentWithDarkColorSchemeParamsWithNoProperties() {
        val customTabsIntent = CustomTabsIntent.Builder().setColorSchemeParams(
            CustomTabsIntent.COLOR_SCHEME_DARK,
            CustomTabColorSchemeParams.Builder().build(),
        ).build()

        val result = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertEquals(null, result.colorSchemes?.lightColorSchemeParams)
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.TIRAMISU])
    fun getColorSchemeParamsBundleOnAndroidVersionTiramisu() {
        val colorScheme = CustomTabsIntent.COLOR_SCHEME_DARK
        val colorSchemeParams = createColorSchemeParams()
        val customTabColorScheme = createCustomTabColorSchemeParamsFrom(colorSchemeParams)
        val customTabsIntent = CustomTabsIntent.Builder().setColorSchemeParams(
            colorScheme,
            customTabColorScheme,
        ).build()

        val result = customTabsIntent.intent.toSafeIntent().getColorSchemeParamsBundle()!!
        val expected = SparseArray<Bundle>()
        expected.put(colorScheme, createBundleFrom(customTabColorScheme))

        result[colorScheme].assertEquals(expected[colorScheme])
    }

    @Test
    @Config(sdk = [Build.VERSION_CODES.S_V2])
    fun getColorSchemeParamsBundlePreAndroidVersionTiramisu() {
        val colorScheme = CustomTabsIntent.COLOR_SCHEME_DARK
        val colorSchemeParams = createColorSchemeParams()
        val customTabColorScheme = createCustomTabColorSchemeParamsFrom(colorSchemeParams)
        val customTabsIntent = CustomTabsIntent.Builder().setColorSchemeParams(
            colorScheme,
            customTabColorScheme,
        ).build()

        val result = customTabsIntent.intent.toSafeIntent().getColorSchemeParamsBundle()!!
        val expected = SparseArray<Bundle>()
        expected.put(colorScheme, createBundleFrom(customTabColorScheme))

        result[colorScheme].assertEquals(expected[colorScheme])
    }

    @Test
    fun createFromIntentWithCloseButton() {
        val size = 24
        val builder = CustomTabsIntent.Builder()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertEquals(closeButtonIcon, customTabConfig.closeButtonIcon)
        assertEquals(size, customTabConfig.closeButtonIcon?.width)
        assertEquals(size, customTabConfig.closeButtonIcon?.height)

        val customTabConfigNoResources = createCustomTabConfigFromIntent(builder.build().intent, null)
        assertEquals(closeButtonIcon, customTabConfigNoResources.closeButtonIcon)
        assertEquals(size, customTabConfigNoResources.closeButtonIcon?.width)
        assertEquals(size, customTabConfigNoResources.closeButtonIcon?.height)
    }

    @Test
    fun createFromIntentWithMaxOversizedCloseButton() {
        val size = 64
        val builder = CustomTabsIntent.Builder()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertNull(customTabConfig.closeButtonIcon)

        val customTabConfigNoResources = createCustomTabConfigFromIntent(builder.build().intent, null)
        assertEquals(closeButtonIcon, customTabConfigNoResources.closeButtonIcon)
    }

    @Test
    fun createFromIntentUsingDisplayMetricsForCloseButton() {
        val size = 64
        val builder = CustomTabsIntent.Builder()
        val resources: Resources = mock()
        val closeButtonIcon = Bitmap.createBitmap(IntArray(size * size), size, size, Bitmap.Config.ARGB_8888)
        builder.setCloseButtonIcon(closeButtonIcon)

        `when`(resources.getDimension(R.dimen.mozac_feature_customtabs_max_close_button_size)).thenReturn(64f)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, resources)
        assertEquals(closeButtonIcon, customTabConfig.closeButtonIcon)
    }

    @Test
    fun createFromIntentWithInvalidCloseButton() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        // Intent is a parcelable but not a Bitmap
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_CLOSE_BUTTON_ICON, Intent())

        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)
        assertNull(customTabConfig.closeButtonIcon)
    }

    @Test
    fun createFromIntentWithUrlbarHiding() {
        val builder = CustomTabsIntent.Builder()
        builder.setUrlBarHidingEnabled(true)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertTrue(customTabConfig.enableUrlbarHiding)
    }

    @Test
    fun createFromIntentWithShareMenuItem() {
        val builder = CustomTabsIntent.Builder()
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertTrue(customTabConfig.showShareMenuItem)
    }

    @Test
    fun createFromIntentWithShareState() {
        val builder = CustomTabsIntent.Builder()
        builder.setShareState(CustomTabsIntent.SHARE_STATE_ON)

        val extraShareState = builder.build().intent.getIntExtra(CustomTabsIntent.EXTRA_SHARE_STATE, 5)
        assertEquals(CustomTabsIntent.SHARE_STATE_ON, extraShareState)
    }

    @Test
    fun createFromIntentWithCustomizedMenu() {
        val builder = CustomTabsIntent.Builder()
        val pendingIntent = PendingIntent.getActivity(null, 0, null, 0)
        builder.addMenuItem("menuitem1", pendingIntent)
        builder.addMenuItem("menuitem2", pendingIntent)

        val customTabConfig = createCustomTabConfigFromIntent(builder.build().intent, testContext.resources)
        assertEquals(2, customTabConfig.menuItems.size)
        assertEquals("menuitem1", customTabConfig.menuItems[0].name)
        assertSame(pendingIntent, customTabConfig.menuItems[0].pendingIntent)
        assertEquals("menuitem2", customTabConfig.menuItems[1].name)
        assertSame(pendingIntent, customTabConfig.menuItems[1].pendingIntent)
    }

    @Test
    fun createFromIntentWithActionButton() {
        val builder = CustomTabsIntent.Builder()

        val bitmap = mock<Bitmap>()
        val intent = PendingIntent.getActivity(testContext, 0, Intent("testAction"), 0)
        builder.setActionButton(bitmap, "desc", intent)

        val customTabsIntent = builder.build()
        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertNotNull(customTabConfig.actionButtonConfig)
        assertEquals("desc", customTabConfig.actionButtonConfig?.description)
        assertEquals(intent, customTabConfig.actionButtonConfig?.pendingIntent)
        assertEquals(bitmap, customTabConfig.actionButtonConfig?.icon)
        assertFalse(customTabConfig.actionButtonConfig!!.tint)
    }

    @Test
    fun createFromIntentWithInvalidActionButton() {
        val customTabsIntent = CustomTabsIntent.Builder().build()

        val invalid = Bundle()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_ACTION_BUTTON_BUNDLE, invalid)
        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)

        assertNull(customTabConfig.actionButtonConfig)
    }

    @Test
    fun createFromIntentWithInvalidExtras() {
        val customTabsIntent = CustomTabsIntent.Builder().build()

        val extrasField = Intent::class.java.getDeclaredField("mExtras")
        extrasField.isAccessible = true
        extrasField.set(customTabsIntent.intent, null)
        extrasField.isAccessible = false

        assertFalse(isCustomTabIntent(customTabsIntent.intent))

        // Make sure we're not failing
        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)
        assertNotNull(customTabConfig)
        assertNull(customTabConfig.actionButtonConfig)
    }

    @Test
    fun createFromIntentWithExitAnimationOption() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        val bundle = Bundle()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_EXIT_ANIMATION_BUNDLE, bundle)

        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)
        assertEquals(bundle, customTabConfig.exitAnimations)
    }

    @Test
    fun createFromIntentWithPageTitleOption() {
        val customTabsIntent = CustomTabsIntent.Builder().build()
        customTabsIntent.intent.putExtra(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE)

        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent.intent, testContext.resources)
        assertTrue(customTabConfig.titleVisible)
    }

    @Test
    fun createFromIntentWithSessionToken() {
        val customTabsIntent: Intent = mock()
        val bundle: Bundle = mock()
        val binder: Binder = mock()
        `when`(customTabsIntent.extras).thenReturn(bundle)
        `when`(bundle.getBinder(CustomTabsIntent.EXTRA_SESSION)).thenReturn(binder)

        val customTabConfig = createCustomTabConfigFromIntent(customTabsIntent, testContext.resources)
        assertNotNull(customTabConfig.sessionToken)
    }

    @Test
    fun `GIVEN a custom tab has custom menu items it wants to show WHEN creating the menu builder THEN include the custom menu items to be shown`() {
        val defaultItems = listOf(
            BrowserMenuCheckbox("item1") {},
            BrowserMenuDivider(),
            BrowserMenuSwitch("item3") {},
        )
        val defaultExtras = mapOf("default" to "extras")
        val customMenuItems = listOf(
            CustomTabMenuItem("customItem1", mock()),
            CustomTabMenuItem("customItem2", mock()),
        )
        val defaultMenuBuilder = BrowserMenuBuilder(defaultItems, defaultExtras)
        val customTab = CustomTabSessionState(
            id = "customTabId",
            config = CustomTabConfig(
                menuItems = customMenuItems,
            ),
            content = ContentState("http://test.com"),
        )
        val browserStore = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    mock<TabSessionState>(),
                ),
                customTabs = listOf(customTab, mock()),
            ),
        )

        val customTabMenu = defaultMenuBuilder.addCustomMenuItems(
            context = testContext,
            browserStore = browserStore,
            customTabSessionId = customTab.id,
            customTabMenuInsertIndex = 1,
        )

        assertEquals(5, customTabMenu!!.items.size)
        assertEquals(defaultItems[0], customTabMenu.items[0])
        assertTrue(customTabMenu.items[1] is SimpleBrowserMenuItem)
        assertTrue(customTabMenu.items[2] is SimpleBrowserMenuItem)
        assertEquals(defaultItems[1], customTabMenu.items[3])
        assertEquals(defaultItems[2], customTabMenu.items[4])
    }

    @Test
    fun `WHEN COLOR_SCHEME_SYSTEM THEN toNightMode returns MODE_NIGHT_FOLLOW_SYSTEM`() {
        assertEquals(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM, CustomTabsIntent.COLOR_SCHEME_SYSTEM.toNightMode())
    }

    @Test
    fun `WHEN COLOR_SCHEME_LIGHT THEN toNightMode returns MODE_NIGHT_NO`() {
        assertEquals(AppCompatDelegate.MODE_NIGHT_NO, CustomTabsIntent.COLOR_SCHEME_LIGHT.toNightMode())
    }

    @Test
    fun `WHEN COLOR_SCHEME_DARK THEN toNightMode returns MODE_NIGHT_YES`() {
        assertEquals(AppCompatDelegate.MODE_NIGHT_YES, CustomTabsIntent.COLOR_SCHEME_DARK.toNightMode())
    }

    @Test
    fun `WHEN unknown color scheme THEN toNightMode returns null`() {
        assertEquals(null, 100.toNightMode())
    }

    @Test
    fun `WHEN no color scheme params set THEN getConfiguredColorSchemeParams returns null `() {
        val customTabConfig = CustomTabConfig()

        assertEquals(
            null,
            customTabConfig.getConfiguredColorSchemeParams(currentNightMode = Configuration.UI_MODE_NIGHT_UNDEFINED),
        )
    }

    @Test
    fun `WHEN only default color scheme params set THEN getConfiguredColorSchemeParams returns default `() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
            ),
        )

        assertEquals(
            defaultColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(currentNightMode = Configuration.UI_MODE_NIGHT_UNDEFINED),
        )
    }

    @Test
    fun `WHEN night mode follow system and is light mode THEN getConfiguredColorSchemeParams returns light color scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
        )

        assertEquals(
            lightColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_NO,
                preferredNightMode = AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM,
            ),
        )
    }

    @Test
    fun `WHEN night mode follow system, is light mode no light color scheme THEN getConfiguredColorSchemeParams returns default scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
        )

        assertEquals(
            defaultColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_NO,
                preferredNightMode = AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM,
            ),
        )
    }

    @Test
    fun `WHEN night mode follow system and is dark mode THEN getConfiguredColorSchemeParams returns dark color scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
        )

        assertEquals(
            darkColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_YES,
                preferredNightMode = AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM,
            ),
        )
    }

    @Test
    fun `WHEN night mode follow system, is dark mode no dark color scheme THEN getConfiguredColorSchemeParams returns default scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
            ),
        )

        assertEquals(
            defaultColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_YES,
                preferredNightMode = AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM,
            ),
        )
    }

    @Test
    fun `WHEN night mode no THEN getConfiguredColorSchemeParams returns light color scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
        )

        assertEquals(
            lightColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_UNDEFINED,
                preferredNightMode = AppCompatDelegate.MODE_NIGHT_NO,
            ),
        )
    }

    @Test
    fun `WHEN night mode no & no light color params THEN getConfiguredColorSchemeParams returns default color scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
        )

        assertEquals(
            defaultColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_UNDEFINED,
                preferredNightMode = AppCompatDelegate.MODE_NIGHT_NO,
            ),
        )
    }

    @Test
    fun `WHEN night mode yes THEN getConfiguredColorSchemeParams returns dark color scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
        )

        assertEquals(
            darkColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_UNDEFINED,
                preferredNightMode = AppCompatDelegate.MODE_NIGHT_YES,
            ),
        )
    }

    @Test
    fun `WHEN night mode yes & no dark color params THEN getConfiguredColorSchemeParams returns default color scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
            ),
        )

        assertEquals(
            defaultColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_UNDEFINED,
                preferredNightMode = AppCompatDelegate.MODE_NIGHT_YES,
            ),
        )
    }

    @Test
    fun `WHEN night mode not set THEN getConfiguredColorSchemeParams returns default color scheme`() {
        val customTabConfig = CustomTabConfig(
            colorSchemes = ColorSchemes(
                defaultColorSchemeParams = defaultColorSchemeParams,
                lightColorSchemeParams = lightColorSchemeParams,
                darkColorSchemeParams = darkColorSchemeParams,
            ),
        )

        assertEquals(
            defaultColorSchemeParams,
            customTabConfig.getConfiguredColorSchemeParams(
                currentNightMode = Configuration.UI_MODE_NIGHT_UNDEFINED,
                preferredNightMode = null,
            ),
        )
    }

    @Test
    fun `WHEN ColorSchemeParams has all properties THEN withDefault returns the same ColorSchemeParams`() {
        val result = lightColorSchemeParams.withDefault(defaultColorSchemeParams)

        assertEquals(lightColorSchemeParams, result)
    }

    @Test
    fun `WHEN ColorSchemeParams has some properties THEN withDefault uses default for the missing properties`() {
        val colorSchemeParams = ColorSchemeParams(
            toolbarColor = Color.BLACK,
            navigationBarDividerColor = Color.YELLOW,
        )

        val expected = ColorSchemeParams(
            toolbarColor = colorSchemeParams.toolbarColor,
            secondaryToolbarColor = defaultColorSchemeParams.secondaryToolbarColor,
            navigationBarColor = defaultColorSchemeParams.navigationBarColor,
            navigationBarDividerColor = colorSchemeParams.navigationBarDividerColor,
        )

        val result = colorSchemeParams.withDefault(defaultColorSchemeParams)

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN ColorSchemeParams has some properties and app is in private mode THEN getToolbarBackgroundColor returns null`() {
        val colorSchemeParams = ColorSchemeParams(
            toolbarColor = Color.BLACK,
            navigationBarDividerColor = Color.YELLOW,
        )
        val expected = null

        val result = colorSchemeParams.getToolbarBackgroundColor(false)

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN ColorSchemeParams has some properties and app is in normal mode THEN getToolbarBackgroundColor returns toolbarColor`() {
        val toolbarColor = Color.BLACK
        val colorSchemeParams = ColorSchemeParams(
            toolbarColor = toolbarColor,
            navigationBarDividerColor = Color.YELLOW,
        )

        val result = colorSchemeParams.getToolbarBackgroundColor(true)

        assertEquals(toolbarColor, result)
    }

    @Test
    fun `WHEN ColorSchemeParams has some properties THEN getToolbarContrastColorDisabled returns the correct color`() {
        val colorSchemeParams = ColorSchemeParams(
            toolbarColor = Color.BLACK,
            navigationBarDividerColor = Color.YELLOW,
        )
        val expected = Color.parseColor(LIGHT_GRAY_HEX)

        val result = colorSchemeParams.getToolbarContrastColorDisabled(
            true,
            fallbackColor = Color.WHITE,
        )

        assertEquals(expected, result)
    }

    @Test
    fun `WHEN ColorSchemeParams has no properties THEN withDefault returns all default ColorSchemeParams`() {
        val result = ColorSchemeParams().withDefault(defaultColorSchemeParams)

        assertEquals(defaultColorSchemeParams, result)
    }

    private fun createColorSchemeParams() = ColorSchemeParams(
        toolbarColor = Color.BLACK,
        secondaryToolbarColor = Color.RED,
        navigationBarColor = Color.BLUE,
        navigationBarDividerColor = Color.YELLOW,
    )

    private fun createCustomTabColorSchemeParamsFrom(colorSchemeParams: ColorSchemeParams): CustomTabColorSchemeParams {
        val customTabColorSchemeBuilder = CustomTabColorSchemeParams.Builder()
        customTabColorSchemeBuilder.setToolbarColor(colorSchemeParams.toolbarColor!!)
        customTabColorSchemeBuilder.setSecondaryToolbarColor(colorSchemeParams.secondaryToolbarColor!!)
        customTabColorSchemeBuilder.setNavigationBarColor(colorSchemeParams.navigationBarColor!!)
        customTabColorSchemeBuilder.setNavigationBarDividerColor(colorSchemeParams.navigationBarDividerColor!!)
        return customTabColorSchemeBuilder.build()
    }

    private fun createBundleFrom(customTabColorScheme: CustomTabColorSchemeParams): Bundle {
        val expectedBundle = Bundle()
        expectedBundle.putInt(CustomTabsIntent.EXTRA_TOOLBAR_COLOR, customTabColorScheme.toolbarColor!!)
        expectedBundle.putInt(CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR, customTabColorScheme.secondaryToolbarColor!!)
        expectedBundle.putInt(CustomTabsIntent.EXTRA_NAVIGATION_BAR_COLOR, customTabColorScheme.navigationBarColor!!)
        expectedBundle.putInt(CustomTabsIntent.EXTRA_NAVIGATION_BAR_DIVIDER_COLOR, customTabColorScheme.navigationBarDividerColor!!)
        return expectedBundle
    }

    /**
     * As Bundle does not implement Equals, assert the values individually.
     */
    private fun Bundle.assertEquals(bundle: Bundle) {
        assertEquals(bundle.getInt(CustomTabsIntent.EXTRA_TOOLBAR_COLOR), getInt(CustomTabsIntent.EXTRA_TOOLBAR_COLOR))
        assertEquals(bundle.getInt(CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR), getInt(CustomTabsIntent.EXTRA_SECONDARY_TOOLBAR_COLOR))
        assertEquals(bundle.getInt(CustomTabsIntent.EXTRA_NAVIGATION_BAR_COLOR), getInt(CustomTabsIntent.EXTRA_NAVIGATION_BAR_COLOR))
        assertEquals(bundle.getInt(CustomTabsIntent.EXTRA_NAVIGATION_BAR_DIVIDER_COLOR), getInt(CustomTabsIntent.EXTRA_NAVIGATION_BAR_DIVIDER_COLOR))
    }

    private val defaultColorSchemeParams = ColorSchemeParams(
        toolbarColor = Color.CYAN,
        secondaryToolbarColor = Color.GREEN,
        navigationBarColor = Color.WHITE,
        navigationBarDividerColor = Color.MAGENTA,
    )

    private val lightColorSchemeParams = ColorSchemeParams(
        toolbarColor = Color.BLACK,
        secondaryToolbarColor = Color.RED,
        navigationBarColor = Color.BLUE,
        navigationBarDividerColor = Color.YELLOW,
    )

    private val darkColorSchemeParams = ColorSchemeParams(
        toolbarColor = Color.DKGRAY,
        secondaryToolbarColor = Color.LTGRAY,
        navigationBarColor = Color.GRAY,
        navigationBarDividerColor = Color.WHITE,
    )
}
