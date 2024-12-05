/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import io.mockk.every
import io.mockk.mockk
import mozilla.components.service.nimbus.evalJexlSafe
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.experiments.nimbus.NimbusMessagingHelperInterface
import org.mozilla.experiments.nimbus.StringHolder
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.HomeActivityIntentTestRule
import org.mozilla.fenix.nimbus.AddOnData
import org.mozilla.fenix.nimbus.CustomizationThemeData
import org.mozilla.fenix.nimbus.ExtraCardData
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.nimbus.JunoOnboarding
import org.mozilla.fenix.nimbus.OnboardingCardData
import org.mozilla.fenix.nimbus.OnboardingCardType
import org.mozilla.fenix.nimbus.TermsOfServiceData
import org.mozilla.fenix.nimbus.ThemeType
import org.mozilla.fenix.onboarding.store.OnboardingAddonStatus

class OnboardingMapperTest {

    @get:Rule
    val activityTestRule =
        HomeActivityIntentTestRule.withDefaultSettingsOverrides(skipOnboarding = true)

    private lateinit var junoOnboardingFeature: JunoOnboarding
    private lateinit var jexlConditions: Map<String, String>
    private lateinit var jexlHelper: NimbusMessagingHelperInterface
    private lateinit var evalFunction: (String) -> Boolean

    @Before
    fun setup() {
        junoOnboardingFeature = FxNimbus.features.junoOnboarding.value()
        jexlConditions = junoOnboardingFeature.conditions

        jexlHelper = mockk(relaxed = true)
        evalFunction = { condition -> jexlHelper.evalJexlSafe(condition) }

        every { evalFunction("true") } returns true
        every { evalFunction("false") } returns false
    }

    @Test
    fun showNotificationTrue_showAddWidgetFalse_pagesToDisplay_returnsSortedListOfAllConvertedPages_withoutAddWidgetPage() {
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption, syncPageUiData, notificationPageUiData)
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = true,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun showNotificationFalse_showAddWidgetFalse_pagesToDisplay_returnsSortedListOfConvertedPages_withoutNotificationPage_and_addWidgetPage() {
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption, syncPageUiData)
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun pagesToDisplay_returnsSortedListOfConvertedPages_withPrivacyCaption_alwaysOnFirstPage() {
        var result = unsortedAllKnownCardData.toPageUiData(
            privacyCaption = privacyCaption,
            showDefaultBrowserPage = false,
            showNotificationPage = false,
            showAddWidgetPage = false,
            jexlConditions = jexlConditions,
            func = evalFunction,
        )
        assertEquals(result[0].privacyCaption, privacyCaption)

        result = unsortedAllKnownCardData.toPageUiData(
            privacyCaption = privacyCaption,
            showDefaultBrowserPage = false,
            showNotificationPage = true,
            showAddWidgetPage = false,
            jexlConditions = jexlConditions,
            func = evalFunction,
        )
        assertEquals(result[0].privacyCaption, privacyCaption)
        assertEquals(result[1].privacyCaption, null)

        result = unsortedAllKnownCardData.toPageUiData(
            privacyCaption = privacyCaption,
            showDefaultBrowserPage = true,
            showNotificationPage = true,
            showAddWidgetPage = false,
            jexlConditions = jexlConditions,
            func = evalFunction,
        )
        assertEquals(result[0].privacyCaption, privacyCaption)
        assertEquals(result[1].privacyCaption, null)
        assertEquals(result[2].privacyCaption, null)

        result = unsortedAllKnownCardData.toPageUiData(
            privacyCaption = privacyCaption,
            showDefaultBrowserPage = false,
            showNotificationPage = false,
            showAddWidgetPage = true,
            jexlConditions = jexlConditions,
            func = evalFunction,
        )
        assertEquals(result[0].privacyCaption, privacyCaption)
        assertEquals(result[1].privacyCaption, null)
    }

