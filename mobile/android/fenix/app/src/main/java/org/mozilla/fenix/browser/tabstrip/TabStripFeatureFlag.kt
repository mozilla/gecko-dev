/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import android.content.Context
import mozilla.components.support.ktx.android.content.doesDeviceHaveHinge
import org.mozilla.fenix.Config
import org.mozilla.fenix.ReleaseChannel
import org.mozilla.fenix.utils.isLargeScreenSize

/**
 * Returns true if the tab strip is enabled.
 */
fun Context.isTabStripEnabled(): Boolean =
    Config.channel.isTabStripChannel() && isTabStripEligible()

/**
 * Returns true for channels that have the tab strip enabled.
 */
private fun ReleaseChannel.isTabStripChannel(): Boolean =
    isBeta || isNightlyOrDebug

/**
 * Returns true if the the device has the prerequisites to enable the tab strip.
 */
private fun Context.isTabStripEligible(): Boolean =
    // Tab Strip is currently disabled on foldable devices, while we work on improving the
    // Homescreen / Toolbar / Browser screen to better support the feature. There is also
    // an emulator bug that causes the doesDeviceHaveHinge check to return true on emulators,
    // causing it to be disabled on emulator tablets for API 34 and below.
    // https://issuetracker.google.com/issues/296162661
    isLargeScreenSize() && !doesDeviceHaveHinge()
