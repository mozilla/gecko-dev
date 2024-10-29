/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.annotation.DrawableRes
import org.mozilla.fenix.compose.LinkTextState

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
    val reviewCount: String,
    val installUrl: String,
)
