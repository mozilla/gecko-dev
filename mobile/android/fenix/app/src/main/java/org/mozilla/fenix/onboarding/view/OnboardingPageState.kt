/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.annotation.DrawableRes
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.onboarding.store.OnboardingAddonStatus

/**
 * Model containing data for [OnboardingPage].
 *
 * @property imageRes The main image to be displayed on the page.
 * @property title Title of the page.
 * @property description Description of the page.
 * @property privacyCaption Optional privacy caption to show and allow user to view the privacy policy.
 * @property primaryButton [Action] for the primary button.
 * @property secondaryButton Optional [Action] for the secondary button.
 * @property addOns Optional list of add-ons to install during onboarding.
 * @property themeOptions Optional list of theme customizing options during onboarding.
 * @property termsOfService Optional term of service page data.
 * @property toolbarOptions Optional list of toolbar selection options.
 * @property onRecordImpressionEvent Callback for recording impression event.
 */
data class OnboardingPageState(
    @DrawableRes val imageRes: Int,
    val title: String,
    val description: String,
    val privacyCaption: Caption? = null,
    val primaryButton: Action,
    val secondaryButton: Action? = null,
    val addOns: List<OnboardingAddOn>? = null,
    val themeOptions: List<ThemeOption>? = null,
    val termsOfService: OnboardingTermsOfService? = null,
    val toolbarOptions: List<ToolbarOption>? = null,
    val onRecordImpressionEvent: () -> Unit = {},
)

/**
 * Model containing text and action for a button.
 */
data class Action(
    val text: String,
    val onClick: () -> Unit,
)

/**
 * Model containing text and [LinkTextState] for a caption.
 */
data class Caption(
    val text: String,
    val linkTextState: LinkTextState,
)

/**
 * Model containing data for an add-on that's installable during onboarding.
 */
data class OnboardingAddOn(
    val id: String,
    @DrawableRes val iconRes: Int,
    val name: String,
    val description: String,
    val averageRating: String,
    val reviewCount: String,
    val installUrl: String,
    val status: OnboardingAddonStatus,
)

/**
 * Model containing data for a toolbar placement.
 */
data class ToolbarOption(
    val toolbarType: ToolbarOptionType,
    @DrawableRes val imageRes: Int,
    val label: String,
)

/**
 * Types of toolbar placement options available.
 *
 * @property id Identifier for the toolbar option type, used in telemetry.
 */
enum class ToolbarOptionType(val id: String) {
    /**
     * Sets the toolbar placement to the top.
     */
    TOOLBAR_TOP("toolbar_top"),

    /**
     * Sets the toolbar placement to the bottom.
     */
    TOOLBAR_BOTTOM("toolbar_bottom"),
}

/**
 * Model containing data for theme customizing during onboarding.
 */
data class ThemeOption(
    val label: String,
    val imageRes: Int,
    val themeType: ThemeOptionType,
)

/**
 * Types of theming options available.
 */
enum class ThemeOptionType(val id: String) {
    /**
     * Sets the theme to dark mode.
     */
    THEME_DARK("theme_dark"),

    /**
     * Sets the theme to light mode.
     */
    THEME_LIGHT("theme_light"),

    /**
     * Adapts the theme to match the device's system setting.
     */
    THEME_SYSTEM("theme_system"),
}

/**
 * Model containing data for the terms of service page during onboarding.
 */
data class OnboardingTermsOfService(
    val lineOneText: String,
    val lineOneLinkText: String,
    val lineOneLinkUrl: String,
    val lineTwoText: String,
    val lineTwoLinkText: String,
    val lineTwoLinkUrl: String,
    val lineThreeText: String,
    val lineThreeLinkText: String,
)

/**
 * Contains all the events which can happen in terms of service onboarding page.
 */
interface OnboardingTermsOfServiceEventHandler {

    /**
     * Invoked when the terms of service link is clicked.
     */
    fun onTermsOfServiceLinkClicked(url: String) = Unit

    /**
     * Invoked when the privacy notice link is clicked.
     */
    fun onPrivacyNoticeLinkClicked(url: String) = Unit

    /**
     * Invoked when the manage privacy preferences link is clicked.
     */
    fun onManagePrivacyPreferencesLinkClicked() = Unit

    /**
     * Invoked when the accept button is clicked.
     */
    fun onAcceptTermsButtonClicked() = Unit
}
