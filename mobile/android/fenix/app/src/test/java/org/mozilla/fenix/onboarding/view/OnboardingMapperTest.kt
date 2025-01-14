/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.onboarding.store.OnboardingAddonStatus

class OnboardingMapperTest {

    @Test
    fun `GIVEN a default browser page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_onboarding_welcome,
            title = "default browser title",
            description = "default browser body with link text",
            primaryButton = Action("default browser primary button text", unitLambda),
            secondaryButton = Action("default browser secondary button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.DEFAULT_BROWSER,
            imageRes = R.drawable.ic_onboarding_welcome,
            title = "default browser title",
            description = "default browser body with link text",
            primaryButtonLabel = "default browser primary button text",
            secondaryButtonLabel = "default browser secondary button text",
            privacyCaption = null,
        )
        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = unitLambda,
            onMakeFirefoxDefaultSkipClick = unitLambda,
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
            onAddOnsButtonClick = {},
            onCustomizeToolbarButtonClick = {},
            onCustomizeThemeClick = {},
            onTermsOfServiceButtonClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a sync page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_onboarding_sync,
            title = "sync title",
            description = "sync body",
            primaryButton = Action("sync primary button text", unitLambda),
            secondaryButton = Action("sync secondary button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.SYNC_SIGN_IN,
            imageRes = R.drawable.ic_onboarding_sync,
            title = "sync title",
            description = "sync body",
            primaryButtonLabel = "sync primary button text",
            secondaryButtonLabel = "sync secondary button text",
            privacyCaption = null,
        )
        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = unitLambda,
            onSignInSkipClick = unitLambda,
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
            onAddOnsButtonClick = {},
            onCustomizeToolbarButtonClick = {},
            onCustomizeThemeClick = {},
            onTermsOfServiceButtonClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a notification page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_notification_permission,
            title = "notification title",
            description = "notification body",
            primaryButton = Action("notification primary button text", unitLambda),
            secondaryButton = Action("notification secondary button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.NOTIFICATION_PERMISSION,
            imageRes = R.drawable.ic_notification_permission,
            title = "notification title",
            description = "notification body",
            primaryButtonLabel = "notification primary button text",
            secondaryButtonLabel = "notification secondary button text",
            privacyCaption = null,
        )
        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = unitLambda,
            onNotificationPermissionSkipClick = unitLambda,
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
            onAddOnsButtonClick = {},
            onCustomizeToolbarButtonClick = {},
            onCustomizeThemeClick = {},
            onTermsOfServiceButtonClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN an add search widget page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_onboarding_search_widget,
            title = "add search widget title",
            description = "add search widget body with link text",
            primaryButton = Action("add search widget primary button text", unitLambda),
            secondaryButton = Action("add search widget secondary button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.ADD_SEARCH_WIDGET,
            imageRes = R.drawable.ic_onboarding_search_widget,
            title = "add search widget title",
            description = "add search widget body with link text",
            primaryButtonLabel = "add search widget primary button text",
            secondaryButtonLabel = "add search widget secondary button text",
            privacyCaption = null,
        )
        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = unitLambda,
            onAddFirefoxWidgetSkipClick = unitLambda,
            onAddOnsButtonClick = {},
            onCustomizeToolbarButtonClick = {},
            onCustomizeThemeClick = {},
            onTermsOfServiceButtonClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN an add-ons page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val addOns = listOf(
            OnboardingAddOn(
                id = "add-on-1",
                iconRes = R.drawable.ic_extensions_onboarding,
                name = "test add-on 1",
                description = "test 1 add-on description",
                averageRating = "5",
                reviewCount = "12,345",
                installUrl = "url1",
                status = OnboardingAddonStatus.NOT_INSTALLED,
            ),
            OnboardingAddOn(
                id = "add-on-2",
                iconRes = R.drawable.ic_extensions_onboarding,
                name = "test add-on 2",
                description = "test 2 add-on description",
                averageRating = "4.5",
                reviewCount = "1,234",
                installUrl = "url2",
                status = OnboardingAddonStatus.NOT_INSTALLED,
            ),
            OnboardingAddOn(
                id = "add-on-2",
                iconRes = R.drawable.ic_extensions_onboarding,
                name = "test add-on 3",
                description = "test 3 add-on description",
                averageRating = "4",
                reviewCount = "123",
                installUrl = "url3",
                status = OnboardingAddonStatus.NOT_INSTALLED,
            ),
        )
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_onboarding_add_ons,
            title = "add-ons title",
            description = "add-ons body",
            primaryButton = Action("add-ons primary button text", unitLambda),
            addOns = addOns,
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.ADD_ONS,
            imageRes = R.drawable.ic_onboarding_add_ons,
            title = "add-ons title",
            description = "add-ons body",
            primaryButtonLabel = "add-ons primary button text",
            addOns = addOns,
        )

        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
            onAddOnsButtonClick = unitLambda,
            onCustomizeToolbarButtonClick = {},
            onCustomizeThemeClick = {},
            onTermsOfServiceButtonClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a toolbar placement page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val toolbarOptions = listOf(
            ToolbarOption(
                toolbarType = ToolbarOptionType.TOOLBAR_TOP,
                imageRes = R.drawable.ic_onboarding_top_toolbar,
                label = "Top",
            ),
            ToolbarOption(
                toolbarType = ToolbarOptionType.TOOLBAR_BOTTOM,
                imageRes = R.drawable.ic_onboarding_bottom_toolbar,
                label = "Bottom",
            ),
        )

        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_onboarding_customize_toolbar,
            title = "Pick a toolbar placement",
            description = "Keep searches within reach",
            primaryButton = Action("Save and continue", unitLambda),
            toolbarOptions = toolbarOptions,
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.TOOLBAR_PLACEMENT,
            imageRes = R.drawable.ic_onboarding_customize_toolbar,
            title = "Pick a toolbar placement",
            description = "Keep searches within reach",
            primaryButtonLabel = "Save and continue",
            toolbarOptions = toolbarOptions,
        )

        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
            onAddOnsButtonClick = {},
            onCustomizeToolbarButtonClick = unitLambda,
            onCustomizeThemeClick = {},
            onTermsOfServiceButtonClick = {},
            onMarketingDataContinueClick = {},
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a marketing data collection opt out page WHEN mapToOnboardingPageState is called THEN creates the expected OnboardingPageState`() {
        val expected = OnboardingPageState(
            imageRes = R.drawable.ic_high_five,
            title = "marketing data title",
            description = "marketing data body",
            primaryButton = Action("marketing data button text", unitLambda),
        )

        val onboardingPageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.MARKETING_DATA,
            imageRes = R.drawable.ic_high_five,
            title = "marketing data title",
            description = "marketing data body",
            primaryButtonLabel = "marketing data button text",
        )

