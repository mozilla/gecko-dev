/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.iconpicker

import androidx.annotation.StringRes
import org.mozilla.fenix.R

/**
 * Type that represents an app icon shown in the icon picker UI.
 *
 * @property activityAlias The corresponding [ActivityAlias].
 * @property titleId A string resource describing the icon in the icon picker screen.
 * @property subtitleId An optional string resource used as a secondary label.
 * @property isSelected Whether this option is selected.
 */
data class SettingsAppIcon(
    val activityAlias: ActivityAlias,
    @param:StringRes val titleId: Int,
    @param:StringRes val subtitleId: Int? = null,
    val isSelected: Boolean = false,
) {

    /**
     * [SettingsAppIcon] helper object
     */
    companion object {

        private val appDefault = SettingsAppIcon(
            activityAlias = ActivityAlias.AppDefault,
            titleId = R.string.alternative_app_icon_option_default,
        )
        private val appSolidLight = SettingsAppIcon(
            activityAlias = ActivityAlias.AppSolidLight,
            titleId = R.string.alternative_app_icon_option_light,
        )
        private val appSolidDark = SettingsAppIcon(
            activityAlias = ActivityAlias.AppSolidDark,
            titleId = R.string.alternative_app_icon_option_dark,
        )
        private val appSolidRed = SettingsAppIcon(
            activityAlias = ActivityAlias.AppSolidRed,
            titleId = R.string.alternative_app_icon_option_red,
        )
        private val appSolidGreen = SettingsAppIcon(
            activityAlias = ActivityAlias.AppSolidGreen,
            titleId = R.string.alternative_app_icon_option_green,
        )
        private val appSolidBlue = SettingsAppIcon(
            activityAlias = ActivityAlias.AppSolidBlue,
            titleId = R.string.alternative_app_icon_option_blue,
        )
        private val appSolidPurple = SettingsAppIcon(
            activityAlias = ActivityAlias.AppSolidPurple,
            titleId = R.string.alternative_app_icon_option_purple,
        )
        private val appSolidPurpleDark = SettingsAppIcon(
            activityAlias = ActivityAlias.AppSolidPurpleDark,
            titleId = R.string.alternative_app_icon_option_purple_dark,
        )
        private val appGradientSunrise = SettingsAppIcon(
            activityAlias = ActivityAlias.AppGradientSunrise,
            titleId = R.string.alternative_app_icon_option_gradient_sunrise,
        )
        private val appGradientGoldenHour = SettingsAppIcon(
            activityAlias = ActivityAlias.AppGradientGoldenHour,
            titleId = R.string.alternative_app_icon_option_gradient_golden_hour,
        )
        private val appGradientSunset = SettingsAppIcon(
            activityAlias = ActivityAlias.AppGradientSunset,
            titleId = R.string.alternative_app_icon_option_gradient_sunset,
        )
        private val appGradientBlueHour = SettingsAppIcon(
            activityAlias = ActivityAlias.AppGradientBlueHour,
            titleId = R.string.alternative_app_icon_option_gradient_blue_hour,
        )
        private val appGradientTwilight = SettingsAppIcon(
            activityAlias = ActivityAlias.AppGradientTwilight,
            titleId = R.string.alternative_app_icon_option_gradient_twilight,
        )
        private val appGradientMidnight = SettingsAppIcon(
            activityAlias = ActivityAlias.AppGradientMidnight,
            titleId = R.string.alternative_app_icon_option_gradient_midnight,
        )
        private val appGradientNorthernLights = SettingsAppIcon(
            activityAlias = ActivityAlias.AppGradientNorthernLights,
            titleId = R.string.alternative_app_icon_option_gradient_northern_lights,
        )
        private val appRetro2004 = SettingsAppIcon(
            activityAlias = ActivityAlias.AppRetro2004,
            titleId = R.string.alternative_app_icon_option_retro_2004,
        )
        private val appRetro2017 = SettingsAppIcon(
            activityAlias = ActivityAlias.AppRetro2017,
            titleId = R.string.alternative_app_icon_option_retro_2017,
        )
        private val appPixelated = SettingsAppIcon(
            activityAlias = ActivityAlias.AppPixelated,
            titleId = R.string.alternative_app_icon_option_pixelated,
        )
        private val appMinimal = SettingsAppIcon(
            activityAlias = ActivityAlias.AppMinimal,
            titleId = R.string.alternative_app_icon_option_minimal,
        )
        private val appPride = SettingsAppIcon(
            activityAlias = ActivityAlias.AppPride,
            titleId = R.string.alternative_app_icon_option_pride,
        )
        private val appCute = SettingsAppIcon(
            activityAlias = ActivityAlias.AppCute,
            titleId = R.string.alternative_app_icon_option_cute,
        )
        private val appMomo = SettingsAppIcon(
            activityAlias = ActivityAlias.AppMomo,
            titleId = R.string.alternative_app_icon_option_momo,
            subtitleId = R.string.alternative_app_icon_option_momo_subtitle,
        )

        /**
         *  Grouped [SettingsAppIcon] options used in the icon picker screen.
         */
        val groupedAppIcons: Map<SettingsGroupTitle, List<SettingsAppIcon>> = mapOf(
            SettingsGroupTitle(R.string.alternative_app_icon_group_solid_colors) to listOf(
                appDefault,
                appSolidLight,
                appSolidDark,
                appSolidRed,
                appSolidGreen,
                appSolidBlue,
                appSolidPurple,
                appSolidPurpleDark,
            ),
            SettingsGroupTitle(R.string.alternative_app_icon_group_gradients) to listOf(
                appGradientSunrise,
                appGradientGoldenHour,
                appGradientSunset,
                appGradientBlueHour,
                appGradientTwilight,
                appGradientMidnight,
                appGradientNorthernLights,
            ),
            SettingsGroupTitle(R.string.alternative_app_icon_group_other) to listOf(
                appRetro2004,
                appRetro2017,
                appPixelated,
                appMinimal,
                appPride,
                appCute,
                appMomo,
            ),
        )
    }
}

/**
 * Type that represents a group title in the icon picker UI.
 *
 * @property titleId A string resource describing the group title.
 */
data class SettingsGroupTitle(@param:StringRes val titleId: Int)
