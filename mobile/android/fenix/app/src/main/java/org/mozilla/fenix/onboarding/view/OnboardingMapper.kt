/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import org.mozilla.fenix.nimbus.AddOnData
import org.mozilla.fenix.nimbus.CustomizationThemeData
import org.mozilla.fenix.nimbus.CustomizationToolbarData
import org.mozilla.fenix.nimbus.OnboardingCardData
import org.mozilla.fenix.nimbus.OnboardingCardType
import org.mozilla.fenix.nimbus.TermsOfServiceData
import org.mozilla.fenix.nimbus.ThemeType
import org.mozilla.fenix.nimbus.ToolbarType
import org.mozilla.fenix.onboarding.store.OnboardingAddonStatus

/**
 * Returns a list of all the required Nimbus 'cards' that have been converted to [OnboardingPageUiData].
 */
internal fun Collection<OnboardingCardData>.toPageUiData(
    privacyCaption: Caption,
    showDefaultBrowserPage: Boolean,
    showNotificationPage: Boolean,
    showAddWidgetPage: Boolean,
    jexlConditions: Map<String, String>,
    func: (String) -> Boolean,
): List<OnboardingPageUiData> {
    // we are first filtering the cards based on Nimbus configuration
    return filter { it.shouldDisplayCard(func, jexlConditions) }
        // we are then filtering again based on device capabilities
        .filter { it.isCardEnabled(showDefaultBrowserPage, showNotificationPage, showAddWidgetPage) }
        .sortedBy { it.ordering }
        .mapIndexed {
                index, onboardingCardData ->
            // only first onboarding card shows privacy caption
            onboardingCardData.toPageUiData(if (index == 0) privacyCaption else null)
        }
}

private fun OnboardingCardData.isCardEnabled(
    showDefaultBrowserPage: Boolean,
    showNotificationPage: Boolean,
    showAddWidgetPage: Boolean,
): Boolean = when (cardType) {
    OnboardingCardType.DEFAULT_BROWSER -> enabled && showDefaultBrowserPage
    OnboardingCardType.NOTIFICATION_PERMISSION -> enabled && showNotificationPage
    OnboardingCardType.ADD_SEARCH_WIDGET -> enabled && showAddWidgetPage
    OnboardingCardType.ADD_ONS -> extraData?.addOnsData?.isNotEmpty() == true
    OnboardingCardType.TOOLBAR_PLACEMENT -> extraData?.customizationToolbarData?.isNotEmpty() == true
    OnboardingCardType.THEME_SELECTION -> extraData?.customizationThemeData?.isNotEmpty() == true
    else -> enabled
}

/**
 *  Determines whether the given [OnboardingCardData] should be displayed.
 *
 *  @param func Function that receives a condition as a [String] and returns its JEXL evaluation as a [Boolean].
 *  @param jexlConditions A <String, String> map containing the Nimbus conditions.
 *
 *  @return True if the card should be displayed, otherwise false.
 */
private fun OnboardingCardData.shouldDisplayCard(
    func: (String) -> Boolean,
    jexlConditions: Map<String, String>,
): Boolean {
    val jexlCache: MutableMap<String, Boolean> = mutableMapOf()

    // Make sure the conditions exist and have a value, and that the number
    // of valid conditions matches the number of conditions on the card's
    // respective prerequisite or disqualifier table. If these mismatch,
    // that means a card contains a condition that's not in the feature
    // conditions lookup table. JEXLs can only be evaluated on
    // supported conditions. Otherwise, consider the card invalid.
    val allPrerequisites = prerequisites.mapNotNull { jexlConditions[it] }
    val allDisqualifiers = disqualifiers.mapNotNull { jexlConditions[it] }

    val validPrerequisites = if (allPrerequisites.size == prerequisites.size) {
        allPrerequisites.all { condition ->
            jexlCache.getOrPut(condition) {
                func(condition)
            }
        }
    } else {
        false
    }

    val hasDisqualifiers =
        if (allDisqualifiers.isNotEmpty() && allDisqualifiers.size == disqualifiers.size) {
            allDisqualifiers.all { condition ->
                jexlCache.getOrPut(condition) {
                    func(condition)
                }
            }
        } else {
            false
        }

    return validPrerequisites && !hasDisqualifiers
}

