/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.iconpicker

import androidx.annotation.ColorRes
import androidx.annotation.DrawableRes
import org.mozilla.fenix.R

/**
 * Enum that represents <activity-alias> entries declared in the AndroidManifest.
 * These entries are used to allow users to choose an alternative launcher icon.
 *
 * @property aliasSuffix A reference to the [AliasSuffix] enum, which maps to the suffix portion
 * of the `android:name` attribute in the manifest.
 * @property iconForegroundId The foreground drawable resource used in icon previews.
 * @property iconBackground The background layer used in icon previews, which can be a solid color or drawable.
 */
enum class ActivityAlias(
    val aliasSuffix: AliasSuffix,
    val iconForegroundId: Int,
    val iconBackground: IconBackground = IconBackground.Color(colorResId = R.color.photonWhite),
) {
    AppDefault(
        aliasSuffix = AliasSuffix.AppDefault,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Color(colorResId = R.color.ic_launcher_background),
    ),
    AppSolidLight(
        aliasSuffix = AliasSuffix.AppSolidLight,
        iconForegroundId = R.drawable.ic_launcher_foreground,
    ),
    AppSolidDark(
        aliasSuffix = AliasSuffix.AppSolidDark,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Color(colorResId = R.color.photonBlack),
    ),
    AppSolidRed(
        aliasSuffix = AliasSuffix.AppSolidRed,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_solid_red_background),
    ),
    AppSolidGreen(
        aliasSuffix = AliasSuffix.AppSolidGreen,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_solid_green_background),
    ),
    AppSolidBlue(
        aliasSuffix = AliasSuffix.AppSolidBlue,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_solid_blue_background),
    ),
    AppSolidPurple(
        aliasSuffix = AliasSuffix.AppSolidPurple,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_solid_purple_background),
    ),
    AppSolidPurpleDark(
        aliasSuffix = AliasSuffix.AppSolidPurpleDark,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_solid_purple_dark_background),
    ),
    AppGradientSunrise(
        aliasSuffix = AliasSuffix.AppGradientSunrise,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_gradient_sunrise_background),
    ),
    AppGradientGoldenHour(
        aliasSuffix = AliasSuffix.AppGradientGoldenHour,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_gradient_golden_hour_background),
    ),
    AppGradientSunset(
        aliasSuffix = AliasSuffix.AppGradientSunset,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_gradient_sunset_background),
    ),
    AppGradientBlueHour(
        aliasSuffix = AliasSuffix.AppGradientBlueHour,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_gradient_blue_hour_background),
    ),
    AppGradientTwilight(
        aliasSuffix = AliasSuffix.AppGradientTwilight,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_gradient_twilight_background),
    ),
    AppGradientMidnight(
        aliasSuffix = AliasSuffix.AppGradientMidnight,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_gradient_midnight_background),
    ),
    AppGradientNorthernLights(
        aliasSuffix = AliasSuffix.AppGradientNorthernLights,
        iconForegroundId = R.drawable.ic_launcher_foreground,
        iconBackground = IconBackground.Drawable(R.drawable.ic_launcher_gradient_northern_lights_background),
    ),
    AppRetro2004(
        aliasSuffix = AliasSuffix.AppRetro2004,
        iconForegroundId = R.drawable.ic_launcher_foreground_retro_2004,
    ),
    AppRetro2017(
        aliasSuffix = AliasSuffix.AppRetro2017,
        iconForegroundId = R.drawable.ic_launcher_foreground_retro_2017,
    ),
    AppPixelated(
        aliasSuffix = AliasSuffix.AppPixelated,
        iconForegroundId = R.drawable.ic_launcher_foreground_pixelated,
    ),
    AppMinimal(
        aliasSuffix = AliasSuffix.AppMinimal,
        iconForegroundId = R.drawable.ic_launcher_foreground_minimal,
    ),
    AppPride(
        aliasSuffix = AliasSuffix.AppPride,
        iconForegroundId = R.drawable.ic_launcher_foreground_pride,
    ),
    AppCute(
        aliasSuffix = AliasSuffix.AppCute,
        iconForegroundId = R.drawable.ic_launcher_foreground_cute,
    ),
    AppMomo(
        aliasSuffix = AliasSuffix.AppMomo,
        iconForegroundId = R.drawable.ic_launcher_foreground_momo,
    ),
}

/**
 * Represents the background layer of an app icon mipmap assigned to a `<activity-alias>`.
 *
 * It allows passing both `@DrawableRes` and `@ColorRes`, as mipmap files support both
 * `drawable` and `color` parameters for `<background android:drawable>`
 */
sealed class IconBackground {
    /**
     * A solid color background.
     *
     * @property colorResId The color resource ID to use.
     */
    data class Color(@param:ColorRes val colorResId: Int) : IconBackground()

    /**
     * A drawable background.
     *
     * @property drawableResId The drawable resource ID to use.
     */
    data class Drawable(@param:DrawableRes val drawableResId: Int) : IconBackground()
}

/**
 * Enum that maps directly to the `android:name` attribute of each `<activity-alias>` entry
 * in the AndroidManifest. Only the alias name suffix (after the application ID) is stored.
 *
 * These values are used to construct full component names for switching the launcher icon at runtime.
 *
 * Example:
 * - "AppSolidLight" → android:name="${applicationId}.AppSolidLight"
 *
 * @property suffix The alias name suffix as declared in the manifest.
 */
enum class AliasSuffix(val suffix: String) {
    AppDefault("App"),
    AppSolidLight("AppSolidLight"),
    AppSolidDark("AppSolidDark"),
    AppSolidRed("AppSolidRed"),
    AppSolidGreen("AppSolidGreen"),
    AppSolidBlue("AppSolidBlue"),
    AppSolidPurple("AppSolidPurple"),
    AppSolidPurpleDark("AppSolidPurpleDark"),
    AppGradientSunrise("AppGradientSunrise"),
    AppGradientGoldenHour("AppGradientGoldenHour"),
    AppGradientSunset("AppGradientSunset"),
    AppGradientBlueHour("AppGradientBlueHour"),
    AppGradientTwilight("AppGradientTwilight"),
    AppGradientMidnight("AppGradientMidnight"),
    AppGradientNorthernLights("AppGradientNorthernLights"),
    AppRetro2004("AppRetro2004"),
    AppRetro2017("AppRetro2017"),
    AppPixelated("AppPixelated"),
    AppMinimal("AppMinimal"),
    AppPride("AppPride"),
    AppCute("AppCute"),
    AppMomo("AppMomo"),
    ;

    /**
     * [AliasSuffix] helper object
     */
    companion object {
        /**
         * Returns the [AliasSuffix] associated with the given string.
         *
         * @param aliasSuffix The suffix from android:name in the manifest (e.g. "AppSolidLight").
         */
        fun fromString(aliasSuffix: String): AliasSuffix =
            entries.find { it.suffix == aliasSuffix } ?: AppDefault
    }
}