    @Test
    fun showDefaultBrowserPageFalse_showNotificationFalse_showAddWidgetTrue_pagesToDisplay_returnsSortedListOfAllConvertedPages() {
        val expected = listOf(addSearchWidgetPageUiDataWithPrivacyCaption, syncPageUiData)
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = false,
                showNotificationPage = false,
                showAddWidgetPage = true,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun showNotificationFalse_showAddWidgetTrue_pagesToDisplay_returnsSortedListOfAllConvertedPages_withoutNotificationPage() {
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption, addSearchWidgetPageUiData, syncPageUiData)
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = true,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun showNotificationTrue_and_showAddWidgetTrue_pagesToDisplay_returnsSortedListOfConvertedPages() {
        val expected = listOf(
            defaultBrowserPageUiDataWithPrivacyCaption,
            addSearchWidgetPageUiData,
            syncPageUiData,
            notificationPageUiData,
        )
        assertEquals(
            expected,
            unsortedAllKnownCardData.toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = true,
                showAddWidgetPage = true,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun themeSelectionCardHasExtraData_toPageUiData_returnsConvertedPage() {
        // Page UI values
        val imageRes = R.drawable.ic_pick_a_theme
        val title = "Pick a theme"
        val description = "See the web in the best light."
        val primaryButtonLabel = "Save and continue"
        val secondaryButtonLabel = "Skip"

        // Theme data values
        val themeLabel = "System auto"
        val themeImage = R.drawable.ic_pick_a_theme_system_auto

        // The onboarding card data object containing extraData.
        val themeSelectionCardData = OnboardingCardData(
            cardType = OnboardingCardType.THEME_SELECTION,
            imageRes = imageRes,
            title = StringHolder(null, title),
            body = StringHolder(null, description),
            primaryButtonLabel = StringHolder(null, primaryButtonLabel),
            secondaryButtonLabel = StringHolder(null, secondaryButtonLabel),
            ordering = 45,
            extraData = ExtraCardData(
                customizationThemeData = listOf(
                    CustomizationThemeData(
                        label = StringHolder(null, themeLabel),
                        imageRes = themeImage,
                        themeType = ThemeType.THEME_SYSTEM,
                    ),
                ),
            ),
        )

        // Expected converted page UI data, matching the themeSelectionCardData object
        val expected = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.THEME_SELECTION,
            imageRes = imageRes,
            title = title,
            description = description,
            primaryButtonLabel = primaryButtonLabel,
            secondaryButtonLabel = secondaryButtonLabel,
            themeOptions = listOf(
                ThemeOption(
                    label = themeLabel,
                    imageRes = themeImage,
                    themeType = ThemeOptionType.THEME_SYSTEM,
                ),
            ),
        )

        assertEquals(
            expected,
            listOf(defaultBrowserCardData, themeSelectionCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ).last(),
        )
    }

    @Test
    fun themeSelectedCardNoExtraData_toPageUiData_convertedPageDoesNotIncludeThemeSelectedCard() {
        val themeSelectionCardData = OnboardingCardData(
            cardType = OnboardingCardType.THEME_SELECTION,
            imageRes = R.drawable.ic_pick_a_theme,
            title = StringHolder(null, "Pick a theme"),
            body = StringHolder(null, "See the web in the best light."),
            primaryButtonLabel = StringHolder(null, "Save and continue"),
            secondaryButtonLabel = StringHolder(null, "Skip"),
            ordering = 45,
        )

        assertEquals(
            defaultBrowserPageUiDataWithPrivacyCaption,
            listOf(defaultBrowserCardData, themeSelectionCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ).last(),
        )
    }

    @Test
    fun termsOfServiceData_toPageUiData_returnsConvertedPage() {
        val imageRes = R.drawable.ic_extensions_onboarding
        val title = "Terms of service card title"
        val description = "Terms of service card body"
        val primaryButtonLabel = "add-ons card primary button text"

        val lineOneText = "By continuing, you agree to the Firefox %1\$s."
        val lineOneLinkText = "Terms of Service"
        val lineOneLinkUrl = "LinkOne"
        val lineTwoText = "Firefox cares about your privacy. Read more in our %1\$s."
        val lineTwoLinkText = "Privacy Notice"
        val lineTwoLinkUrl = "LinkTwo"
        val lineThreeText = "To help improve the browser, Firefox sends diagnostic and interaction data to Mozilla. %1\$s"
        val lineThreeLinkText = "Manage"

        val expected = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.TERMS_OF_SERVICE,
            imageRes = imageRes,
            title = title,
            description = description,
            primaryButtonLabel = primaryButtonLabel,
            termsOfService = OnboardingTermsOfService(
                lineOneText = lineOneText,
                lineOneLinkText = lineOneLinkText,
                lineOneLinkUrl = lineOneLinkUrl,
                lineTwoText = lineTwoText,
                lineTwoLinkText = lineTwoLinkText,
                lineTwoLinkUrl = lineTwoLinkUrl,
                lineThreeText = lineThreeText,
                lineThreeLinkText = lineThreeLinkText,
            ),
        )

        val nimbusTermsOfServiceData = TermsOfServiceData(
            lineOneText = StringHolder(R.string.onboarding_term_of_service_line_one, ""),
            lineOneLinkText = StringHolder(R.string.onboarding_term_of_service_line_one_link_text, ""),
            lineOneLinkUrl = StringHolder(null, lineOneLinkUrl),
            lineTwoText = StringHolder(R.string.onboarding_term_of_service_line_two, ""),
            lineTwoLinkText = StringHolder(R.string.onboarding_term_of_service_line_two_link_text, ""),
            lineTwoLinkUrl = StringHolder(null, lineTwoLinkUrl),
            lineThreeText = StringHolder(R.string.onboarding_term_of_service_line_three, ""),
            lineThreeLinkText = StringHolder(R.string.onboarding_term_of_service_line_three_link_text, ""),
        )

        val termsOfServiceCardData = OnboardingCardData(
            cardType = OnboardingCardType.TERMS_OF_SERVICE,
            imageRes = imageRes,
            title = StringHolder(null, title),
            body = StringHolder(null, description),
            primaryButtonLabel = StringHolder(null, primaryButtonLabel),
            ordering = 30,
            extraData = ExtraCardData(
                termOfServiceData = nimbusTermsOfServiceData,
            ),
        )

        assertEquals(
            expected,
            listOf(defaultBrowserCardData, termsOfServiceCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ).last(),
        )
    }

    @Test
    fun addOnsCardHasExtraData_toPageUiData_returnsConvertedPage() {
        val imageRes = R.drawable.ic_extensions_onboarding
        val title = "add-ons card title"
        val description = "add-ons card body"
        val primaryButtonLabel = "add-ons card primary button text"

        // Add-ons
        val addOnIconRes = R.drawable.ic_extensions_onboarding
        val id = "add-on-1"
        val addOnName = "test add-on"
        val addOnDescription = "test add-on description"
        val addOnAverageRating = "5"
        val addOnReviewCount = "1234"
        val addOnInstallUrl = "test.addon.org"
        val status = OnboardingAddonStatus.NOT_INSTALLED

        val expected = OnboardingPageUiData(
            type = OnboardingPageUiData.Type.ADD_ONS,
            imageRes = imageRes,
            title = title,
            description = description,
            primaryButtonLabel = primaryButtonLabel,
            addOns = listOf(
                OnboardingAddOn(
                    id = id,
                    iconRes = addOnIconRes,
                    name = addOnName,
                    description = addOnDescription,
                    averageRating = addOnAverageRating,
                    reviewCount = addOnReviewCount,
                    installUrl = addOnInstallUrl,
                    status = status,
                ),
            ),
        )

        val addOnsCardData = OnboardingCardData(
            cardType = OnboardingCardType.ADD_ONS,
            imageRes = imageRes,
            title = StringHolder(null, title),
            body = StringHolder(null, description),
            primaryButtonLabel = StringHolder(null, primaryButtonLabel),
            ordering = 30,
            extraData = ExtraCardData(
                addOnsData = listOf(
                    AddOnData(
                        id = StringHolder(null, id),
                        iconRes = addOnIconRes,
                        name = StringHolder(null, addOnName),
                        description = StringHolder(null, addOnDescription),
                        averageRating = addOnAverageRating,
                        reviewCount = addOnReviewCount,
                        installUrl = StringHolder(null, addOnInstallUrl),
                    ),
                ),
            ),
        )

        assertEquals(
            expected,
            listOf(defaultBrowserCardData, addOnsCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ).last(),
        )
    }

    @Test
    fun addOnsCardNoExtraData_toPageUiData_convertedPageDoesNotIncludeAddOnsCard() {
        val imageRes = R.drawable.ic_extensions_onboarding
        val title = "add-ons card title"
        val description = "add-ons card body"
        val primaryButtonLabel = "add-ons card primary button text"

        val addOnsCardData = OnboardingCardData(
            cardType = OnboardingCardType.ADD_ONS,
            imageRes = imageRes,
            title = StringHolder(null, title),
            body = StringHolder(null, description),
            primaryButtonLabel = StringHolder(null, primaryButtonLabel),
            ordering = 30,
        )

        assertEquals(
            defaultBrowserPageUiDataWithPrivacyCaption,
            listOf(defaultBrowserCardData, addOnsCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ).last(),
        )
    }

    @Test
    fun cardConditionsMatchJexlConditions_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("ALWAYS" to "true", "NEVER" to "false")
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(defaultBrowserCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noJexlConditionsAndNoCardConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf<String, String>()
        val expected = emptyList<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(addSearchWidgetCardDataNoConditions).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noJexlConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf<String, String>()
        val expected = emptyList<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(defaultBrowserCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun prerequisitesMatchJexlConditions_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("ALWAYS" to "true")
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(defaultBrowserCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun prerequisitesDontMatchJexlConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf("NEVER" to "false")
        val expected = emptyList<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(defaultBrowserCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noCardConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf("ALWAYS" to "true", "NEVER" to "false")
        val expected = emptyList<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(addSearchWidgetCardDataNoConditions).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noDisqualifiers_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("ALWAYS" to "true", "NEVER" to "false")
        val expected = listOf(defaultBrowserPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(defaultBrowserCardDataNoDisqualifiers).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun disqualifiersMatchJexlConditions_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("NEVER" to "false")
        val expected = listOf(syncPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(syncCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun disqualifiersDontMatchJexlConditions_shouldDisplayCard_returnsNoPage() {
        val jexlConditions = mapOf("NEVER" to "false")
        val expected = listOf<OnboardingPageUiData>()

        assertEquals(
            expected,
            listOf(notificationCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }

    @Test
    fun noPrerequisites_shouldDisplayCard_returnsConvertedPage() {
        val jexlConditions = mapOf("ALWAYS" to "true", "NEVER" to "false")
        val expected = listOf(syncPageUiDataWithPrivacyCaption)

        assertEquals(
            expected,
            listOf(syncCardData).toPageUiData(
                privacyCaption = privacyCaption,
                showDefaultBrowserPage = true,
                showNotificationPage = false,
                showAddWidgetPage = false,
                jexlConditions = jexlConditions,
                func = evalFunction,
            ),
        )
    }
}
val privacyCaption: Caption = mockk(relaxed = true)

private val defaultBrowserPageUiDataWithPrivacyCaption = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.DEFAULT_BROWSER,
    imageRes = R.drawable.ic_onboarding_welcome,
    title = "default browser title",
    description = "default browser body",
    primaryButtonLabel = "default browser primary button text",
    secondaryButtonLabel = "default browser secondary button text",
    privacyCaption = privacyCaption,
)
private val addSearchWidgetPageUiData = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.ADD_SEARCH_WIDGET,
    imageRes = R.drawable.ic_onboarding_search_widget,
    title = "add search widget title",
    description = "add search widget body",
    primaryButtonLabel = "add search widget primary button text",
    secondaryButtonLabel = "add search widget secondary button text",
    privacyCaption = null,
)
private val addSearchWidgetPageUiDataWithPrivacyCaption = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.ADD_SEARCH_WIDGET,
    imageRes = R.drawable.ic_onboarding_search_widget,
    title = "add search widget title",
    description = "add search widget body",
    primaryButtonLabel = "add search widget primary button text",
    secondaryButtonLabel = "add search widget secondary button text",
    privacyCaption = privacyCaption,
)
private val syncPageUiData = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.SYNC_SIGN_IN,
    imageRes = R.drawable.ic_onboarding_sync,
    title = "sync title",
    description = "sync body",
    primaryButtonLabel = "sync primary button text",
    secondaryButtonLabel = "sync secondary button text",
    privacyCaption = null,
)
private val syncPageUiDataWithPrivacyCaption = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.SYNC_SIGN_IN,
    imageRes = R.drawable.ic_onboarding_sync,
    title = "sync title",
    description = "sync body",
    primaryButtonLabel = "sync primary button text",
    secondaryButtonLabel = "sync secondary button text",
    privacyCaption = privacyCaption,
)
private val notificationPageUiData = OnboardingPageUiData(
    type = OnboardingPageUiData.Type.NOTIFICATION_PERMISSION,
    imageRes = R.drawable.ic_notification_permission,
    title = "notification title",
    description = "notification body",
    primaryButtonLabel = "notification primary button text",
    secondaryButtonLabel = "notification secondary button text",
    privacyCaption = null,
)

private val defaultBrowserCardData = OnboardingCardData(
    cardType = OnboardingCardType.DEFAULT_BROWSER,
    imageRes = R.drawable.ic_onboarding_welcome,
    title = StringHolder(null, "default browser title"),
    body = StringHolder(null, "default browser body"),
    primaryButtonLabel = StringHolder(null, "default browser primary button text"),
    secondaryButtonLabel = StringHolder(null, "default browser secondary button text"),
    ordering = 10,
    prerequisites = listOf("ALWAYS"),
    disqualifiers = listOf("NEVER"),
)

private val defaultBrowserCardDataNoDisqualifiers = OnboardingCardData(
    cardType = OnboardingCardType.DEFAULT_BROWSER,
    imageRes = R.drawable.ic_onboarding_welcome,
    title = StringHolder(null, "default browser title"),
    body = StringHolder(null, "default browser body"),
    primaryButtonLabel = StringHolder(null, "default browser primary button text"),
    secondaryButtonLabel = StringHolder(null, "default browser secondary button text"),
    ordering = 10,
    prerequisites = listOf("ALWAYS"),
    disqualifiers = listOf(),
)

private val addSearchWidgetCardDataNoConditions = OnboardingCardData(
    cardType = OnboardingCardType.ADD_SEARCH_WIDGET,
    imageRes = R.drawable.ic_onboarding_search_widget,
    title = StringHolder(null, "add search widget title"),
    body = StringHolder(null, "add search widget body"),
    primaryButtonLabel = StringHolder(null, "add search widget primary button text"),
    secondaryButtonLabel = StringHolder(null, "add search widget secondary button text"),
    ordering = 15,
    prerequisites = listOf(),
    disqualifiers = listOf(),
)

private val addSearchWidgetCardData = OnboardingCardData(
    cardType = OnboardingCardType.ADD_SEARCH_WIDGET,
    imageRes = R.drawable.ic_onboarding_search_widget,
    title = StringHolder(null, "add search widget title"),
    body = StringHolder(null, "add search widget body"),
    primaryButtonLabel = StringHolder(null, "add search widget primary button text"),
    secondaryButtonLabel = StringHolder(null, "add search widget secondary button text"),
    ordering = 15,
)

private val syncCardData = OnboardingCardData(
    cardType = OnboardingCardType.SYNC_SIGN_IN,
    imageRes = R.drawable.ic_onboarding_sync,
    title = StringHolder(null, "sync title"),
    body = StringHolder(null, "sync body"),
    primaryButtonLabel = StringHolder(null, "sync primary button text"),
    secondaryButtonLabel = StringHolder(null, "sync secondary button text"),
    ordering = 20,
    prerequisites = listOf(),
    disqualifiers = listOf("NEVER"),
)

private val notificationCardData = OnboardingCardData(
    cardType = OnboardingCardType.NOTIFICATION_PERMISSION,
    imageRes = R.drawable.ic_notification_permission,
    title = StringHolder(null, "notification title"),
    body = StringHolder(null, "notification body"),
    primaryButtonLabel = StringHolder(null, "notification primary button text"),
    secondaryButtonLabel = StringHolder(null, "notification secondary button text"),
    ordering = 30,
    prerequisites = listOf(),
    disqualifiers = listOf("NEVER", "OTHER"),
)

private val unsortedAllKnownCardData = listOf(
    syncCardData,
    notificationCardData,
    defaultBrowserCardData,
    addSearchWidgetCardData,
)