private fun OnboardingCardData.toPageUiData(privacyCaption: Caption?) = OnboardingPageUiData(
    type = cardType.toPageUiDataType(),
    imageRes = imageRes.resourceId,
    title = title,
    description = body,
    primaryButtonLabel = primaryButtonLabel,
    secondaryButtonLabel = secondaryButtonLabel.ifEmpty { null },
    privacyCaption = privacyCaption,
    addOns = extraData?.addOnsData?.takeIf { it.isNotEmpty() }?.toOnboardingAddOns(),
    toolbarOptions = extraData?.customizationToolbarData
        ?.takeIf { it.isNotEmpty() }
        ?.toOnboardingToolbarOptions(),
    themeOptions = extraData?.customizationThemeData
        ?.takeIf { it.isNotEmpty() }
        ?.toOnboardingThemeOptions(),
    termsOfService = extraData?.termOfServiceData?.toOnboardingTermsOfService(),
)

private fun OnboardingCardType.toPageUiDataType() = when (this) {
    OnboardingCardType.DEFAULT_BROWSER -> OnboardingPageUiData.Type.DEFAULT_BROWSER
    OnboardingCardType.SYNC_SIGN_IN -> OnboardingPageUiData.Type.SYNC_SIGN_IN
    OnboardingCardType.NOTIFICATION_PERMISSION -> OnboardingPageUiData.Type.NOTIFICATION_PERMISSION
    OnboardingCardType.ADD_SEARCH_WIDGET -> OnboardingPageUiData.Type.ADD_SEARCH_WIDGET
    OnboardingCardType.ADD_ONS -> OnboardingPageUiData.Type.ADD_ONS
    OnboardingCardType.TOOLBAR_PLACEMENT -> OnboardingPageUiData.Type.TOOLBAR_PLACEMENT
    OnboardingCardType.THEME_SELECTION -> OnboardingPageUiData.Type.THEME_SELECTION
    OnboardingCardType.TERMS_OF_SERVICE -> OnboardingPageUiData.Type.TERMS_OF_SERVICE
}

private fun List<AddOnData>.toOnboardingAddOns() = map { it.toOnboardingAddOn() }

private fun List<CustomizationToolbarData>.toOnboardingToolbarOptions() = map { it.toOnboardingCustomizeToolbar() }

private fun TermsOfServiceData.toOnboardingTermsOfService() = with(this) {
    OnboardingTermsOfService(
        lineOneText = lineOneText,
        lineOneLinkText = lineOneLinkText,
        lineOneLinkUrl = lineOneLinkUrl,
        lineTwoText = lineTwoText,
        lineTwoLinkText = lineTwoLinkText,
        lineTwoLinkUrl = lineTwoLinkUrl,
        lineThreeText = lineThreeText,
        lineThreeLinkText = lineThreeLinkText,
    )
}

private fun AddOnData.toOnboardingAddOn() = with(this) {
    OnboardingAddOn(
        id = id,
        iconRes = iconRes.resourceId,
        name = name,
        description = description,
        averageRating = averageRating,
        reviewCount = reviewCount,
        installUrl = installUrl,
        status = OnboardingAddonStatus.NOT_INSTALLED,
    )
}

private fun CustomizationToolbarData.toOnboardingCustomizeToolbar() = with(this) {
    ToolbarOption(
        toolbarType = toolbarType.toToolbarOptionType(),
        imageRes = imageRes.resourceId,
        label = label,
    )
}

private fun ToolbarType.toToolbarOptionType() = when (this) {
    ToolbarType.TOOLBAR_TOP -> ToolbarOptionType.TOOLBAR_TOP
    ToolbarType.TOOLBAR_BOTTOM -> ToolbarOptionType.TOOLBAR_BOTTOM
}

private fun List<CustomizationThemeData>.toOnboardingThemeOptions() = map { it.toOnboardingThemeOption() }

private fun CustomizationThemeData.toOnboardingThemeOption() = with(this) {
    ThemeOption(
        label = label,
        imageRes = imageRes.resourceId,
        themeType = themeType.toThemeOptionType(),
    )
}

