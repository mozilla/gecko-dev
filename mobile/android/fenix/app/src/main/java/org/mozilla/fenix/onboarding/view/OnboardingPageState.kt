/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.annotation.DrawableRes
import org.mozilla.fenix.compose.LinkTextState

/**
 * Model containing data for [OnboardingPage].
 *
 * @property imageRes [DrawableRes] displayed on the page.
 * @property title [String] title of the page.
 * @property description [String] description of the page.
 * @property privacyCaption privacy caption to show and allow user to view on privacy policy.
 * @property primaryButton [Action] action for the primary button.
 * @property secondaryButton [Action] action for the secondary button.
 * @property onRecordImpressionEvent Callback for recording impression event.
 */
data class OnboardingPageState(
    @DrawableRes val imageRes: Int,
    val title: String,
    val description: String,
    val privacyCaption: Caption? = null,
    val primaryButton: Action,
    val secondaryButton: Action? = null,
    val onRecordImpressionEvent: () -> Unit = {},
)

/**
 * Model containing data for [AddOnsOnboardingPage].
 *
 * @property imageRes Image resource for the main image to be displayed on the page.
 * @property title Title of the page.
 * @property description Description of the page.
 * @property primaryButton [Action] for the primary button.
 * @property addOnsUiData List of add-ons to install during onboarding.
 * @property onRecordImpressionEvent Callback for recording impression event.
 */
data class OnboardingAddOnsPageState(
    @DrawableRes val imageRes: Int,
    val title: String,
    val description: String,
    val primaryButton: Action,
    val addOnsUiData: List<OnboardingAddOn>,
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
    @DrawableRes val iconRes: Int,
    val name: String,
    val description: String,
    val averageRating: String,
    val numberOfReviews: String,
)
