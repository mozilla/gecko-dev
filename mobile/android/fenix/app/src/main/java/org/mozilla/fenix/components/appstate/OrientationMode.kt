/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import android.content.res.Configuration
import java.security.InvalidParameterException

/**
 * Enum that represents orientation state the device is in.
 */
enum class OrientationMode {
    Undefined, Portrait, Landscape;

    companion object {

        /**
         * Convert a system [Configuration.orientation] into a [OrientationMode].
         */
        fun fromInteger(orientation: Int): OrientationMode {
            return when (orientation) {
                Configuration.ORIENTATION_UNDEFINED -> Undefined
                Configuration.ORIENTATION_PORTRAIT -> Portrait
                Configuration.ORIENTATION_LANDSCAPE -> Landscape
                else -> throw throw InvalidParameterException(
                    "The value provided doesn't match system Configuration constants",
                )
            }
        }
    }
}
