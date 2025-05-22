/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.automotive

import android.content.Context

/**
 * Checks if the android automotive hardware feature is present.
 * Note: Android Automotive is NOT the same as Android Auto.
 */
fun Context.isAndroidAutomotiveAvailable() =
    packageManager.hasSystemFeature("android.hardware.type.automotive")