private fun ThemeType.toThemeOptionType() = when (this) {
    ThemeType.THEME_DARK -> ThemeOptionType.THEME_DARK
    ThemeType.THEME_LIGHT -> ThemeOptionType.THEME_LIGHT
    ThemeType.THEME_SYSTEM -> ThemeOptionType.THEME_SYSTEM
}

/**
 * Mapper to convert [OnboardingPageUiData] to [OnboardingPageState] that is a param for
 * [OnboardingPage] composable.
 */
@Suppress("LongParameterList")
internal fun mapToOnboardingPageState(
    onboardingPageUiData: OnboardingPageUiData,
    onMakeFirefoxDefaultClick: () -> Unit,
    onMakeFirefoxDefaultSkipClick: () -> Unit,
    onSignInButtonClick: () -> Unit,
    onSignInSkipClick: () -> Unit,
    onNotificationPermissionButtonClick: () -> Unit,
    onNotificationPermissionSkipClick: () -> Unit,
    onAddFirefoxWidgetClick: () -> Unit,
    onAddFirefoxWidgetSkipClick: () -> Unit,
    onAddOnsButtonClick: () -> Unit,
    onCustomizeToolbarButtonClick: () -> Unit,
    onCustomizeToolbarSkipClick: () -> Unit,
    onCustomizeThemeClick: () -> Unit,
    onCustomizeThemeSkip: () -> Unit,
    onTermsOfServiceButtonClick: () -> Unit,
): OnboardingPageState = when (onboardingPageUiData.type) {
    OnboardingPageUiData.Type.DEFAULT_BROWSER -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onMakeFirefoxDefaultClick,
        onNegativeButtonClick = onMakeFirefoxDefaultSkipClick,
    )

    OnboardingPageUiData.Type.ADD_SEARCH_WIDGET -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onAddFirefoxWidgetClick,
        onNegativeButtonClick = onAddFirefoxWidgetSkipClick,
    )

    OnboardingPageUiData.Type.SYNC_SIGN_IN -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onSignInButtonClick,
        onNegativeButtonClick = onSignInSkipClick,
    )

    OnboardingPageUiData.Type.NOTIFICATION_PERMISSION -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onNotificationPermissionButtonClick,
        onNegativeButtonClick = onNotificationPermissionSkipClick,
    )

    OnboardingPageUiData.Type.ADD_ONS -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onAddOnsButtonClick,
        onNegativeButtonClick = {}, // No negative button option for add-ons.
    )

    OnboardingPageUiData.Type.TOOLBAR_PLACEMENT -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onCustomizeToolbarButtonClick,
        onNegativeButtonClick = onCustomizeToolbarSkipClick,
    )

    OnboardingPageUiData.Type.THEME_SELECTION -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onCustomizeThemeClick,
        onNegativeButtonClick = onCustomizeThemeSkip,
    )

    OnboardingPageUiData.Type.TERMS_OF_SERVICE -> createOnboardingPageState(
        onboardingPageUiData = onboardingPageUiData,
        onPositiveButtonClick = onTermsOfServiceButtonClick,
        onNegativeButtonClick = {}, // No negative button option for terms of service.
    )
}

private fun createOnboardingPageState(
    onboardingPageUiData: OnboardingPageUiData,
    onPositiveButtonClick: () -> Unit,
    onNegativeButtonClick: () -> Unit,
): OnboardingPageState = OnboardingPageState(
    imageRes = onboardingPageUiData.imageRes,
    title = onboardingPageUiData.title,
    description = onboardingPageUiData.description,
    primaryButton = Action(onboardingPageUiData.primaryButtonLabel, onPositiveButtonClick),
    secondaryButton = onboardingPageUiData.secondaryButtonLabel?.let {
        Action(it, onNegativeButtonClick)
    },
    privacyCaption = onboardingPageUiData.privacyCaption,
    addOns = onboardingPageUiData.addOns,
    themeOptions = onboardingPageUiData.themeOptions,
    toolbarOptions = onboardingPageUiData.toolbarOptions,
    termsOfService = onboardingPageUiData.termsOfService,
)