        val actual = mapToOnboardingPageState(
            onboardingPageUiData = onboardingPageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
            onAddOnsButtonClick = {},
            onCustomizeToolbarButtonClick = {},
            onCustomizeToolbarSkipClick = {},
            onCustomizeThemeClick = {},
            onCustomizeThemeSkip = {},
            onTermsOfServiceButtonClick = {},
            onMarketingDataContinueClick = unitLambda,
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a customize theme page UI data WHEN mapping function is called THEN equivalent page state is created`() {
        // Page UI values
        val imageRes = R.drawable.ic_pick_a_theme
        val title = "Pick a theme"
        val description = "See the web in the best light."
        val primaryButtonLabel = "Save and continue"

        // Theming options
        val themeOptionSystem = ThemeOption(
            label = "System auto",
            imageRes = R.drawable.ic_pick_a_theme_system_auto,
            themeType = ThemeOptionType.THEME_SYSTEM,
        )
        val themeOptionLight = ThemeOption(
            label = "Light",
            imageRes = R.drawable.ic_pick_a_theme_light,
            themeType = ThemeOptionType.THEME_LIGHT,
        )
        val themeOptionDark = ThemeOption(
            label = "Dark",
            imageRes = R.drawable.ic_pick_a_theme_dark,
            themeType = ThemeOptionType.THEME_DARK,
        )
        val themeOptions = listOf(themeOptionSystem, themeOptionLight, themeOptionDark)

        val pageUiData = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.THEME_SELECTION,
            imageRes = imageRes,
            title = title,
            description = description,
            primaryButtonLabel = primaryButtonLabel,
            themeOptions = themeOptions,
        )

        val expectedPageState = OnboardingPageState(
            imageRes = imageRes,
            title = title,
            description = description,
            primaryButton = Action(primaryButtonLabel, unitLambda),
            themeOptions = themeOptions,
        )

        val actualPageState = mapToOnboardingPageState(
            onboardingPageUiData = pageUiData,
            onMakeFirefoxDefaultClick = {},
            onMakeFirefoxDefaultSkipClick = {},
            onSignInButtonClick = {},
            onSignInSkipClick = {},
            onNotificationPermissionButtonClick = {},
            onNotificationPermissionSkipClick = {},
            onAddFirefoxWidgetClick = {},
            onAddFirefoxWidgetSkipClick = {},
            onAddOnsButtonClick = {},
            onCustomizeToolbarButtonClick = {},
            onCustomizeThemeClick = unitLambda,
            onTermsOfServiceButtonClick = {},
        )

        assertEquals(expectedPageState, actualPageState)
    }
}

private val unitLambda = { dummyUnitFunc() }

private fun dummyUnitFunc() {}
