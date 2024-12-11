/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.annotation

import android.content.res.Configuration
import androidx.compose.ui.tooling.preview.Devices
import androidx.compose.ui.tooling.preview.Preview
import org.mozilla.fenix.theme.AcornWindowSize

private const val SMALL_WINDOW_WIDTH = 400
private const val MEDIUM_WINDOW_WIDTH = 700
private const val LARGE_WINDOW_WIDTH = 1000

/**
 * A wrapper annotation for creating a preview that renders a preview for each
 * combination of [AcornWindowSize] and Light/Dark theme.
 */
// The device parameter is needed in order to force the `LocalConfiguration.current.screenWidth`
// to work properly. See: https://issuetracker.google.com/issues/300116108#comment1
@Preview(
    name = "Small Window Light",
    widthDp = SMALL_WINDOW_WIDTH,
    uiMode = Configuration.UI_MODE_NIGHT_NO,
)
@Preview(
    name = "Small Window Dark",
    widthDp = SMALL_WINDOW_WIDTH,
    uiMode = Configuration.UI_MODE_NIGHT_YES,
)
@Preview(
    name = "Medium Window Light",
    widthDp = MEDIUM_WINDOW_WIDTH,
    device = Devices.NEXUS_7,
    uiMode = Configuration.UI_MODE_NIGHT_NO,
)
@Preview(
    name = "Medium Window Dark",
    widthDp = MEDIUM_WINDOW_WIDTH,
    device = Devices.NEXUS_7,
    uiMode = Configuration.UI_MODE_NIGHT_YES,
)
@Preview(
    name = "Large Window Light",
    widthDp = LARGE_WINDOW_WIDTH,
    device = Devices.AUTOMOTIVE_1024p,
    uiMode = Configuration.UI_MODE_NIGHT_NO,
)
@Preview(
    name = "Large Window Dark",
    widthDp = LARGE_WINDOW_WIDTH,
    device = Devices.AUTOMOTIVE_1024p,
    uiMode = Configuration.UI_MODE_NIGHT_YES,
)
annotation class FlexibleWindowLightDarkPreview
