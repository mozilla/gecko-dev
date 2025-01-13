/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.ext

import org.mozilla.fenix.utils.Settings

private const val MIN_NUMBER_OF_APP_LAUNCHES = 3

/**
 * Try to show the wallpaper onboarding dialog on the third opening of the app.
 *
 * Note: We use 'at least three' instead of exactly 'three' in case the app is opened in such a
 * way that the other conditions are not met.
 */
internal fun Settings.showWallpaperOnboardingDialog(featureRecommended: Boolean) =
    numberOfAppLaunches >= MIN_NUMBER_OF_APP_LAUNCHES && showWallpaperOnboarding && !featureRecommended
