/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding.view

import androidx.annotation.DrawableRes
import org.mozilla.fenix.nimbus.OnboardingCardData

/**
 * Model containing the required data from a raw [OnboardingCardData] object in a UI state.
 */
data class OnboardingPageUiData(
    val type: Type,
    @DrawableRes val imageRes: Int,
    val title: String,
    val description: String,
    val primaryButtonLabel: String,
    val secondaryButtonLabel: String? = null,
    val privacyCaption: Caption? = null,
    val addOns: List<OnboardingAddOn>? = null,
    val toolbarOptions: List<ToolbarOption>? = null,
    val themeOptions: List<ThemeOption>? = null,
    val termsOfService: OnboardingTermsOfService? = null,
) {
    /**
     * Model for different types of Onboarding Pages.
     *
     * @property telemetryId Identifier for the page, used in telemetry.
     */
    enum class Type(
        val telemetryId: String,
    ) {
        DEFAULT_BROWSER(
            telemetryId = "default",
        ),
        SYNC_SIGN_IN(
            telemetryId = "sync",
        ),
        ADD_SEARCH_WIDGET(
            telemetryId = "search_widget",
        ),
        NOTIFICATION_PERMISSION(
            telemetryId = "notification",
        ),
        ADD_ONS(
            telemetryId = "add_ons",
        ),
        TOOLBAR_PLACEMENT(
            telemetryId = "toolbar_placement",
        ),
        THEME_SELECTION(
            telemetryId = "theme",
        ),
        TERMS_OF_SERVICE(
            telemetryId = "terms_of_service",
        ),
    }
}

/**
 * Returns the sequence position for the given [OnboardingPageUiData.Type].
 */
fun List<OnboardingPageUiData>.sequencePosition(type: OnboardingPageUiData.Type): String =
    indexOfFirst { it.type == type }.inc().toString()

/**
 * Helper function for telemetry that maps List<OnboardingPageUiData> to a string of page names
 * separated by an underscore.
 * e.g. [DEFAULT_BROWSER, SYNC_SIGN_IN] will be mapped to "default_sync".
 */
fun List<OnboardingPageUiData>.telemetrySequenceId(): String =
    joinToString(separator = "_") { it.type.telemetryId }
