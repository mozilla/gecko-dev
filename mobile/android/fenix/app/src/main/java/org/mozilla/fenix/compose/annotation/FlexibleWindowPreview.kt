/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.annotation

import androidx.compose.ui.tooling.preview.Devices
import androidx.compose.ui.tooling.preview.Preview
import mozilla.components.compose.base.theme.AcornWindowSize

/**
 * A wrapper annotation for creating a preview that renders a preview for each value of [AcornWindowSize].
 */
// The device parameter is needed in order to force the `LocalConfiguration.current.screenWidth`
// to work properly. See: https://issuetracker.google.com/issues/300116108#comment1
@Preview(
    name = "Small Window",
    widthDp = 400,
)
@Preview(
    name = "Medium Window",
    widthDp = 700,
    device = Devices.NEXUS_7,
)
@Preview(
    name = "Large Window",
    widthDp = 1000,
    device = Devices.AUTOMOTIVE_1024p,
)
annotation class FlexibleWindowPreview
